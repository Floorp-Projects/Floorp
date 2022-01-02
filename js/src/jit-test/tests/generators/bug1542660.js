// |jit-test| skip-if: !('gc' in this) || !('clearKeptObjects' in this)
// Locals in async functions should not keep objects alive after going out of scope.
// Test by Mathieu Hofman.

let nextId = 0;

let weakRef;
let savedCallback;

const tests = [
  function() {
    let object = { id: ++nextId };
    console.log(`created object ${object.id}`);
    savedCallback = () => {};
    weakRef = new WeakRef(object);
  },
  async function() {
    let object = { id: ++nextId };
    console.log(`created object ${object.id}`);
    savedCallback = () => {};
    weakRef = new WeakRef(object);
  },
  async function() {
    function* gen() {
      {
        let object = { id: ++nextId };
        console.log(`created object ${object.id}`);
        // Yielding here stores the local variable `object` in the generator
        // object.
        yield 1;
        weakRef = new WeakRef(object);
      }
      // Yielding here should clear it.
      yield 2;
    }
    let iter = gen();
    assertEq(iter.next().value, 1);
    assertEq(iter.next().value, 2);
    savedCallback = iter;  // Keep the generator alive for GC.
  }
];

(async () => {
  for (const test of tests) {
    await test();
    assertEq(!!weakRef.deref(), true);
    clearKeptObjects();
    gc();
    if (weakRef.deref()) {
      throw new Error(`object ${nextId} was not collected`);
    }
  }
})();
