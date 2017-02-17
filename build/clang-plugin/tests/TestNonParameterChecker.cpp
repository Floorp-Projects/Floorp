#define MOZ_NON_PARAM __attribute__((annotate("moz_non_param")))

struct Param {};
struct MOZ_NON_PARAM NonParam {};
union MOZ_NON_PARAM NonParamUnion {};
class MOZ_NON_PARAM NonParamClass {};
enum MOZ_NON_PARAM NonParamEnum { X, Y, Z };
enum class MOZ_NON_PARAM NonParamEnumClass { X, Y, Z };

struct HasNonParamStruct { NonParam x; int y; }; // expected-note 14 {{'HasNonParamStruct' is a non-param type because member 'x' is a non-param type 'NonParam'}}
union HasNonParamUnion { NonParam x; int y; }; // expected-note 18 {{'HasNonParamUnion' is a non-param type because member 'x' is a non-param type 'NonParam'}}
struct HasNonParamStructUnion { HasNonParamUnion z; }; // expected-note 9 {{'HasNonParamStructUnion' is a non-param type because member 'z' is a non-param type 'HasNonParamUnion'}}

#define MAYBE_STATIC
#include "NonParameterTestCases.h"
#undef MAYBE_STATIC

// Do not check typedef and using.
typedef void (*funcTypeParam)(Param x);
typedef void (*funcTypeNonParam)(NonParam x);

using usingFuncTypeParam = void (*)(Param x);
using usingFuncTypeNonParam = void (*)(NonParam x);

class class_
{
    explicit class_(Param x) {}
    explicit class_(NonParam x) {} //expected-error {{Type 'NonParam' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
    explicit class_(HasNonParamStruct x) {} //expected-error {{Type 'HasNonParamStruct' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
    explicit class_(HasNonParamUnion x) {} //expected-error {{Type 'HasNonParamUnion' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
    explicit class_(HasNonParamStructUnion x) {} //expected-error {{Type 'HasNonParamStructUnion' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}

#define MAYBE_STATIC
#include "NonParameterTestCases.h"
#undef MAYBE_STATIC
};

class classWithStatic
{
#define MAYBE_STATIC static
#include "NonParameterTestCases.h"
#undef MAYBE_STATIC
};

template <typename T>
class tmplClassForParam
{
public:
    void raw(T x) {}
    void rawDefault(T x = T()) {}
    void const_(const T x) {}
    void ptr(T* x) {}
    void ref(T& x) {}
    void constRef(const T& x) {}

    void notCalled(T x) {}
};

template <typename T>
class tmplClassForNonParam
{
public:
    void raw(T x) {} //expected-error {{Type 'NonParam' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
    void rawDefault(T x = T()) {} //expected-error {{Type 'NonParam' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
    void const_(const T x) {} //expected-error {{Type 'NonParam' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
    void ptr(T* x) {}
    void ref(T& x) {}
    void constRef(const T& x) {}

    void notCalled(T x) {}
};

template <typename T>
class tmplClassForHasNonParamStruct
{
public:
    void raw(T x) {} //expected-error {{Type 'HasNonParamStruct' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
    void rawDefault(T x = T()) {} //expected-error {{Type 'HasNonParamStruct' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
    void const_(const T x) {} //expected-error {{Type 'HasNonParamStruct' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
    void ptr(T* x) {}
    void ref(T& x) {}
    void constRef(const T& x) {}

    void notCalled(T x) {}
};

void testTemplateClass()
{
    tmplClassForParam<Param> paramClass;
    Param param;
    paramClass.raw(param);
    paramClass.rawDefault();
    paramClass.const_(param);
    paramClass.ptr(&param);
    paramClass.ref(param);
    paramClass.constRef(param);

    tmplClassForNonParam<NonParam> nonParamClass; //expected-note 3 {{The bad argument was passed to 'tmplClassForNonParam' here}}
    NonParam nonParam;
    nonParamClass.raw(nonParam);
    nonParamClass.rawDefault();
    nonParamClass.const_(nonParam);
    nonParamClass.ptr(&nonParam);
    nonParamClass.ref(nonParam);
    nonParamClass.constRef(nonParam);

    tmplClassForHasNonParamStruct<HasNonParamStruct> hasNonParamStructClass;//expected-note 3 {{The bad argument was passed to 'tmplClassForHasNonParamStruct' here}}
    HasNonParamStruct hasNonParamStruct;
    hasNonParamStructClass.raw(hasNonParamStruct);
    hasNonParamStructClass.rawDefault();
    hasNonParamStructClass.const_(hasNonParamStruct);
    hasNonParamStructClass.ptr(&hasNonParamStruct);
    hasNonParamStructClass.ref(hasNonParamStruct);
    hasNonParamStructClass.constRef(hasNonParamStruct);
}

template <typename T>
class NestedTemplateInner
{
public:
    void raw(T x) {} //expected-error {{Type 'NonParam' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
};

template <typename T>
class nestedTemplateOuter
{
public:
    void constRef(const T& x) {
        NestedTemplateInner<T> inner; //expected-note {{The bad argument was passed to 'NestedTemplateInner' here}}
        inner.raw(x);
    }
};

void testNestedTemplateClass()
{
    nestedTemplateOuter<NonParam> outer;
    NonParam nonParam;
    outer.constRef(nonParam); // FIXME: this line needs note "The bad argument was passed to 'constRef' here"
}

template <typename T>
void tmplFuncForParam(T x) {}
template <typename T>
void tmplFuncForNonParam(T x) {} //expected-error {{Type 'NonParam' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
template <typename T>
void tmplFuncForNonParamImplicit(T x) {} //expected-error {{Type 'NonParam' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
template <typename T>
void tmplFuncForHasNonParamStruct(T x) {} //expected-error {{Type 'HasNonParamStruct' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
template <typename T>
void tmplFuncForHasNonParamStructImplicit(T x) {} //expected-error {{Type 'HasNonParamStruct' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}

void testTemplateFunc()
{
    Param param;
    tmplFuncForParam<Param>(param);

    NonParam nonParam;
    tmplFuncForNonParam<NonParam>(nonParam); // FIXME: this line needs note "The bad argument was passed to 'tmplFuncForNonParam' here"
    tmplFuncForNonParamImplicit(nonParam); // FIXME: this line needs note "The bad argument was passed to 'tmplFuncForNonParamImplicit' here"

    HasNonParamStruct hasNonParamStruct;
    tmplFuncForHasNonParamStruct<HasNonParamStruct>(hasNonParamStruct); // FIXME: this line needs note "The bad argument was passed to 'tmplFuncForHasNonParamStruct' here"
    tmplFuncForHasNonParamStructImplicit(hasNonParamStruct); // FIXME: this line needs note "The bad argument was passed to 'tmplFuncForHasNonParamStructImplicit' here"
}

void testLambda()
{
    auto paramLambda = [](Param x) -> void {};
    auto nonParamLambda = [](NonParam x) -> void {}; //expected-error {{Type 'NonParam' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
    auto nonParamStructLambda = [](HasNonParamStruct x) -> void {}; //expected-error {{Type 'HasNonParamStruct' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
    auto nonParamUnionLambda = [](HasNonParamUnion x) -> void {}; //expected-error {{Type 'HasNonParamUnion' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
    auto nonParamStructUnionLambda = [](HasNonParamStructUnion x) -> void {}; //expected-error {{Type 'HasNonParamStructUnion' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}

    (void)[](Param x) -> void {};
    (void)[](NonParam x) -> void {}; //expected-error {{Type 'NonParam' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
    (void)[](HasNonParamStruct x) -> void {}; //expected-error {{Type 'HasNonParamStruct' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
    (void)[](HasNonParamUnion x) -> void {}; //expected-error {{Type 'HasNonParamUnion' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
    (void)[](HasNonParamStructUnion x) -> void {}; //expected-error {{Type 'HasNonParamStructUnion' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
}

// Check that alignas() implies the MOZ_NON_PARAM attribute.

struct alignas(8) AlignasStruct { char a; }; // expected-note {{'AlignasStruct' is a non-param type because it has an alignas(_) annotation}}
void takesAlignasStruct(AlignasStruct x) { } // expected-error {{Type 'AlignasStruct' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
void takesAlignasStructByRef(const AlignasStruct& x) { }

struct AlignasMember { alignas(8) char a; }; // expected-note {{'AlignasMember' is a non-param type because member 'a' has an alignas(_) annotation}}
void takesAlignasMember(AlignasMember x) { } // expected-error {{Type 'AlignasMember' must not be used as parameter}} expected-note {{Please consider passing a const reference instead}}
void takesAlignasMemberByRef(const AlignasMember& x) { }
