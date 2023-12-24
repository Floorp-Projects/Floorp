// |jit-test| skip-if: !getBuildConfiguration("decorators")

load(libdir + "asserts.js");

let dec1Called = false;

// This explicitly tests the case where undefined is returned.
function dec(value, context) {
  dec1Called = true;
  // return undefined
}

function decorate_getter(value, context) {
  return {
    get: function() {
      return 2 * value.get.call(this);
    }
  };
}

function decorate_setter(value, context) {
  return {
    set: function(x) {
      return value.set.call(this, 2*x);
    }
  };
}

function decorate_initializer(value, context) {
  return {
    init: function(initialValue) {
      return 2 * initialValue;
    }
  };
}

function checkDecoratorContext(kind, isPrivate, isStatic, name) {
  return (value, context) => {
    assertEq(typeof value, "object");
    assertEq(typeof value.get, "function");
    assertEq(typeof value.set, "function");
    assertEq(context.kind, kind);
    assertEq(typeof context.access, "object");
    assertEq(context.private, isPrivate);
    assertEq(context.static, isStatic);
    assertEq(context.name, name);
    if (isStatic) {
      assertEq(typeof context.addInitializer, "undefined");
    } else {
      assertEq(typeof context.addInitializer, "function");
    }
    // return undefined
  }
}

class C {
  @dec accessor x = 1;
  @decorate_getter accessor x2 = 1;
  @decorate_setter @decorate_getter accessor x3 = 1;
  @decorate_initializer accessor x4 = 1;
  @decorate_initializer @decorate_initializer accessor x5 = 1;
  @decorate_setter @decorate_getter @decorate_initializer accessor x6 = 1;
  @checkDecoratorContext("accessor", true, false, "x8 accessor storage") accessor x8 = 1;
  @checkDecoratorContext("accessor", true, true, "x9 accessor storage") static accessor x9 = 1;
  @checkDecoratorContext("accessor", true, false, "#x10 accessor storage") accessor #x10 = 1;
}

let c = new C();
assertEq(dec1Called, true);
assertEq(c.x, 1);
c.x = 2;
assertEq(c.x, 2);
assertEq(c.x2, 2);
assertEq(c.x3, 2);
c.x3 = 4;
assertEq(c.x3, 16);
assertEq(c.x4, 2);
assertEq(c.x5, 4);
assertEq(c.x6, 4);
c.x6 = 4;
assertEq(c.x6, 16);

class D {
  @decorate_initializer accessor #x = 1;
  @decorate_getter accessor #x2 = 1;
  @decorate_setter @decorate_getter accessor #x3 = 1;

  getX() {
    return this.#x;
  }

  setX(v) {
    this.#x = v;
  }

  getX2() {
    return this.#x2;
  }

  setX2(v) {
    this.#x2 = v;
  }

  getX3() {
    return this.#x3;
  }

  setX3(v) {
    this.#x3 = v;
  }
}

let d = new D();
assertEq(d.getX(), 2);
d.setX(4);
assertEq(d.getX(), 4);
assertEq(d.getX2(), 2);
d.setX2(4);
assertEq(d.getX2(), 8);
assertEq(d.getX3(), 2);
d.setX3(4);
assertEq(d.getX3(), 16);

class E {
  @decorate_getter static accessor x = 1;
}

assertEq(E.x, 2);
E.x = 2;
assertEq(E.x, 4);

class F {
  @decorate_getter static accessor #x = 1;

  getX() {
    return F.#x;
  }

  setX(v) {
    F.#x = v;
  }
}
let f = new F();
assertEq(f.getX(), 2);
f.setX(4);
assertEq(f.getX(), 8);

assertThrowsInstanceOf(() => {
  class G {
    @(() => { return "hello!"; }) accessor x;
  }
}, TypeError), "Returning a value other than undefined or a callable throws.";

assertThrowsInstanceOf(() => {
  class G {
    @(() => { return {get: "hello!"}; }) accessor x;
  }
}, TypeError), "Returning a value other than undefined or a callable throws.";

assertThrowsInstanceOf(() => {
  class G {
    @(() => { return {set: "hello!"}; }) accessor x;
  }
}, TypeError), "Returning a value other than undefined or a callable throws.";

assertThrowsInstanceOf(() => {
  class G {
    @(() => { return {init: "hello!"}; }) accessor x;
  }
}, TypeError), "Returning a value other than undefined or a callable throws.";

const decoratorOrder = [];
function makeOrderedDecorator(order) {
  return function (value, context) {
    decoratorOrder.push(order);
    return value;
  }
}

class H {
  @makeOrderedDecorator(1) @makeOrderedDecorator(2) @makeOrderedDecorator(3)
  accessor x = 1;
}

let h = new H();
assertEq(decoratorOrder.length, 3);
assertEq(decoratorOrder[0], 3);
assertEq(decoratorOrder[1], 2);
assertEq(decoratorOrder[2], 1);
