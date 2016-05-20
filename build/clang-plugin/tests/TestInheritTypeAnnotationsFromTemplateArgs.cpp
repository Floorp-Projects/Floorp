#define MOZ_INHERIT_TYPE_ANNOTATIONS_FROM_TEMPLATE_ARGS                 \
  __attribute__((annotate("moz_inherit_type_annotations_from_template_args")))
#define MOZ_STACK_CLASS __attribute__((annotate("moz_stack_class")))
#define MOZ_NON_MEMMOVABLE __attribute__((annotate("moz_non_memmovable")))
#define MOZ_NEEDS_MEMMOVABLE_TYPE __attribute__((annotate("moz_needs_memmovable_type")))

class Normal {};
class MOZ_STACK_CLASS Stack {};
class IndirectStack : Stack {}; // expected-note {{'IndirectStack' is a stack type because it inherits from a stack type 'Stack'}}
class ContainsStack { Stack m; }; // expected-note {{'ContainsStack' is a stack type because member 'm' is a stack type 'Stack'}}
class MOZ_NON_MEMMOVABLE Pointery {};
class IndirectPointery : Pointery {}; // expected-note {{'IndirectPointery' is a non-memmove()able type because it inherits from a non-memmove()able type 'Pointery'}}
class ContainsPointery { Pointery m; }; // expected-note {{'ContainsPointery' is a non-memmove()able type because member 'm' is a non-memmove()able type 'Pointery'}}

template<class T>
class MOZ_INHERIT_TYPE_ANNOTATIONS_FROM_TEMPLATE_ARGS Template {}; // expected-note-re 5 {{'Template<{{.*}}>' is a stack type because it has a template argument stack type '{{.*}}'}} expected-note-re 5 {{'Template<{{.*}}>' is a non-memmove()able type because it has a template argument non-memmove()able type '{{.*}}'}}
class IndirectTemplate : Template<Stack> {}; // expected-note {{'IndirectTemplate' is a stack type because it inherits from a stack type 'Template<Stack>'}}
class ContainsTemplate { Template<Stack> m; }; // expected-note {{'ContainsTemplate' is a stack type because member 'm' is a stack type 'Template<Stack>'}}

static Template<Stack> a; // expected-error {{variable of type 'Template<Stack>' only valid on the stack}} expected-note {{value incorrectly allocated in a global variable}}
static Template<IndirectStack> b; // expected-error {{variable of type 'Template<IndirectStack>' only valid on the stack}} expected-note {{value incorrectly allocated in a global variable}}
static Template<ContainsStack> c; // expected-error {{variable of type 'Template<ContainsStack>' only valid on the stack}} expected-note {{value incorrectly allocated in a global variable}}
static IndirectTemplate d; // expected-error {{variable of type 'IndirectTemplate' only valid on the stack}} expected-note {{value incorrectly allocated in a global variable}}
static ContainsTemplate e; // expected-error {{variable of type 'ContainsTemplate' only valid on the stack}} expected-note {{value incorrectly allocated in a global variable}}
static Template<Normal> f;

template<class T>
class MOZ_NEEDS_MEMMOVABLE_TYPE Mover { // expected-error-re 8 {{Cannot instantiate 'Mover<{{.*}}>' with non-memmovable template argument '{{.*}}'}}
  char mForceInstantiation[sizeof(T)];
};
class IndirectTemplatePointery : Template<Pointery> {}; // expected-note {{'IndirectTemplatePointery' is a non-memmove()able type because it inherits from a non-memmove()able type 'Template<Pointery>'}}
class ContainsTemplatePointery { Template<Pointery> m; }; // expected-note {{'ContainsTemplatePointery' is a non-memmove()able type because member 'm' is a non-memmove()able type 'Template<Pointery>'}}

static Mover<Template<Pointery>> n; // expected-note {{instantiation of 'Mover<Template<Pointery> >' requested here}}
static Mover<Template<IndirectPointery>> o; // expected-note {{instantiation of 'Mover<Template<IndirectPointery> >' requested here}}
static Mover<Template<ContainsPointery>> p; // expected-note {{instantiation of 'Mover<Template<ContainsPointery> >' requested here}}
static Mover<IndirectTemplatePointery> q; // expected-note {{instantiation of 'Mover<IndirectTemplatePointery>' requested here}}
static Mover<ContainsTemplatePointery> r; // expected-note {{instantiation of 'Mover<ContainsTemplatePointery>' requested here}}
static Mover<Template<Normal>> s;

template<class T, class... Ts>
class MOZ_INHERIT_TYPE_ANNOTATIONS_FROM_TEMPLATE_ARGS ManyTs {}; // expected-note-re 3 {{'ManyTs<{{.*}}>' is a non-memmove()able type because it has a template argument non-memmove()able type '{{.*}}'}}

static Mover<ManyTs<Pointery>> t; // expected-note {{instantiation of 'Mover<ManyTs<Pointery> >' requested here}}
static Mover<ManyTs<Normal, Pointery>> u; // expected-note {{instantiation of 'Mover<ManyTs<Normal, Pointery> >' requested here}}
static Mover<ManyTs<Normal, Normal, Pointery>> v; // expected-note {{instantiation of 'Mover<ManyTs<Normal, Normal, Pointery> >' requested here}}
