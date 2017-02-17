#define MOZ_NEEDS_MEMMOVABLE_TYPE __attribute__((annotate("moz_needs_memmovable_type")))

template<class T>
class MOZ_NEEDS_MEMMOVABLE_TYPE Mover { T mForceInst; }; // expected-error-re 4 {{Cannot instantiate 'Mover<{{.*}}>' with non-memmovable template argument '{{.*}}'}}

namespace std {
// In theory defining things in std:: like this invokes undefined
// behavior, but in practice it's good enough for this test case.
template<class C> class basic_string { }; // expected-note 2 {{'std::basic_string<char>' is a non-memmove()able type because it is an stl-provided type not guaranteed to be memmove-able}} expected-note {{'std::string' (aka 'basic_string<char>') is a non-memmove()able type because it is an stl-provided type not guaranteed to be memmove-able}}
typedef basic_string<char> string;
template<class T, class U> class pair { T mT; U mU; }; // expected-note {{std::pair<bool, std::basic_string<char> >' is a non-memmove()able type because member 'mU' is a non-memmove()able type 'std::basic_string<char>'}}
class arbitrary_name { }; // expected-note {{'std::arbitrary_name' is a non-memmove()able type because it is an stl-provided type not guaranteed to be memmove-able}}
}

class HasString { std::string m; }; // expected-note {{'HasString' is a non-memmove()able type because member 'm' is a non-memmove()able type 'std::string' (aka 'basic_string<char>')}}

static Mover<std::string> bad; // expected-note {{instantiation of 'Mover<std::basic_string<char> >' requested here}}
static Mover<HasString> bad_mem; // expected-note {{instantiation of 'Mover<HasString>' requested here}}
static Mover<std::arbitrary_name> assumed_bad; // expected-note {{instantiation of 'Mover<std::arbitrary_name>' requested here}}
static Mover<std::pair<bool, int>> good;
static Mover<std::pair<bool, std::string>> not_good; // expected-note {{instantiation of 'Mover<std::pair<bool, std::basic_string<char> > >' requested here}}
