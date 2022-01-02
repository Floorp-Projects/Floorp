// |jit-test| 
load(libdir + "asserts.js");

// Parse
class A {
  static { }
};

// Run
var ran = false;
class B {
  static {
    ran = true;
  }
};

assertEq(ran, true);

// Can access static assigned before.
var res;
class C {
  static x = 10;
  static {
    res = this.x;
  }
};

assertEq(res, 10);

// Multiple static blocks
class D {
  static { }
  static { }
};

class D1 {
  static { } static { }
};

// Blocks run in order.
class E {
  static {
    res = 10;
  }
  static {
    assertEq(res, 10);
    res = 14;
  }
}

assertEq(res, 14);


// Can use static to access private state.
let accessor;
class F {
  #x = 10
  static {
    accessor = (o) => o.#x;
  }
}

let f = new F;
assertEq(accessor(f), 10);

class G {
  static {
    this.a = 10;
  }
}
assertEq(G.a, 10);

// Can use the super-keyword to access a static method in super class.
class H {
  static a() { return 101; }
}

class I extends H {
  static {
    res = super.a();
  }
}

assertEq(res, 101);

// Can add a property to a class from within a static
class J {
  static {
    this.x = 12;
  }
}

assertEq(J.x, 12);


function assertThrowsSyntaxError(str) {
  assertThrowsInstanceOf(() => eval(str), SyntaxError);       // Direct Eval
  assertThrowsInstanceOf(() => (1, eval)(str), SyntaxError);  // Indirect Eval
  assertThrowsInstanceOf(() => Function(str), SyntaxError);   // Function
}

assertThrowsSyntaxError(`
class X {
  static {
    arguments;
  }
}
`)


assertThrowsSyntaxError(`
class X extends class {} {
  static {
    super(); // can't call super in a static block
  }
}
`)

assertThrowsSyntaxError(`
class X {
  static {
    return 10; // Can't return in a static block
  }
}
`)

assertThrowsSyntaxError(`
class X {
  static {
    await 10; // Can't await in a static block
  }
}
`)

// Can't await inside a static block, even if we're inside an async function.
//
// The error message here is not good `SyntaxError: await is a reserved identifier`,
// but can be fixed later. s
assertThrowsSyntaxError(`
async function f() {
  class X {
    static {
      await 10; // Can't await in a static block
    }
  }
}
`)


assertThrowsSyntaxError(`
class X {
  static {
    yield 10; // Can't yield in a static block
  }
}
`)


assertThrowsSyntaxError(`
function* gen() {
  class X {
    static {
      yield 10; // Can't yield in a static block, even inside a generator
    }
  }
}
`)

// Incomplete should throw a sensible error.
assertThrowsSyntaxError(`
class A { static { this.x
`)