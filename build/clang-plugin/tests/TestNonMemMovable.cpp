#define MOZ_NON_MEMMOVABLE __attribute__((annotate("moz_non_memmovable")))
#define MOZ_NEEDS_MEMMOVABLE_TYPE __attribute__((annotate("moz_needs_memmovable_type")))
#define MOZ_NEEDS_MEMMOVABLE_MEMBERS __attribute__((annotate("moz_needs_memmovable_members")))

/*
  These are a bunch of structs with variable levels of memmovability.
  They will be used as template parameters to the various NeedyTemplates
*/
struct MOZ_NON_MEMMOVABLE NonMovable {};
struct Movable {};

// Subclasses
struct S_NonMovable : NonMovable {}; // expected-note 51 {{'S_NonMovable' is a non-memmove()able type because it inherits from a non-memmove()able type 'NonMovable'}}
struct S_Movable : Movable {};

// Members
struct W_NonMovable {
  NonMovable m; // expected-note 34 {{'W_NonMovable' is a non-memmove()able type because member 'm' is a non-memmove()able type 'NonMovable'}}
};
struct W_Movable {
  Movable m;
};

// Wrapped Subclasses
struct WS_NonMovable {
  S_NonMovable m; // expected-note 34 {{'WS_NonMovable' is a non-memmove()able type because member 'm' is a non-memmove()able type 'S_NonMovable'}}
};
struct WS_Movable {
  S_Movable m;
};

// Combinations of the above
struct SW_NonMovable : W_NonMovable {}; // expected-note 17 {{'SW_NonMovable' is a non-memmove()able type because it inherits from a non-memmove()able type 'W_NonMovable'}}
struct SW_Movable : W_Movable {};

struct SWS_NonMovable : WS_NonMovable {}; // expected-note 17 {{'SWS_NonMovable' is a non-memmove()able type because it inherits from a non-memmove()able type 'WS_NonMovable'}}
struct SWS_Movable : WS_Movable {};

// Basic templated wrapper
template <class T>
struct Template_Inline {
  T m; // expected-note-re 56 {{'Template_Inline<{{.*}}>' is a non-memmove()able type because member 'm' is a non-memmove()able type '{{.*}}'}}
};

template <class T>
struct Template_Ref {
  T* m;
};

template <class T>
struct Template_Unused {};

template <class T>
struct MOZ_NON_MEMMOVABLE Template_NonMovable {};

/*
  These tests take the following form:
  DECLARATIONS => Declarations of the templates which are either marked with MOZ_NEEDS_MEMMOVABLE_TYPE
                  or which instantiate a MOZ_NEEDS_MEMMOVABLE_TYPE through some mechanism.
  BAD N        => Instantiations of the wrapper template with each of the non-memmovable types.
                  The prefix S_ means subclass, W_ means wrapped. Each of these rows should produce an error
                  on the NeedyTemplate in question, and a note at the instantiation location of that template.
                  Unfortunately, on every case more complicated than bad1, the instantiation location is
                  within another template. Thus, the notes are expected on the template in question which
                  actually instantiates the MOZ_NEEDS_MEMMOVABLE_TYPE template.
  GOOD N       => Instantiations of the wrapper template with each of the memmovable types.
                  This is meant as a sanity check to ensure that we don't reject valid instantiations of
                  templates.


  Note 1: Each set uses it's own types to ensure that they don't re-use each-other's template specializations.
  If they did, then some of the error messages would not be emitted (as error messages are emitted for template
  specializations, rather than for variable declarations)

  Note 2: Every instance of NeedyTemplate contains a member of type T. This is to ensure that T is actually
  instantiated (if T is a template) by clang. If T isn't instantiated, then we can't actually tell if it is
  NON_MEMMOVABLE. (This is OK in practice, as you cannot memmove a type which you don't know the size of).

  Note 3: There are a set of tests for specializations of NeedyTemplate at the bottom. For each set of tests,
  these tests contribute two expected errors to the templates.
*/

//
// 1 - Unwrapped MOZ_NEEDS_MEMMOVABLE_TYPE
//

template <class T>
struct MOZ_NEEDS_MEMMOVABLE_TYPE NeedyTemplate1 {T m;}; // expected-error-re 26 {{Cannot instantiate 'NeedyTemplate1<{{.*}}>' with non-memmovable template argument '{{.*}}'}}

void bad1() {
  NeedyTemplate1<NonMovable> a1; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<S_NonMovable> a2; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<W_NonMovable> a3; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<WS_NonMovable> a4; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<SW_NonMovable> a5; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<SWS_NonMovable> a6; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}

  NeedyTemplate1<Template_Inline<NonMovable> > b1; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<Template_Inline<S_NonMovable> > b2; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<Template_Inline<W_NonMovable> > b3; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<Template_Inline<WS_NonMovable> > b4; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<Template_Inline<SW_NonMovable> > b5; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<Template_Inline<SWS_NonMovable> > b6; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}

  NeedyTemplate1<Template_NonMovable<NonMovable> > c1; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<Template_NonMovable<S_NonMovable> > c2; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<Template_NonMovable<W_NonMovable> > c3; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<Template_NonMovable<WS_NonMovable> > c4; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<Template_NonMovable<SW_NonMovable> > c5; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<Template_NonMovable<SWS_NonMovable> > c6; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<Template_NonMovable<Movable> > c7; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<Template_NonMovable<S_Movable> > c8; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<Template_NonMovable<W_Movable> > c9; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<Template_NonMovable<WS_Movable> > c10; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<Template_NonMovable<SW_Movable> > c11; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
  NeedyTemplate1<Template_NonMovable<SWS_Movable> > c12; // expected-note-re {{instantiation of 'NeedyTemplate1<{{.*}}>' requested here}}
}

void good1() {
  NeedyTemplate1<Movable> a1;
  NeedyTemplate1<S_Movable> a2;
  NeedyTemplate1<W_Movable> a3;
  NeedyTemplate1<WS_Movable> a4;
  NeedyTemplate1<SW_Movable> a5;
  NeedyTemplate1<SWS_Movable> a6;

  NeedyTemplate1<Template_Inline<Movable> > b1;
  NeedyTemplate1<Template_Inline<S_Movable> > b2;
  NeedyTemplate1<Template_Inline<W_Movable> > b3;
  NeedyTemplate1<Template_Inline<WS_Movable> > b4;
  NeedyTemplate1<Template_Inline<SW_Movable> > b5;
  NeedyTemplate1<Template_Inline<SWS_Movable> > b6;

  NeedyTemplate1<Template_Unused<Movable> > c1;
  NeedyTemplate1<Template_Unused<S_Movable> > c2;
  NeedyTemplate1<Template_Unused<W_Movable> > c3;
  NeedyTemplate1<Template_Unused<WS_Movable> > c4;
  NeedyTemplate1<Template_Unused<SW_Movable> > c5;
  NeedyTemplate1<Template_Unused<SWS_Movable> > c6;
  NeedyTemplate1<Template_Unused<NonMovable> > c7;
  NeedyTemplate1<Template_Unused<S_NonMovable> > c8;
  NeedyTemplate1<Template_Unused<W_NonMovable> > c9;
  NeedyTemplate1<Template_Unused<WS_NonMovable> > c10;
  NeedyTemplate1<Template_Unused<SW_NonMovable> > c11;
  NeedyTemplate1<Template_Unused<SWS_NonMovable> > c12;

  NeedyTemplate1<Template_Ref<Movable> > d1;
  NeedyTemplate1<Template_Ref<S_Movable> > d2;
  NeedyTemplate1<Template_Ref<W_Movable> > d3;
  NeedyTemplate1<Template_Ref<WS_Movable> > d4;
  NeedyTemplate1<Template_Ref<SW_Movable> > d5;
  NeedyTemplate1<Template_Ref<SWS_Movable> > d6;
  NeedyTemplate1<Template_Ref<NonMovable> > d7;
  NeedyTemplate1<Template_Ref<S_NonMovable> > d8;
  NeedyTemplate1<Template_Ref<W_NonMovable> > d9;
  NeedyTemplate1<Template_Ref<WS_NonMovable> > d10;
  NeedyTemplate1<Template_Ref<SW_NonMovable> > d11;
  NeedyTemplate1<Template_Ref<SWS_NonMovable> > d12;
}

//
// 2 - Subclassed MOZ_NEEDS_MEMMOVABLE_TYPE
//

template <class T>
struct MOZ_NEEDS_MEMMOVABLE_TYPE NeedyTemplate2 {T m;}; // expected-error-re 26 {{Cannot instantiate 'NeedyTemplate2<{{.*}}>' with non-memmovable template argument '{{.*}}'}}
template <class T>
struct S_NeedyTemplate2 : NeedyTemplate2<T> {}; // expected-note-re 26 {{instantiation of 'NeedyTemplate2<{{.*}}>' requested here}}

void bad2() {
  S_NeedyTemplate2<NonMovable> a1;
  S_NeedyTemplate2<S_NonMovable> a2;
  S_NeedyTemplate2<W_NonMovable> a3;
  S_NeedyTemplate2<WS_NonMovable> a4;
  S_NeedyTemplate2<SW_NonMovable> a5;
  S_NeedyTemplate2<SWS_NonMovable> a6;

  S_NeedyTemplate2<Template_Inline<NonMovable> > b1;
  S_NeedyTemplate2<Template_Inline<S_NonMovable> > b2;
  S_NeedyTemplate2<Template_Inline<W_NonMovable> > b3;
  S_NeedyTemplate2<Template_Inline<WS_NonMovable> > b4;
  S_NeedyTemplate2<Template_Inline<SW_NonMovable> > b5;
  S_NeedyTemplate2<Template_Inline<SWS_NonMovable> > b6;

  S_NeedyTemplate2<Template_NonMovable<NonMovable> > c1;
  S_NeedyTemplate2<Template_NonMovable<S_NonMovable> > c2;
  S_NeedyTemplate2<Template_NonMovable<W_NonMovable> > c3;
  S_NeedyTemplate2<Template_NonMovable<WS_NonMovable> > c4;
  S_NeedyTemplate2<Template_NonMovable<SW_NonMovable> > c5;
  S_NeedyTemplate2<Template_NonMovable<SWS_NonMovable> > c6;
  S_NeedyTemplate2<Template_NonMovable<Movable> > c7;
  S_NeedyTemplate2<Template_NonMovable<S_Movable> > c8;
  S_NeedyTemplate2<Template_NonMovable<W_Movable> > c9;
  S_NeedyTemplate2<Template_NonMovable<WS_Movable> > c10;
  S_NeedyTemplate2<Template_NonMovable<SW_Movable> > c11;
  S_NeedyTemplate2<Template_NonMovable<SWS_Movable> > c12;
}

void good2() {
  S_NeedyTemplate2<Movable> a1;
  S_NeedyTemplate2<S_Movable> a2;
  S_NeedyTemplate2<W_Movable> a3;
  S_NeedyTemplate2<WS_Movable> a4;
  S_NeedyTemplate2<SW_Movable> a5;
  S_NeedyTemplate2<SWS_Movable> a6;

  S_NeedyTemplate2<Template_Inline<Movable> > b1;
  S_NeedyTemplate2<Template_Inline<S_Movable> > b2;
  S_NeedyTemplate2<Template_Inline<W_Movable> > b3;
  S_NeedyTemplate2<Template_Inline<WS_Movable> > b4;
  S_NeedyTemplate2<Template_Inline<SW_Movable> > b5;
  S_NeedyTemplate2<Template_Inline<SWS_Movable> > b6;

  S_NeedyTemplate2<Template_Unused<Movable> > c1;
  S_NeedyTemplate2<Template_Unused<S_Movable> > c2;
  S_NeedyTemplate2<Template_Unused<W_Movable> > c3;
  S_NeedyTemplate2<Template_Unused<WS_Movable> > c4;
  S_NeedyTemplate2<Template_Unused<SW_Movable> > c5;
  S_NeedyTemplate2<Template_Unused<SWS_Movable> > c6;
  S_NeedyTemplate2<Template_Unused<NonMovable> > c7;
  S_NeedyTemplate2<Template_Unused<S_NonMovable> > c8;
  S_NeedyTemplate2<Template_Unused<W_NonMovable> > c9;
  S_NeedyTemplate2<Template_Unused<WS_NonMovable> > c10;
  S_NeedyTemplate2<Template_Unused<SW_NonMovable> > c11;
  S_NeedyTemplate2<Template_Unused<SWS_NonMovable> > c12;

  S_NeedyTemplate2<Template_Ref<Movable> > d1;
  S_NeedyTemplate2<Template_Ref<S_Movable> > d2;
  S_NeedyTemplate2<Template_Ref<W_Movable> > d3;
  S_NeedyTemplate2<Template_Ref<WS_Movable> > d4;
  S_NeedyTemplate2<Template_Ref<SW_Movable> > d5;
  S_NeedyTemplate2<Template_Ref<SWS_Movable> > d6;
  S_NeedyTemplate2<Template_Ref<NonMovable> > d7;
  S_NeedyTemplate2<Template_Ref<S_NonMovable> > d8;
  S_NeedyTemplate2<Template_Ref<W_NonMovable> > d9;
  S_NeedyTemplate2<Template_Ref<WS_NonMovable> > d10;
  S_NeedyTemplate2<Template_Ref<SW_NonMovable> > d11;
  S_NeedyTemplate2<Template_Ref<SWS_NonMovable> > d12;
}

//
// 3 - Wrapped MOZ_NEEDS_MEMMOVABLE_TYPE
//

template <class T>
struct MOZ_NEEDS_MEMMOVABLE_TYPE NeedyTemplate3 {T m;}; // expected-error-re 26 {{Cannot instantiate 'NeedyTemplate3<{{.*}}>' with non-memmovable template argument '{{.*}}'}}
template <class T>
struct W_NeedyTemplate3 {
  NeedyTemplate3<T> m; // expected-note-re 26 {{instantiation of 'NeedyTemplate3<{{.*}}>' requested here}}
};
void bad3() {
  W_NeedyTemplate3<NonMovable> a1;
  W_NeedyTemplate3<S_NonMovable> a2;
  W_NeedyTemplate3<W_NonMovable> a3;
  W_NeedyTemplate3<WS_NonMovable> a4;
  W_NeedyTemplate3<SW_NonMovable> a5;
  W_NeedyTemplate3<SWS_NonMovable> a6;

  W_NeedyTemplate3<Template_Inline<NonMovable> > b1;
  W_NeedyTemplate3<Template_Inline<S_NonMovable> > b2;
  W_NeedyTemplate3<Template_Inline<W_NonMovable> > b3;
  W_NeedyTemplate3<Template_Inline<WS_NonMovable> > b4;
  W_NeedyTemplate3<Template_Inline<SW_NonMovable> > b5;
  W_NeedyTemplate3<Template_Inline<SWS_NonMovable> > b6;

  W_NeedyTemplate3<Template_NonMovable<NonMovable> > c1;
  W_NeedyTemplate3<Template_NonMovable<S_NonMovable> > c2;
  W_NeedyTemplate3<Template_NonMovable<W_NonMovable> > c3;
  W_NeedyTemplate3<Template_NonMovable<WS_NonMovable> > c4;
  W_NeedyTemplate3<Template_NonMovable<SW_NonMovable> > c5;
  W_NeedyTemplate3<Template_NonMovable<SWS_NonMovable> > c6;
  W_NeedyTemplate3<Template_NonMovable<Movable> > c7;
  W_NeedyTemplate3<Template_NonMovable<S_Movable> > c8;
  W_NeedyTemplate3<Template_NonMovable<W_Movable> > c9;
  W_NeedyTemplate3<Template_NonMovable<WS_Movable> > c10;
  W_NeedyTemplate3<Template_NonMovable<SW_Movable> > c11;
  W_NeedyTemplate3<Template_NonMovable<SWS_Movable> > c12;
}

void good3() {
  W_NeedyTemplate3<Movable> a1;
  W_NeedyTemplate3<S_Movable> a2;
  W_NeedyTemplate3<W_Movable> a3;
  W_NeedyTemplate3<WS_Movable> a4;
  W_NeedyTemplate3<SW_Movable> a5;
  W_NeedyTemplate3<SWS_Movable> a6;

  W_NeedyTemplate3<Template_Inline<Movable> > b1;
  W_NeedyTemplate3<Template_Inline<S_Movable> > b2;
  W_NeedyTemplate3<Template_Inline<W_Movable> > b3;
  W_NeedyTemplate3<Template_Inline<WS_Movable> > b4;
  W_NeedyTemplate3<Template_Inline<SW_Movable> > b5;
  W_NeedyTemplate3<Template_Inline<SWS_Movable> > b6;

  W_NeedyTemplate3<Template_Unused<Movable> > c1;
  W_NeedyTemplate3<Template_Unused<S_Movable> > c2;
  W_NeedyTemplate3<Template_Unused<W_Movable> > c3;
  W_NeedyTemplate3<Template_Unused<WS_Movable> > c4;
  W_NeedyTemplate3<Template_Unused<SW_Movable> > c5;
  W_NeedyTemplate3<Template_Unused<SWS_Movable> > c6;
  W_NeedyTemplate3<Template_Unused<NonMovable> > c7;
  W_NeedyTemplate3<Template_Unused<S_NonMovable> > c8;
  W_NeedyTemplate3<Template_Unused<W_NonMovable> > c9;
  W_NeedyTemplate3<Template_Unused<WS_NonMovable> > c10;
  W_NeedyTemplate3<Template_Unused<SW_NonMovable> > c11;
  W_NeedyTemplate3<Template_Unused<SWS_NonMovable> > c12;

  W_NeedyTemplate3<Template_Ref<Movable> > d1;
  W_NeedyTemplate3<Template_Ref<S_Movable> > d2;
  W_NeedyTemplate3<Template_Ref<W_Movable> > d3;
  W_NeedyTemplate3<Template_Ref<WS_Movable> > d4;
  W_NeedyTemplate3<Template_Ref<SW_Movable> > d5;
  W_NeedyTemplate3<Template_Ref<SWS_Movable> > d6;
  W_NeedyTemplate3<Template_Ref<NonMovable> > d7;
  W_NeedyTemplate3<Template_Ref<S_NonMovable> > d8;
  W_NeedyTemplate3<Template_Ref<W_NonMovable> > d9;
  W_NeedyTemplate3<Template_Ref<WS_NonMovable> > d10;
  W_NeedyTemplate3<Template_Ref<SW_NonMovable> > d11;
  W_NeedyTemplate3<Template_Ref<SWS_NonMovable> > d12;
}

//
// 4 - Wrapped Subclassed MOZ_NEEDS_MEMMOVABLE_TYPE
//

template <class T>
struct MOZ_NEEDS_MEMMOVABLE_TYPE NeedyTemplate4 {T m;}; // expected-error-re 26 {{Cannot instantiate 'NeedyTemplate4<{{.*}}>' with non-memmovable template argument '{{.*}}'}}
template <class T>
struct S_NeedyTemplate4 : NeedyTemplate4<T> {}; // expected-note-re 26 {{instantiation of 'NeedyTemplate4<{{.*}}>' requested here}}
template <class T>
struct WS_NeedyTemplate4 {
  S_NeedyTemplate4<T> m;
};
void bad4() {
  WS_NeedyTemplate4<NonMovable> a1;
  WS_NeedyTemplate4<S_NonMovable> a2;
  WS_NeedyTemplate4<W_NonMovable> a3;
  WS_NeedyTemplate4<WS_NonMovable> a4;
  WS_NeedyTemplate4<SW_NonMovable> a5;
  WS_NeedyTemplate4<SWS_NonMovable> a6;

  WS_NeedyTemplate4<Template_Inline<NonMovable> > b1;
  WS_NeedyTemplate4<Template_Inline<S_NonMovable> > b2;
  WS_NeedyTemplate4<Template_Inline<W_NonMovable> > b3;
  WS_NeedyTemplate4<Template_Inline<WS_NonMovable> > b4;
  WS_NeedyTemplate4<Template_Inline<SW_NonMovable> > b5;
  WS_NeedyTemplate4<Template_Inline<SWS_NonMovable> > b6;

  WS_NeedyTemplate4<Template_NonMovable<NonMovable> > c1;
  WS_NeedyTemplate4<Template_NonMovable<S_NonMovable> > c2;
  WS_NeedyTemplate4<Template_NonMovable<W_NonMovable> > c3;
  WS_NeedyTemplate4<Template_NonMovable<WS_NonMovable> > c4;
  WS_NeedyTemplate4<Template_NonMovable<SW_NonMovable> > c5;
  WS_NeedyTemplate4<Template_NonMovable<SWS_NonMovable> > c6;
  WS_NeedyTemplate4<Template_NonMovable<Movable> > c7;
  WS_NeedyTemplate4<Template_NonMovable<S_Movable> > c8;
  WS_NeedyTemplate4<Template_NonMovable<W_Movable> > c9;
  WS_NeedyTemplate4<Template_NonMovable<WS_Movable> > c10;
  WS_NeedyTemplate4<Template_NonMovable<SW_Movable> > c11;
  WS_NeedyTemplate4<Template_NonMovable<SWS_Movable> > c12;
}

void good4() {
  WS_NeedyTemplate4<Movable> a1;
  WS_NeedyTemplate4<S_Movable> a2;
  WS_NeedyTemplate4<W_Movable> a3;
  WS_NeedyTemplate4<WS_Movable> a4;
  WS_NeedyTemplate4<SW_Movable> a5;
  WS_NeedyTemplate4<SWS_Movable> a6;

  WS_NeedyTemplate4<Template_Inline<Movable> > b1;
  WS_NeedyTemplate4<Template_Inline<S_Movable> > b2;
  WS_NeedyTemplate4<Template_Inline<W_Movable> > b3;
  WS_NeedyTemplate4<Template_Inline<WS_Movable> > b4;
  WS_NeedyTemplate4<Template_Inline<SW_Movable> > b5;
  WS_NeedyTemplate4<Template_Inline<SWS_Movable> > b6;

  WS_NeedyTemplate4<Template_Unused<Movable> > c1;
  WS_NeedyTemplate4<Template_Unused<S_Movable> > c2;
  WS_NeedyTemplate4<Template_Unused<W_Movable> > c3;
  WS_NeedyTemplate4<Template_Unused<WS_Movable> > c4;
  WS_NeedyTemplate4<Template_Unused<SW_Movable> > c5;
  WS_NeedyTemplate4<Template_Unused<SWS_Movable> > c6;
  WS_NeedyTemplate4<Template_Unused<NonMovable> > c7;
  WS_NeedyTemplate4<Template_Unused<S_NonMovable> > c8;
  WS_NeedyTemplate4<Template_Unused<W_NonMovable> > c9;
  WS_NeedyTemplate4<Template_Unused<WS_NonMovable> > c10;
  WS_NeedyTemplate4<Template_Unused<SW_NonMovable> > c11;
  WS_NeedyTemplate4<Template_Unused<SWS_NonMovable> > c12;

  WS_NeedyTemplate4<Template_Ref<Movable> > d1;
  WS_NeedyTemplate4<Template_Ref<S_Movable> > d2;
  WS_NeedyTemplate4<Template_Ref<W_Movable> > d3;
  WS_NeedyTemplate4<Template_Ref<WS_Movable> > d4;
  WS_NeedyTemplate4<Template_Ref<SW_Movable> > d5;
  WS_NeedyTemplate4<Template_Ref<SWS_Movable> > d6;
  WS_NeedyTemplate4<Template_Ref<NonMovable> > d7;
  WS_NeedyTemplate4<Template_Ref<S_NonMovable> > d8;
  WS_NeedyTemplate4<Template_Ref<W_NonMovable> > d9;
  WS_NeedyTemplate4<Template_Ref<WS_NonMovable> > d10;
  WS_NeedyTemplate4<Template_Ref<SW_NonMovable> > d11;
  WS_NeedyTemplate4<Template_Ref<SWS_NonMovable> > d12;
}

//
// 5 - Subclassed Wrapped MOZ_NEEDS_MEMMOVABLE_TYPE
//

template <class T>
struct MOZ_NEEDS_MEMMOVABLE_TYPE NeedyTemplate5 {T m;}; // expected-error-re 26 {{Cannot instantiate 'NeedyTemplate5<{{.*}}>' with non-memmovable template argument '{{.*}}'}}
template <class T>
struct W_NeedyTemplate5 {
  NeedyTemplate5<T> m; // expected-note-re 26 {{instantiation of 'NeedyTemplate5<{{.*}}>' requested here}}
};
template <class T>
struct SW_NeedyTemplate5 : W_NeedyTemplate5<T> {};
void bad5() {
  SW_NeedyTemplate5<NonMovable> a1;
  SW_NeedyTemplate5<S_NonMovable> a2;
  SW_NeedyTemplate5<W_NonMovable> a3;
  SW_NeedyTemplate5<WS_NonMovable> a4;
  SW_NeedyTemplate5<SW_NonMovable> a5;
  SW_NeedyTemplate5<SWS_NonMovable> a6;

  SW_NeedyTemplate5<Template_Inline<NonMovable> > b1;
  SW_NeedyTemplate5<Template_Inline<S_NonMovable> > b2;
  SW_NeedyTemplate5<Template_Inline<W_NonMovable> > b3;
  SW_NeedyTemplate5<Template_Inline<WS_NonMovable> > b4;
  SW_NeedyTemplate5<Template_Inline<SW_NonMovable> > b5;
  SW_NeedyTemplate5<Template_Inline<SWS_NonMovable> > b6;

  SW_NeedyTemplate5<Template_NonMovable<NonMovable> > c1;
  SW_NeedyTemplate5<Template_NonMovable<S_NonMovable> > c2;
  SW_NeedyTemplate5<Template_NonMovable<W_NonMovable> > c3;
  SW_NeedyTemplate5<Template_NonMovable<WS_NonMovable> > c4;
  SW_NeedyTemplate5<Template_NonMovable<SW_NonMovable> > c5;
  SW_NeedyTemplate5<Template_NonMovable<SWS_NonMovable> > c6;
  SW_NeedyTemplate5<Template_NonMovable<Movable> > c7;
  SW_NeedyTemplate5<Template_NonMovable<S_Movable> > c8;
  SW_NeedyTemplate5<Template_NonMovable<W_Movable> > c9;
  SW_NeedyTemplate5<Template_NonMovable<WS_Movable> > c10;
  SW_NeedyTemplate5<Template_NonMovable<SW_Movable> > c11;
  SW_NeedyTemplate5<Template_NonMovable<SWS_Movable> > c12;
}

void good5() {
  SW_NeedyTemplate5<Movable> a1;
  SW_NeedyTemplate5<S_Movable> a2;
  SW_NeedyTemplate5<W_Movable> a3;
  SW_NeedyTemplate5<WS_Movable> a4;
  SW_NeedyTemplate5<SW_Movable> a5;
  SW_NeedyTemplate5<SWS_Movable> a6;

  SW_NeedyTemplate5<Template_Inline<Movable> > b1;
  SW_NeedyTemplate5<Template_Inline<S_Movable> > b2;
  SW_NeedyTemplate5<Template_Inline<W_Movable> > b3;
  SW_NeedyTemplate5<Template_Inline<WS_Movable> > b4;
  SW_NeedyTemplate5<Template_Inline<SW_Movable> > b5;
  SW_NeedyTemplate5<Template_Inline<SWS_Movable> > b6;

  SW_NeedyTemplate5<Template_Unused<Movable> > c1;
  SW_NeedyTemplate5<Template_Unused<S_Movable> > c2;
  SW_NeedyTemplate5<Template_Unused<W_Movable> > c3;
  SW_NeedyTemplate5<Template_Unused<WS_Movable> > c4;
  SW_NeedyTemplate5<Template_Unused<SW_Movable> > c5;
  SW_NeedyTemplate5<Template_Unused<SWS_Movable> > c6;
  SW_NeedyTemplate5<Template_Unused<NonMovable> > c7;
  SW_NeedyTemplate5<Template_Unused<S_NonMovable> > c8;
  SW_NeedyTemplate5<Template_Unused<W_NonMovable> > c9;
  SW_NeedyTemplate5<Template_Unused<WS_NonMovable> > c10;
  SW_NeedyTemplate5<Template_Unused<SW_NonMovable> > c11;
  SW_NeedyTemplate5<Template_Unused<SWS_NonMovable> > c12;

  SW_NeedyTemplate5<Template_Ref<Movable> > d1;
  SW_NeedyTemplate5<Template_Ref<S_Movable> > d2;
  SW_NeedyTemplate5<Template_Ref<W_Movable> > d3;
  SW_NeedyTemplate5<Template_Ref<WS_Movable> > d4;
  SW_NeedyTemplate5<Template_Ref<SW_Movable> > d5;
  SW_NeedyTemplate5<Template_Ref<SWS_Movable> > d6;
  SW_NeedyTemplate5<Template_Ref<NonMovable> > d7;
  SW_NeedyTemplate5<Template_Ref<S_NonMovable> > d8;
  SW_NeedyTemplate5<Template_Ref<W_NonMovable> > d9;
  SW_NeedyTemplate5<Template_Ref<WS_NonMovable> > d10;
  SW_NeedyTemplate5<Template_Ref<SW_NonMovable> > d11;
  SW_NeedyTemplate5<Template_Ref<SWS_NonMovable> > d12;
}

//
// 6 - MOZ_NEEDS_MEMMOVABLE_TYPE instantiated with default template argument
//
// Note: This has an extra error, because it also includes a test with the default template argument.
//

template <class T>
struct MOZ_NEEDS_MEMMOVABLE_TYPE NeedyTemplate6 {T m;}; // expected-error-re 27 {{Cannot instantiate 'NeedyTemplate6<{{.*}}>' with non-memmovable template argument '{{.*}}'}}
template <class T>
struct W_NeedyTemplate6 {
  NeedyTemplate6<T> m; // expected-note-re 27 {{instantiation of 'NeedyTemplate6<{{.*}}>' requested here}}
};
template <class T>
struct SW_NeedyTemplate6 : W_NeedyTemplate6<T> {};
// We create a different NonMovable type here, as NeedyTemplate6 will already be instantiated with NonMovable
struct MOZ_NON_MEMMOVABLE NonMovable2 {};
template <class T = NonMovable2>
struct Defaulted_SW_NeedyTemplate6 {
  SW_NeedyTemplate6<T> m;
};
void bad6() {
  Defaulted_SW_NeedyTemplate6<NonMovable> a1;
  Defaulted_SW_NeedyTemplate6<S_NonMovable> a2;
  Defaulted_SW_NeedyTemplate6<W_NonMovable> a3;
  Defaulted_SW_NeedyTemplate6<WS_NonMovable> a4;
  Defaulted_SW_NeedyTemplate6<SW_NonMovable> a5;
  Defaulted_SW_NeedyTemplate6<SWS_NonMovable> a6;

  Defaulted_SW_NeedyTemplate6<Template_Inline<NonMovable> > b1;
  Defaulted_SW_NeedyTemplate6<Template_Inline<S_NonMovable> > b2;
  Defaulted_SW_NeedyTemplate6<Template_Inline<W_NonMovable> > b3;
  Defaulted_SW_NeedyTemplate6<Template_Inline<WS_NonMovable> > b4;
  Defaulted_SW_NeedyTemplate6<Template_Inline<SW_NonMovable> > b5;
  Defaulted_SW_NeedyTemplate6<Template_Inline<SWS_NonMovable> > b6;

  Defaulted_SW_NeedyTemplate6<Template_NonMovable<NonMovable> > c1;
  Defaulted_SW_NeedyTemplate6<Template_NonMovable<S_NonMovable> > c2;
  Defaulted_SW_NeedyTemplate6<Template_NonMovable<W_NonMovable> > c3;
  Defaulted_SW_NeedyTemplate6<Template_NonMovable<WS_NonMovable> > c4;
  Defaulted_SW_NeedyTemplate6<Template_NonMovable<SW_NonMovable> > c5;
  Defaulted_SW_NeedyTemplate6<Template_NonMovable<SWS_NonMovable> > c6;
  Defaulted_SW_NeedyTemplate6<Template_NonMovable<Movable> > c7;
  Defaulted_SW_NeedyTemplate6<Template_NonMovable<S_Movable> > c8;
  Defaulted_SW_NeedyTemplate6<Template_NonMovable<W_Movable> > c9;
  Defaulted_SW_NeedyTemplate6<Template_NonMovable<WS_Movable> > c10;
  Defaulted_SW_NeedyTemplate6<Template_NonMovable<SW_Movable> > c11;
  Defaulted_SW_NeedyTemplate6<Template_NonMovable<SWS_Movable> > c12;

  Defaulted_SW_NeedyTemplate6<> c13;
}

void good6() {
  Defaulted_SW_NeedyTemplate6<Movable> a1;
  Defaulted_SW_NeedyTemplate6<S_Movable> a2;
  Defaulted_SW_NeedyTemplate6<W_Movable> a3;
  Defaulted_SW_NeedyTemplate6<WS_Movable> a4;
  Defaulted_SW_NeedyTemplate6<SW_Movable> a5;
  Defaulted_SW_NeedyTemplate6<SWS_Movable> a6;

  Defaulted_SW_NeedyTemplate6<Template_Inline<Movable> > b1;
  Defaulted_SW_NeedyTemplate6<Template_Inline<S_Movable> > b2;
  Defaulted_SW_NeedyTemplate6<Template_Inline<W_Movable> > b3;
  Defaulted_SW_NeedyTemplate6<Template_Inline<WS_Movable> > b4;
  Defaulted_SW_NeedyTemplate6<Template_Inline<SW_Movable> > b5;
  Defaulted_SW_NeedyTemplate6<Template_Inline<SWS_Movable> > b6;

  Defaulted_SW_NeedyTemplate6<Template_Unused<Movable> > c1;
  Defaulted_SW_NeedyTemplate6<Template_Unused<S_Movable> > c2;
  Defaulted_SW_NeedyTemplate6<Template_Unused<W_Movable> > c3;
  Defaulted_SW_NeedyTemplate6<Template_Unused<WS_Movable> > c4;
  Defaulted_SW_NeedyTemplate6<Template_Unused<SW_Movable> > c5;
  Defaulted_SW_NeedyTemplate6<Template_Unused<SWS_Movable> > c6;
  Defaulted_SW_NeedyTemplate6<Template_Unused<NonMovable> > c7;
  Defaulted_SW_NeedyTemplate6<Template_Unused<S_NonMovable> > c8;
  Defaulted_SW_NeedyTemplate6<Template_Unused<W_NonMovable> > c9;
  Defaulted_SW_NeedyTemplate6<Template_Unused<WS_NonMovable> > c10;
  Defaulted_SW_NeedyTemplate6<Template_Unused<SW_NonMovable> > c11;
  Defaulted_SW_NeedyTemplate6<Template_Unused<SWS_NonMovable> > c12;

  Defaulted_SW_NeedyTemplate6<Template_Ref<Movable> > d1;
  Defaulted_SW_NeedyTemplate6<Template_Ref<S_Movable> > d2;
  Defaulted_SW_NeedyTemplate6<Template_Ref<W_Movable> > d3;
  Defaulted_SW_NeedyTemplate6<Template_Ref<WS_Movable> > d4;
  Defaulted_SW_NeedyTemplate6<Template_Ref<SW_Movable> > d5;
  Defaulted_SW_NeedyTemplate6<Template_Ref<SWS_Movable> > d6;
  Defaulted_SW_NeedyTemplate6<Template_Ref<NonMovable> > d7;
  Defaulted_SW_NeedyTemplate6<Template_Ref<S_NonMovable> > d8;
  Defaulted_SW_NeedyTemplate6<Template_Ref<W_NonMovable> > d9;
  Defaulted_SW_NeedyTemplate6<Template_Ref<WS_NonMovable> > d10;
  Defaulted_SW_NeedyTemplate6<Template_Ref<SW_NonMovable> > d11;
  Defaulted_SW_NeedyTemplate6<Template_Ref<SWS_NonMovable> > d12;
}

//
// 7 - MOZ_NEEDS_MEMMOVABLE_TYPE instantiated as default template argument
//

template <class T>
struct MOZ_NEEDS_MEMMOVABLE_TYPE NeedyTemplate7 {T m;}; // expected-error-re 26 {{Cannot instantiate 'NeedyTemplate7<{{.*}}>' with non-memmovable template argument '{{.*}}'}}
template <class T, class Q = NeedyTemplate7<T> >
struct Defaulted_Templated_NeedyTemplate7 {Q m;}; // expected-note-re 26 {{instantiation of 'NeedyTemplate7<{{.*}}>' requested here}}
void bad7() {
  Defaulted_Templated_NeedyTemplate7<NonMovable> a1;
  Defaulted_Templated_NeedyTemplate7<S_NonMovable> a2;
  Defaulted_Templated_NeedyTemplate7<W_NonMovable> a3;
  Defaulted_Templated_NeedyTemplate7<WS_NonMovable> a4;
  Defaulted_Templated_NeedyTemplate7<SW_NonMovable> a5;
  Defaulted_Templated_NeedyTemplate7<SWS_NonMovable> a6;

  Defaulted_Templated_NeedyTemplate7<Template_Inline<NonMovable> > b1;
  Defaulted_Templated_NeedyTemplate7<Template_Inline<S_NonMovable> > b2;
  Defaulted_Templated_NeedyTemplate7<Template_Inline<W_NonMovable> > b3;
  Defaulted_Templated_NeedyTemplate7<Template_Inline<WS_NonMovable> > b4;
  Defaulted_Templated_NeedyTemplate7<Template_Inline<SW_NonMovable> > b5;
  Defaulted_Templated_NeedyTemplate7<Template_Inline<SWS_NonMovable> > b6;

  Defaulted_Templated_NeedyTemplate7<Template_NonMovable<NonMovable> > c1;
  Defaulted_Templated_NeedyTemplate7<Template_NonMovable<S_NonMovable> > c2;
  Defaulted_Templated_NeedyTemplate7<Template_NonMovable<W_NonMovable> > c3;
  Defaulted_Templated_NeedyTemplate7<Template_NonMovable<WS_NonMovable> > c4;
  Defaulted_Templated_NeedyTemplate7<Template_NonMovable<SW_NonMovable> > c5;
  Defaulted_Templated_NeedyTemplate7<Template_NonMovable<SWS_NonMovable> > c6;
  Defaulted_Templated_NeedyTemplate7<Template_NonMovable<Movable> > c7;
  Defaulted_Templated_NeedyTemplate7<Template_NonMovable<S_Movable> > c8;
  Defaulted_Templated_NeedyTemplate7<Template_NonMovable<W_Movable> > c9;
  Defaulted_Templated_NeedyTemplate7<Template_NonMovable<WS_Movable> > c10;
  Defaulted_Templated_NeedyTemplate7<Template_NonMovable<SW_Movable> > c11;
  Defaulted_Templated_NeedyTemplate7<Template_NonMovable<SWS_Movable> > c12;
}

void good7() {
  Defaulted_Templated_NeedyTemplate7<Movable> a1;
  Defaulted_Templated_NeedyTemplate7<S_Movable> a2;
  Defaulted_Templated_NeedyTemplate7<W_Movable> a3;
  Defaulted_Templated_NeedyTemplate7<WS_Movable> a4;
  Defaulted_Templated_NeedyTemplate7<SW_Movable> a5;
  Defaulted_Templated_NeedyTemplate7<SWS_Movable> a6;

  Defaulted_Templated_NeedyTemplate7<Template_Inline<Movable> > b1;
  Defaulted_Templated_NeedyTemplate7<Template_Inline<S_Movable> > b2;
  Defaulted_Templated_NeedyTemplate7<Template_Inline<W_Movable> > b3;
  Defaulted_Templated_NeedyTemplate7<Template_Inline<WS_Movable> > b4;
  Defaulted_Templated_NeedyTemplate7<Template_Inline<SW_Movable> > b5;
  Defaulted_Templated_NeedyTemplate7<Template_Inline<SWS_Movable> > b6;

  Defaulted_Templated_NeedyTemplate7<Template_Unused<Movable> > c1;
  Defaulted_Templated_NeedyTemplate7<Template_Unused<S_Movable> > c2;
  Defaulted_Templated_NeedyTemplate7<Template_Unused<W_Movable> > c3;
  Defaulted_Templated_NeedyTemplate7<Template_Unused<WS_Movable> > c4;
  Defaulted_Templated_NeedyTemplate7<Template_Unused<SW_Movable> > c5;
  Defaulted_Templated_NeedyTemplate7<Template_Unused<SWS_Movable> > c6;
  Defaulted_Templated_NeedyTemplate7<Template_Unused<NonMovable> > c7;
  Defaulted_Templated_NeedyTemplate7<Template_Unused<S_NonMovable> > c8;
  Defaulted_Templated_NeedyTemplate7<Template_Unused<W_NonMovable> > c9;
  Defaulted_Templated_NeedyTemplate7<Template_Unused<WS_NonMovable> > c10;
  Defaulted_Templated_NeedyTemplate7<Template_Unused<SW_NonMovable> > c11;
  Defaulted_Templated_NeedyTemplate7<Template_Unused<SWS_NonMovable> > c12;

  Defaulted_Templated_NeedyTemplate7<Template_Ref<Movable> > d1;
  Defaulted_Templated_NeedyTemplate7<Template_Ref<S_Movable> > d2;
  Defaulted_Templated_NeedyTemplate7<Template_Ref<W_Movable> > d3;
  Defaulted_Templated_NeedyTemplate7<Template_Ref<WS_Movable> > d4;
  Defaulted_Templated_NeedyTemplate7<Template_Ref<SW_Movable> > d5;
  Defaulted_Templated_NeedyTemplate7<Template_Ref<SWS_Movable> > d6;
  Defaulted_Templated_NeedyTemplate7<Template_Ref<NonMovable> > d7;
  Defaulted_Templated_NeedyTemplate7<Template_Ref<S_NonMovable> > d8;
  Defaulted_Templated_NeedyTemplate7<Template_Ref<W_NonMovable> > d9;
  Defaulted_Templated_NeedyTemplate7<Template_Ref<WS_NonMovable> > d10;
  Defaulted_Templated_NeedyTemplate7<Template_Ref<SW_NonMovable> > d11;
  Defaulted_Templated_NeedyTemplate7<Template_Ref<SWS_NonMovable> > d12;
}

//
// 8 - Wrapped MOZ_NEEDS_MEMMOVABLE_TYPE instantiated as default template argument
//

template <class T>
struct MOZ_NEEDS_MEMMOVABLE_TYPE NeedyTemplate8 {T m;}; // expected-error-re 26 {{Cannot instantiate 'NeedyTemplate8<{{.*}}>' with non-memmovable template argument '{{.*}}'}}
template <class T, class Q = NeedyTemplate8<T> >
struct Defaulted_Templated_NeedyTemplate8 {Q m;}; // expected-note-re 26 {{instantiation of 'NeedyTemplate8<{{.*}}>' requested here}}
template <class T>
struct W_Defaulted_Templated_NeedyTemplate8 {
  Defaulted_Templated_NeedyTemplate8<T> m;
};
void bad8() {
  W_Defaulted_Templated_NeedyTemplate8<NonMovable> a1;
  W_Defaulted_Templated_NeedyTemplate8<S_NonMovable> a2;
  W_Defaulted_Templated_NeedyTemplate8<W_NonMovable> a3;
  W_Defaulted_Templated_NeedyTemplate8<WS_NonMovable> a4;
  W_Defaulted_Templated_NeedyTemplate8<SW_NonMovable> a5;
  W_Defaulted_Templated_NeedyTemplate8<SWS_NonMovable> a6;

  W_Defaulted_Templated_NeedyTemplate8<Template_Inline<NonMovable> > b1;
  W_Defaulted_Templated_NeedyTemplate8<Template_Inline<S_NonMovable> > b2;
  W_Defaulted_Templated_NeedyTemplate8<Template_Inline<W_NonMovable> > b3;
  W_Defaulted_Templated_NeedyTemplate8<Template_Inline<WS_NonMovable> > b4;
  W_Defaulted_Templated_NeedyTemplate8<Template_Inline<SW_NonMovable> > b5;
  W_Defaulted_Templated_NeedyTemplate8<Template_Inline<SWS_NonMovable> > b6;

  W_Defaulted_Templated_NeedyTemplate8<Template_NonMovable<NonMovable> > c1;
  W_Defaulted_Templated_NeedyTemplate8<Template_NonMovable<S_NonMovable> > c2;
  W_Defaulted_Templated_NeedyTemplate8<Template_NonMovable<W_NonMovable> > c3;
  W_Defaulted_Templated_NeedyTemplate8<Template_NonMovable<WS_NonMovable> > c4;
  W_Defaulted_Templated_NeedyTemplate8<Template_NonMovable<SW_NonMovable> > c5;
  W_Defaulted_Templated_NeedyTemplate8<Template_NonMovable<SWS_NonMovable> > c6;
  W_Defaulted_Templated_NeedyTemplate8<Template_NonMovable<Movable> > c7;
  W_Defaulted_Templated_NeedyTemplate8<Template_NonMovable<S_Movable> > c8;
  W_Defaulted_Templated_NeedyTemplate8<Template_NonMovable<W_Movable> > c9;
  W_Defaulted_Templated_NeedyTemplate8<Template_NonMovable<WS_Movable> > c10;
  W_Defaulted_Templated_NeedyTemplate8<Template_NonMovable<SW_Movable> > c11;
  W_Defaulted_Templated_NeedyTemplate8<Template_NonMovable<SWS_Movable> > c12;
}

void good8() {
  W_Defaulted_Templated_NeedyTemplate8<Movable> a1;
  W_Defaulted_Templated_NeedyTemplate8<S_Movable> a2;
  W_Defaulted_Templated_NeedyTemplate8<W_Movable> a3;
  W_Defaulted_Templated_NeedyTemplate8<WS_Movable> a4;
  W_Defaulted_Templated_NeedyTemplate8<SW_Movable> a5;
  W_Defaulted_Templated_NeedyTemplate8<SWS_Movable> a6;

  W_Defaulted_Templated_NeedyTemplate8<Template_Inline<Movable> > b1;
  W_Defaulted_Templated_NeedyTemplate8<Template_Inline<S_Movable> > b2;
  W_Defaulted_Templated_NeedyTemplate8<Template_Inline<W_Movable> > b3;
  W_Defaulted_Templated_NeedyTemplate8<Template_Inline<WS_Movable> > b4;
  W_Defaulted_Templated_NeedyTemplate8<Template_Inline<SW_Movable> > b5;
  W_Defaulted_Templated_NeedyTemplate8<Template_Inline<SWS_Movable> > b6;

  W_Defaulted_Templated_NeedyTemplate8<Template_Unused<Movable> > c1;
  W_Defaulted_Templated_NeedyTemplate8<Template_Unused<S_Movable> > c2;
  W_Defaulted_Templated_NeedyTemplate8<Template_Unused<W_Movable> > c3;
  W_Defaulted_Templated_NeedyTemplate8<Template_Unused<WS_Movable> > c4;
  W_Defaulted_Templated_NeedyTemplate8<Template_Unused<SW_Movable> > c5;
  W_Defaulted_Templated_NeedyTemplate8<Template_Unused<SWS_Movable> > c6;
  W_Defaulted_Templated_NeedyTemplate8<Template_Unused<NonMovable> > c7;
  W_Defaulted_Templated_NeedyTemplate8<Template_Unused<S_NonMovable> > c8;
  W_Defaulted_Templated_NeedyTemplate8<Template_Unused<W_NonMovable> > c9;
  W_Defaulted_Templated_NeedyTemplate8<Template_Unused<WS_NonMovable> > c10;
  W_Defaulted_Templated_NeedyTemplate8<Template_Unused<SW_NonMovable> > c11;
  W_Defaulted_Templated_NeedyTemplate8<Template_Unused<SWS_NonMovable> > c12;

  W_Defaulted_Templated_NeedyTemplate8<Template_Ref<Movable> > d1;
  W_Defaulted_Templated_NeedyTemplate8<Template_Ref<S_Movable> > d2;
  W_Defaulted_Templated_NeedyTemplate8<Template_Ref<W_Movable> > d3;
  W_Defaulted_Templated_NeedyTemplate8<Template_Ref<WS_Movable> > d4;
  W_Defaulted_Templated_NeedyTemplate8<Template_Ref<SW_Movable> > d5;
  W_Defaulted_Templated_NeedyTemplate8<Template_Ref<SWS_Movable> > d6;
  W_Defaulted_Templated_NeedyTemplate8<Template_Ref<NonMovable> > d7;
  W_Defaulted_Templated_NeedyTemplate8<Template_Ref<S_NonMovable> > d8;
  W_Defaulted_Templated_NeedyTemplate8<Template_Ref<W_NonMovable> > d9;
  W_Defaulted_Templated_NeedyTemplate8<Template_Ref<WS_NonMovable> > d10;
  W_Defaulted_Templated_NeedyTemplate8<Template_Ref<SW_NonMovable> > d11;
  W_Defaulted_Templated_NeedyTemplate8<Template_Ref<SWS_NonMovable> > d12;
}

/*
  SpecializedNonMovable is a non-movable class which has an explicit specialization of NeedyTemplate
  for it. Instantiations of NeedyTemplateN<SpecializedNonMovable> should be legal as the explicit
  specialization isn't annotated with MOZ_NEEDS_MEMMOVABLE_TYPE.

  However, as it is MOZ_NON_MEMMOVABLE, derived classes and members shouldn't be able to be used to
  instantiate NeedyTemplate.
*/

struct MOZ_NON_MEMMOVABLE SpecializedNonMovable {};
struct S_SpecializedNonMovable : SpecializedNonMovable {}; // expected-note 8 {{'S_SpecializedNonMovable' is a non-memmove()able type because it inherits from a non-memmove()able type 'SpecializedNonMovable'}}

// Specialize all of the NeedyTemplates with SpecializedNonMovable.
template <>
struct NeedyTemplate1<SpecializedNonMovable> {};
template <>
struct NeedyTemplate2<SpecializedNonMovable> {};
template <>
struct NeedyTemplate3<SpecializedNonMovable> {};
template <>
struct NeedyTemplate4<SpecializedNonMovable> {};
template <>
struct NeedyTemplate5<SpecializedNonMovable> {};
template <>
struct NeedyTemplate6<SpecializedNonMovable> {};
template <>
struct NeedyTemplate7<SpecializedNonMovable> {};
template <>
struct NeedyTemplate8<SpecializedNonMovable> {};

void specialization() {
  /*
    SpecializedNonMovable has a specialization for every variant of NeedyTemplate,
    so these templates are valid, even though SpecializedNonMovable isn't
    memmovable
  */
  NeedyTemplate1<SpecializedNonMovable> a1;
  S_NeedyTemplate2<SpecializedNonMovable> a2;
  W_NeedyTemplate3<SpecializedNonMovable> a3;
  WS_NeedyTemplate4<SpecializedNonMovable> a4;
  SW_NeedyTemplate5<SpecializedNonMovable> a5;
  Defaulted_SW_NeedyTemplate6<SpecializedNonMovable> a6;
  Defaulted_Templated_NeedyTemplate7<SpecializedNonMovable> a7;
  W_Defaulted_Templated_NeedyTemplate8<SpecializedNonMovable> a8;

  /*
    These entries contain an element which is SpecializedNonMovable, and are non-movable
    as there is no valid specialization, and their member is non-memmovable
  */
  NeedyTemplate1<Template_Inline<SpecializedNonMovable> > b1;  // expected-note {{instantiation of 'NeedyTemplate1<Template_Inline<SpecializedNonMovable> >' requested here}}
  S_NeedyTemplate2<Template_Inline<SpecializedNonMovable> > b2;
  W_NeedyTemplate3<Template_Inline<SpecializedNonMovable> > b3;
  WS_NeedyTemplate4<Template_Inline<SpecializedNonMovable> > b4;
  SW_NeedyTemplate5<Template_Inline<SpecializedNonMovable> > b5;
  Defaulted_SW_NeedyTemplate6<Template_Inline<SpecializedNonMovable> > b6;
  Defaulted_Templated_NeedyTemplate7<Template_Inline<SpecializedNonMovable> > b7;
  W_Defaulted_Templated_NeedyTemplate8<Template_Inline<SpecializedNonMovable> > b8;

  /*
    The subclass of SpecializedNonMovable, is also non-memmovable,
    as there is no valid specialization.
  */
  NeedyTemplate1<S_SpecializedNonMovable> c1; // expected-note {{instantiation of 'NeedyTemplate1<S_SpecializedNonMovable>' requested here}}
  S_NeedyTemplate2<S_SpecializedNonMovable> c2;
  W_NeedyTemplate3<S_SpecializedNonMovable> c3;
  WS_NeedyTemplate4<S_SpecializedNonMovable> c4;
  SW_NeedyTemplate5<S_SpecializedNonMovable> c5;
  Defaulted_SW_NeedyTemplate6<S_SpecializedNonMovable> c6;
  Defaulted_Templated_NeedyTemplate7<S_SpecializedNonMovable> c7;
  W_Defaulted_Templated_NeedyTemplate8<S_SpecializedNonMovable> c8;
}

class MOZ_NEEDS_MEMMOVABLE_MEMBERS NeedsMemMovableMembers {
  Movable m1;
  NonMovable m2; // expected-error {{class 'NeedsMemMovableMembers' cannot have non-memmovable member 'm2' of type 'NonMovable'}}
  S_Movable sm1;
  S_NonMovable sm2; // expected-error {{class 'NeedsMemMovableMembers' cannot have non-memmovable member 'sm2' of type 'S_NonMovable'}}
  W_Movable wm1;
  W_NonMovable wm2; // expected-error {{class 'NeedsMemMovableMembers' cannot have non-memmovable member 'wm2' of type 'W_NonMovable'}}
  SW_Movable swm1;
  SW_NonMovable swm2; // expected-error {{class 'NeedsMemMovableMembers' cannot have non-memmovable member 'swm2' of type 'SW_NonMovable'}}
  WS_Movable wsm1;
  WS_NonMovable wsm2; // expected-error {{class 'NeedsMemMovableMembers' cannot have non-memmovable member 'wsm2' of type 'WS_NonMovable'}}
  SWS_Movable swsm1;
  SWS_NonMovable swsm2; // expected-error {{class 'NeedsMemMovableMembers' cannot have non-memmovable member 'swsm2' of type 'SWS_NonMovable'}}
};

class NeedsMemMovableMembersDerived : public NeedsMemMovableMembers {};
