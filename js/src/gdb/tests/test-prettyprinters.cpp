#include "gdb-tests.h"

typedef int A;
typedef A B;

class C { };
class D { };
typedef C C_;
typedef D D_;
class E: C, D { };
typedef E E_;
class F: C_, D_ { };
class G { };
class H: F, G { };

FRAGMENT(prettyprinters, implemented_types) {
  int i;
  A a;
  B b;
  C c;
  C_ c_;
  E e;
  E_ e_;
  F f;
  H h;

  breakpoint();

  (void) i;
  (void) a;
  (void) b;
  (void) c;
  (void) c_;
  (void) e;
  (void) e_;
  (void) f;
  (void) h;
}
