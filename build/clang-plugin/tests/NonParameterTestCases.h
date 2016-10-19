MAYBE_STATIC void raw(Param x) {}

MAYBE_STATIC void raw(NonParam x) {} //expected-error {{Type 'NonParam' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC void raw(NonParamUnion x) {} //expected-error {{Type 'NonParamUnion' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC void raw(NonParamClass x) {} //expected-error {{Type 'NonParamClass' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC void raw(NonParamEnum x) {} //expected-error {{Type 'NonParamEnum' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC void raw(NonParamEnumClass x) {} //expected-error {{Type 'NonParamEnumClass' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC void raw(HasNonParamStruct x) {} //expected-error {{Type 'HasNonParamStruct' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC void raw(HasNonParamUnion x) {} //expected-error {{Type 'HasNonParamUnion' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC void raw(HasNonParamStructUnion x) {} //expected-error {{Type 'HasNonParamStructUnion' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}

MAYBE_STATIC void const_(const NonParam x) {} //expected-error {{Type 'NonParam' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC void const_(const NonParamUnion x) {} //expected-error {{Type 'NonParamUnion' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC void const_(const NonParamClass x) {} //expected-error {{Type 'NonParamClass' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC void const_(const NonParamEnum x) {} //expected-error {{Type 'NonParamEnum' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC void const_(const NonParamEnumClass x) {} //expected-error {{Type 'NonParamEnumClass' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC void const_(const HasNonParamStruct x) {} //expected-error {{Type 'HasNonParamStruct' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC void const_(const HasNonParamUnion x) {} //expected-error {{Type 'HasNonParamUnion' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC void const_(const HasNonParamStructUnion x) {} //expected-error {{Type 'HasNonParamStructUnion' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}

MAYBE_STATIC void array(NonParam x[]) {}
MAYBE_STATIC void array(NonParamUnion x[]) {}
MAYBE_STATIC void array(NonParamClass x[]) {}
MAYBE_STATIC void array(NonParamEnum x[]) {}
MAYBE_STATIC void array(NonParamEnumClass x[]) {}
MAYBE_STATIC void array(HasNonParamStruct x[]) {}
MAYBE_STATIC void array(HasNonParamUnion x[]) {}
MAYBE_STATIC void array(HasNonParamStructUnion x[]) {}

MAYBE_STATIC void ptr(NonParam* x) {}
MAYBE_STATIC void ptr(NonParamUnion* x) {}
MAYBE_STATIC void ptr(NonParamClass* x) {}
MAYBE_STATIC void ptr(NonParamEnum* x) {}
MAYBE_STATIC void ptr(NonParamEnumClass* x) {}
MAYBE_STATIC void ptr(HasNonParamStruct* x) {}
MAYBE_STATIC void ptr(HasNonParamUnion* x) {}
MAYBE_STATIC void ptr(HasNonParamStructUnion* x) {}

MAYBE_STATIC void ref(NonParam& x) {}
MAYBE_STATIC void ref(NonParamUnion& x) {}
MAYBE_STATIC void ref(NonParamClass& x) {}
MAYBE_STATIC void ref(NonParamEnum& x) {}
MAYBE_STATIC void ref(NonParamEnumClass& x) {}
MAYBE_STATIC void ref(HasNonParamStruct& x) {}
MAYBE_STATIC void ref(HasNonParamUnion& x) {}
MAYBE_STATIC void ref(HasNonParamStructUnion& x) {}

MAYBE_STATIC void constRef(const NonParam& x) {}
MAYBE_STATIC void constRef(const NonParamUnion& x) {}
MAYBE_STATIC void constRef(const NonParamClass& x) {}
MAYBE_STATIC void constRef(const NonParamEnum& x) {}
MAYBE_STATIC void constRef(const NonParamEnumClass& x) {}
MAYBE_STATIC void constRef(const HasNonParamStruct& x) {}
MAYBE_STATIC void constRef(const HasNonParamUnion& x) {}
MAYBE_STATIC void constRef(const HasNonParamStructUnion& x) {}

MAYBE_STATIC inline void inlineRaw(NonParam x) {} //expected-error {{Type 'NonParam' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC inline void inlineRaw(NonParamUnion x) {} //expected-error {{Type 'NonParamUnion' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC inline void inlineRaw(NonParamClass x) {} //expected-error {{Type 'NonParamClass' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC inline void inlineRaw(NonParamEnum x) {} //expected-error {{Type 'NonParamEnum' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
MAYBE_STATIC inline void inlineRaw(NonParamEnumClass x) {} //expected-error {{Type 'NonParamEnumClass' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
