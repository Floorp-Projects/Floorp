// |jit-test| skip-if: !getBuildConfiguration("decorators")

load(libdir + "asserts.js");

let globalDecCalled = false;
function globalDec(value, context) {
  globalDecCalled = true;
  assertEq(this, globalThis);
}

// Forward declare c to be able to check it inside of C when running decorators.
let c;
let classDecCalled = false;
class C {
  classDec(value, context) {
    classDecCalled = true;
    // At this point, `this` is an instance of C
    assertEq(c, this);
    return function(initialValue) {
      // At this point, `this` is an instance of D
      assertEq(this instanceof D, true);
      return initialValue;
    }
  }
}

c = new C();

class D {
  @globalDec x1;
  @c.classDec x2;
}

let d = new D();
assertEq(globalDecCalled, true);
assertEq(classDecCalled, true);
