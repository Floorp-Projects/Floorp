#include "gdb-tests.h"

typedef int A;
typedef A B;

class C {};
class D {};
typedef C C_;
typedef D D_;
class E : C, D {};
typedef E E_;
class F : C_, D_ {};
class G {};
class H : F, G {};

FRAGMENT(prettyprinters, implemented_types) {
  int i = 0;
  A a = 0;
  B b = 0;
  C c;
  C_ c_;
  E e;
  E_ e_;
  F f;
  H h;

  breakpoint();

  use(i);
  use(a);
  use(b);
  use(c);
  use(c_);
  use(e);
  use(e_);
  use(f);
  use(h);
}
