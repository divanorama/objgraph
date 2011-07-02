#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <set>
#include <map>

std::string obj;
std::string line;

struct TObj;
typedef std::pair<const TObj*, std::string> TRef;
struct ls_ref {
	bool operator()(const TRef& a, const TRef& b) const {
		if (a.first != b.first) {
			return a.first < b.first;
		}
//		return a.second < b.second;
		return false;
//		return !a.first && a.second < b.second;
	}
};
typedef std::set<TRef, ls_ref> TRefs;

typedef std::pair<std::string, bool> TDef;

struct TObj {
	std::string name;
	std::string node_name;
	mutable std::set<TDef> exports;
	mutable std::set<std::string> imports;
	mutable TRefs refs;
	TObj (const std::string& name) : name (name) {
		for (size_t i = 0; i != name.size (); ++i) {
			if (name[i] >= 'a' && name[i] <= 'z'
			|| name[i] >= 'A' && name[i] <= 'Z'
			|| name[i] >= '0' && name[i] <= '9'
			|| name[i] == '_') {
				node_name += name[i];
			} else {
				node_name += '_';
			}
		}
	}
	bool operator<(const TObj& other) const {
		return name < other.name;
	}
};

typedef std::set<TObj> TObjects;
TObjects Objects;

typedef std::map<std::string, const TObj*> TResolve;
TResolve Resolve;

inline void read_line () {
	char buf[1024];
	if (!fgets (buf, 1024, stdin)) {
		line = "";
		return;
	}
	line = buf;
	if (line.size () && line[line.size () - 1] == '\n') {
		line.resize (line.size () - 1);
	}
}

void parse_sym (const TObj& o, const char* kind, const char* name) {
	if (kind[1]) {
		fprintf (stderr, "Error: symbol kind is too long (%s)\n", kind);
		exit (1);
	}
	if (kind[0] >= 'a' && kind[0] <= 'z') {
		return;
	}
	bool weak = kind[0] == 'W' || kind[0] == 'A' || kind[0] == 'T';
	if (kind[0] == 'U') {
		o.imports.insert (name);
	} else {
		o.exports.insert (TDef (name, weak));
		std::pair<TResolve::iterator, bool> ins = Resolve.insert (TResolve::value_type (name, &o));
		if (ins.second || weak || ins.first->second->name == o.name) {
			return;
		}
		if (ins.first->second->exports.find (TDef (name, false)) != ins.first->second->exports.end ()) {
			fprintf (stderr, "Error: strong symbol %s is exported twice. By %s and %s\n", name, ins.first->second->name.c_str (), o.name.c_str ());
			exit (1);
		}
		ins.second = &o;
	}
}

void parse_obj () {
	std::pair<TObjects::iterator, bool> ins = Objects.insert (obj);
	if (!ins.second) {
		fprintf (stderr, "Error: duplicated object %s\n", obj.c_str ());
		exit (1);
	}
	const TObj& o = *ins.first;
	char tokens[4][1024];
	while (!feof (stdin)) {
		read_line ();
		if (line.length () == 0) {
			return;
		}
		switch (sscanf (line.c_str (), "%s%s%s%s", tokens[0], tokens[1], tokens[2], tokens[3])) {
		case 2:
			parse_sym (o, tokens[0], tokens[1]);
			break;
		case 3:
			parse_sym (o, tokens[1], tokens[2]);
			break;
		default:
			fprintf (stderr, "Error: can't parse line %s\n", line.c_str ());
			exit (1);
		}
	}
}

void get_refs () {
	for (TObjects::const_iterator o = Objects.begin (); o != Objects.end (); ++o)
	for (std::set<std::string>::const_iterator i = o->imports.begin (); i != o->imports.end (); ++i) {
		TResolve::const_iterator r = Resolve.find (*i);
		const TObj* re = r == Resolve.end () ? NULL : r->second;
		o->refs.insert (TRef (re, *i));
	}
}

std::set<const TObj*> done;

inline void dfs (const TObj* o) {
	if (done.insert (o).second)
	for (TRefs::const_iterator r = o->refs.begin (); r != o->refs.end (); ++r)
	if (r->first)
		dfs (r->first);
}

void dfs (const std::string& s) {
	for (TObjects::const_iterator o = Objects.begin (); o != Objects.end (); ++o)
	if (s.empty () || o->name == s)
		dfs (&*o);
}

void out_graph (const std::string& s) {
	dfs (s);
	printf ("digraph {\n");
	for (TObjects::const_iterator o = Objects.begin (); o != Objects.end (); ++o)
	if (done.find (&*o) != done.end ()) {
		printf ("\t%s [label=\"%s\"]\n", o->node_name.c_str (), o->name.c_str ());
	}
	for (TObjects::const_iterator o = Objects.begin (); o != Objects.end (); ++o)
	if (done.find (&*o) != done.end ())
	for (TRefs::const_iterator r = o->refs.begin (); r != o->refs.end (); ++r) {
		const char* from = o->node_name.c_str ();
		const char* to = r->first ? r->first->node_name.c_str () : "EXTLIB";
		const char* example = r->second.c_str ();
		printf ("\t%s -> %s [label=\"%s\"]\n", from, to, example);
	}
	printf ("}\n");
}

int main (int argc, char** argv) {
	--argc, ++argv;
	if (argc > 1) {
		fprintf (stderr, "Usage: nm -g | objgraph [object-name.o]\n");
		return 1;
	}
	const std::string out_obj = argc ? argv[0] : "";
	char buf[1024];
	while (!feof (stdin)) {
		read_line ();
		if (line.length () == 0) {
			continue;
		}
		obj = line;
		obj.resize (obj.size () - 1);
		parse_obj ();		
	}
	get_refs ();
	out_graph (out_obj);
	return 0;
}
