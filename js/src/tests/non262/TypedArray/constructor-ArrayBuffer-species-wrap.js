// |reftest| skip-if(!xulRuntime.shell)

let g = newGlobal();

// Both TypedArray and ArrayBuffer from different global.
for (let ctor of typedArrayConstructors) {
  let a = g.eval(`new ${ctor.name}([1, 2, 3, 4, 5]);`);
  for (let ctor2 of typedArrayConstructors) {
    let b = new ctor2(a);
    assertEq(Object.getPrototypeOf(b).constructor, ctor2);
    assertEq(Object.getPrototypeOf(b.buffer).constructor, g.ArrayBuffer);
  }
}

// Only ArrayBuffer from different global.
let called = false;
let origSpecies = Object.getOwnPropertyDescriptor(ArrayBuffer, Symbol.species);
let modSpecies = {
  get() {
    called = true;
    return g.ArrayBuffer;
  }
};
for (let ctor of typedArrayConstructors) {
  let a = new ctor([1, 2, 3, 4, 5]);
  for (let ctor2 of typedArrayConstructors) {
    called = false;
    Object.defineProperty(ArrayBuffer, Symbol.species, modSpecies);
    let b = new ctor2(a);
    Object.defineProperty(ArrayBuffer, Symbol.species, origSpecies);
    assertEq(called, true);
    assertEq(Object.getPrototypeOf(b).constructor, ctor2);
    assertEq(Object.getPrototypeOf(b.buffer).constructor, g.ArrayBuffer);
  }
}

// Only TypedArray from different global.
g.otherArrayBuffer = ArrayBuffer;
g.eval(`
var called = false;
var origSpecies = Object.getOwnPropertyDescriptor(ArrayBuffer, Symbol.species);
var modSpecies = {
  get() {
    called = true;
    return otherArrayBuffer;
  }
};
`);
for (let ctor of typedArrayConstructors) {
  let a = g.eval(`new ${ctor.name}([1, 2, 3, 4, 5]);`);
  for (let ctor2 of typedArrayConstructors) {
    g.called = false;
    g.eval(`Object.defineProperty(ArrayBuffer, Symbol.species, modSpecies);`);
    let b = new ctor2(a);
    g.eval(`Object.defineProperty(ArrayBuffer, Symbol.species, origSpecies);`);
    assertEq(g.called, true);
    assertEq(Object.getPrototypeOf(b).constructor, ctor2);
    assertEq(Object.getPrototypeOf(b.buffer).constructor, ArrayBuffer);
  }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
