// Bug 1021379 - Rename typed arrays' move method to copyWithin,
// fix up to ES6 semantics
// Tests for TypedArray#copyWithin

load(libdir + "asserts.js");

const constructors = [
  Int8Array,
  Uint8Array,
  Uint8ClampedArray,
  Int16Array,
  Uint16Array,
  Int32Array,
  Uint32Array,
  Float32Array,
  Float64Array
];

for (var constructor of constructors) {

    assertEq(constructor.prototype.copyWithin.length, 2);

    // works with two arguments
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(0, 3),
                 new constructor([4, 5, 3, 4, 5]));
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(1, 3),
                 new constructor([1, 4, 5, 4, 5]));
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(1, 2),
                 new constructor([1, 3, 4, 5, 5]));
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(2, 2),
                 new constructor([1, 2, 3, 4, 5]));

    // works with three arguments
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(0, 3, 4),
                 new constructor([4, 2, 3, 4, 5]));
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(1, 3, 4),
                 new constructor([1, 4, 3, 4, 5]));
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(1, 2, 4),
                 new constructor([1, 3, 4, 4, 5]));

    // works with negative arguments
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(0, -2),
                 new constructor([4, 5, 3, 4, 5]));
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(0, -2, -1),
                 new constructor([4, 2, 3, 4, 5]));
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(-4, -3, -2),
                 new constructor([1, 3, 3, 4, 5]));
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(-4, -3, -1),
                 new constructor([1, 3, 4, 4, 5]));
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(-4, -3),
                 new constructor([1, 3, 4, 5, 5]));

    // throws on null/undefined values
    assertThrowsInstanceOf(function() {
      constructor.prototype.copyWithin.call(null, 0, 3);
    }, TypeError, "Assert that copyWithin fails if this value is null");

    assertThrowsInstanceOf(function() {
      var throw42 = { valueOf: function() { throw 42; } };
      constructor.prototype.copyWithin.call(null, throw42, throw42, throw42);
    }, TypeError, "Assert that copyWithin fails if this value is null");

    assertThrowsInstanceOf(function() {
      var throw42 = { valueOf: function() { throw 42; } };
      constructor.prototype.copyWithin.call(undefined, throw42, throw42, throw42);
    }, TypeError, "Assert that copyWithin fails if this value is undefined");

    assertThrowsInstanceOf(function() {
      constructor.prototype.copyWithin.call(undefined, 0, 3);
    }, TypeError, "Assert that copyWithin fails if this value is undefined");

    // test with this value as string
    assertThrowsInstanceOf(function() {
      constructor.prototype.copyWithin.call("hello world", 0, 3);
    }, TypeError, "Assert that copyWithin fails if this value is string");

    assertThrowsInstanceOf(function() {
      var throw42 = { valueOf: function() { throw 42; } };
      constructor.prototype.copyWithin.call("hello world", throw42, throw42, throw42);
    }, TypeError, "Assert that copyWithin fails if this value is string");

    // test with target > start on 2 arguments
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(3, 0),
                 new constructor([1, 2, 3, 1, 2]));

    // test with target > start on 3 arguments
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(3, 0, 4),
                 new constructor([1, 2, 3, 1, 2]));

    // test on fractional arguments
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(0.2, 3.9),
                 new constructor([4, 5, 3, 4, 5]));

    // test with -0
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(-0, 3),
                 new constructor([4, 5, 3, 4, 5]));

    // test with arguments more than this.length
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(0, 7),
                 new constructor([1, 2, 3, 4, 5]));

    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(7, 0),
                 new constructor([1, 2, 3, 4, 5]));

    // test with arguments less than -this.length
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(-7, 0),
                 new constructor([1, 2, 3, 4, 5]));

    // test with arguments equal to -this.length
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(-5, 0),
                 new constructor([1, 2, 3, 4, 5]));

    // test on empty constructor
    assertDeepEq(new constructor([]).copyWithin(0, 3),
                 new constructor([]));

    // test with target range being shorter than end - start
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(2, 1, 4),
                 new constructor([1, 2, 2, 3, 4]));

    // test overlapping ranges
    arr = new constructor([1, 2, 3, 4, 5]);
    arr.copyWithin(2, 1, 4);
    assertDeepEq(arr.copyWithin(2, 1, 4),
                 new constructor([1, 2, 2, 2, 3]));

    // undefined as third argument
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(0, 3, undefined),
                 new constructor([4, 5, 3, 4, 5]));

    // test that this.length is never called
    arr = new constructor([0, 1, 2, 3, 5]);
    Object.defineProperty(arr, "length", {
      get: function () { throw new Error("length accessor called"); }
    });
    arr.copyWithin(1, 3);

    var large = 10000;

    // test on a large constructor
    arr = new constructor(large);
    assertDeepEq(arr.copyWithin(45, 900), arr);

    // test on floating point numbers
    for (var i = 0; i < large; i++) {
      arr[i] = Math.random();
    }
    arr.copyWithin(45, 900);

    // test on constructor of objects
    for (var i = 0; i < large; i++) {
      arr[i] = { num: Math.random() };
    }
    arr.copyWithin(45, 900);

    // test constructor length remains same
    assertEq(arr.length, large);

    // test null on third argument is handled correctly
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).copyWithin(0, 3, null),
                 new constructor([1, 2, 3, 4, 5]));

    //Any argument referenced, but not provided, has the value |undefined|
    var tarray = new constructor(8);
    try
    {
      tarray.copyWithin({ valueOf: function() { throw 42; } });
      throw new Error("expected to throw");
    }
    catch (e)
    {
      assertEq(e, 42, "should have failed converting target to index");
    }

    /* // fails, unclear whether it should, disabling for now
    // test with a proxy object
    var handler = {
      get: function(recipient, name) {
          return recipient[name] + 2;
      }
    };

    var p = new Proxy(new constructor([1, 2, 3, 4, 5]), handler);

    assertThrowsInstanceOf(function() {
      constructor.prototype.copyWithin.call(p, 0, 3);
    }, TypeError, "Assert that copyWithin fails if used with a proxy object");
    */
}
