let hit = false;

function f() {
  hit = true;
}

class C {
  #f = 1;
  static m(x) {
    x.#f = f();  // f() should be called before the brand check for x.#f
  }
}

try {
  C.m({});  // throws a TypeError
} catch { }

assertEq(hit, true);
