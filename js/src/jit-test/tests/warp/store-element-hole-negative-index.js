function test() {
  Object.defineProperty(Array.prototype, -1, {
    set() {
      throw new Error("shouldn't get here");
    }
  });

  // A bunch of indices which grow the array.
  var indices = [];
  for (var i = 0; i < 10_000; ++i) indices.push(i);

  // And finally a negative index.
  indices.push(-1);

  // Plain data properties use DefineDataProperty.
  var desc = {value: 0, writable: true, enumerable: true, configurable: true};

  var a = [];
  for (var i = 0; i < indices.length; ++i) {
    Object.defineProperty(a, indices[i], desc);
  }
}

test();
