var g = newGlobal();
function foo() {
  try {
    foo();
  } catch {
    g.Reflect.construct(g.Uint8ClampedArray, new g.Int8Array);
  }
}

foo();
