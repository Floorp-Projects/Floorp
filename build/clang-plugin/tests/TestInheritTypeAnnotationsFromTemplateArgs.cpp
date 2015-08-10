#define MOZ_INHERIT_TYPE_ANNOTATIONS_FROM_TEMPLATE_ARGS                 \
  __attribute__((annotate("moz_inherit_type_annotations_from_template_args")))
#define MOZ_STACK_CLASS __attribute__((annotate("moz_stack_class")))

class Normal {};
class MOZ_STACK_CLASS Stack {};
class IndirectStack : Stack {}; // expected-note {{'IndirectStack' is a stack type because it inherits from a stack type 'Stack'}}

template<class T>
class MOZ_INHERIT_TYPE_ANNOTATIONS_FROM_TEMPLATE_ARGS Template {}; // expected-note 2 {{'Template<Stack>' is a stack type because it has a template argument stack type 'Stack'}} expected-note {{'Template<IndirectStack>' is a stack type because it has a template argument stack type 'IndirectStack'}}
class IndirectTemplate : Template<Stack> {}; // expected-note {{'IndirectTemplate' is a stack type because it inherits from a stack type 'Template<Stack>'}}

static Template<Stack> a; // expected-error {{variable of type 'Template<Stack>' only valid on the stack}} expected-note {{value incorrectly allocated in a global variable}}
static Template<IndirectStack> b; // expected-error {{variable of type 'Template<IndirectStack>' only valid on the stack}} expected-note {{value incorrectly allocated in a global variable}}
static Template<Normal> c;
static IndirectTemplate d; // expected-error {{variable of type 'IndirectTemplate' only valid on the stack}} expected-note {{value incorrectly allocated in a global variable}}

