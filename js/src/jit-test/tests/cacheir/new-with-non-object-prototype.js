// Test constructing a function when the |.prototype| property isn't an object.

function test(proto) {
  function Klass() {
    this.prop = 1;
  }
  Klass.prototype = proto;

  const N = 100;

  let c = 0;
  for (let i = 0; i < N; ++i) {
    // Create a new object.
    let o = new Klass();

    // Read a property from the new object to ensure it was correctly allocated
    // and initialised.
    c += o.prop;

    // The prototype defaults to %Object.prototype% when the |.prototype|
    // property isn't an object.
    assertEq(Object.getPrototypeOf(o), Object.prototype);
  }

  assertEq(c, N);
}

const primitivesTypes = [
  undefined,
  null,
  123,
  true,
  "str",
  Symbol(),
  123n,
];

for (let primitive of primitivesTypes) {
  // Create a fresh function object to avoid type pollution.
  let fn = Function(`return ${test}`)();

  fn(primitive);
}

// Repeat the test from above, but this time |Klass| is a cross-realm function.

function testCrossRealm(proto) {
  const otherGlobal = newGlobal();
  const Klass = otherGlobal.eval(`
    function Klass() {
      this.prop = 1;
    }
    Klass;
  `);
  Klass.prototype = proto;

  const N = 100;

  let c = 0;
  for (let i = 0; i < N; ++i) {
    // Create a new object.
    let o = new Klass();

    // Read a property from the new object to ensure it was correctly allocated
    // and initialised.
    c += o.prop;

    // The prototype defaults to %Object.prototype% when the |.prototype|
    // property isn't an object.
    assertEq(Object.getPrototypeOf(o), otherGlobal.Object.prototype);
  }

  assertEq(c, N);
}

for (let primitive of primitivesTypes) {
  // Create a fresh function object to avoid type pollution.
  let fn = Function(`return ${testCrossRealm}`)();

  fn(primitive);
}

// Repeat the test from above, but this time |Klass| is a cross-realm new.target.

function testCrossRealmNewTarget(proto) {
  const otherGlobal = newGlobal();
  const Klass = otherGlobal.eval(`
    function Klass() {}
    Klass;
  `);
  Klass.prototype = proto;

  class C {
    constructor() {
      this.prop = 1;
    }
  }

  class D extends C {
    constructor() {
      super();
    }
  }

  const N = 100;

  let c = 0;
  for (let i = 0; i < N; ++i) {
    // Create a new object.
    let o = Reflect.construct(D, [], Klass);

    // Read a property from the new object to ensure it was correctly allocated
    // and initialised.
    c += o.prop;

    // The prototype defaults to %Object.prototype% when the |.prototype|
    // property isn't an object.
    assertEq(Object.getPrototypeOf(o), otherGlobal.Object.prototype);
  }

  assertEq(c, N);
}

for (let primitive of primitivesTypes) {
  // Create a fresh function object to avoid type pollution.
  let fn = Function(`return ${testCrossRealmNewTarget}`)();

  fn(primitive);
}
