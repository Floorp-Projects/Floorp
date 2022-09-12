function test() {
  Object.defineProperty(Array.prototype, 10_000 * 50, {
    set() {
      throw new Error("shouldn't get here");
    }
  });

  // A bunch of indices which grow the array.
  var indices = [];
  for (var i = 0; i < 10_000; ++i) indices.push(i);

  // And finally an index for a sparse property.
  indices.push(10_000 * 50);

  // Plain data properties use DefineDataProperty.
  var desc = {value: 0, writable: true, enumerable: true, configurable: true};

  var a = [];
  for (var i = 0; i < indices.length; ++i) {
    Object.defineProperty(a, indices[i], desc);
  }
}

test();
