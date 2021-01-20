function testCongruent(i) {
  var p = {};
  var o = {
    // Add toString as an own property, so it'll be always found on this object,
    // even when properties are changed on the prototype.
    toString: Object.prototype.toString,

    // Add a custom prototype, so we can add @@toStringTag without modifying the
    // shape of this object.
    __proto__: p,
  };
  var xs = [{}, p];
  var ys = ["[object Object]", "[object Test]"];

  for (var j = 0; j <= 100; ++j) {
    // Don't use if-statements to avoid cold code bailouts
    var x = xs[(i === 1 && j === 100)|0];
    var y = ys[(i === 1 && j === 100)|0];

    // |o.toString()| must be executed twice, because |x[Symbol.toStringTag]| may
    // have modified |o|.
    var r = o.toString();
    x[Symbol.toStringTag] = "Test";
    var e = o.toString();

    assertEq(r, "[object Object]");
    assertEq(e, y);
  }
}
for (var i = 0; i < 2; ++i) testCongruent(i);

function testUnobserved(i) {
  var p = {};
  var o = {
    // Add toString as an own property, so it'll be always found on this object,
    // even when properties are changed on the prototype.
    toString: Object.prototype.toString,

    // Add a custom prototype, so we can add @@toStringTag without modifying the
    // shape of this object.
    __proto__: p,
  };
  var xs = [{}, p];
  var ys = [false, true];

  for (var j = 0; j <= 100; ++j) {
    // Don't use if-statements to avoid cold code bailouts
    var x = xs[(i === 1 && j === 100)|0];
    var y = ys[(i === 1 && j === 100)|0];

    var executed = false;
    Object.defineProperty(x, Symbol.toStringTag, {
      configurable: true,
      get() {
        executed = true;
      }
    });

    // |o.toString()| must be executed even when the result isn't observed.
    o.toString();

    assertEq(executed, y);
  }
}
for (var i = 0; i < 2; ++i) testUnobserved(i);
