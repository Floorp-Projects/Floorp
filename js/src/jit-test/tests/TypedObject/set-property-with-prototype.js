
if (typeof TypedObject === "undefined")
    quit();

// Test the behavior of property sets on typed objects when they are a
// prototype or their prototype has a setter.
var TO = TypedObject;

function assertThrows(fun, errorType) {
  try {
    fun();
    assertEq(true, false, "Expected error, but none was thrown");
  } catch (e) {
    assertEq(e instanceof errorType, true, "Wrong error type thrown");
  }
}

var PointType = new TO.StructType({x: TO.int32, y: TO.int32 });

function testPoint() {
    var p = new PointType();
    var sub = Object.create(p);
    var found;
    Object.defineProperty(PointType.prototype, "z", {set: function(a) { this.d = a; }});
    Object.defineProperty(PointType.prototype, "innocuous", {set: function(a) { found = a; }});

    sub.x = 5;
    assertEq(sub.x, 5);
    assertEq(p.x, 0);

    sub.z = 5;
    assertEq(sub.d, 5);
    assertEq(sub.z, undefined);

    sub[3] = 5;
    assertEq(sub[3], 5);

    p.innocuous = 10;
    assertEq(found, 10);

    assertThrows(function() {
        p.z = 10;
        assertEq(true, false);
    }, TypeError);
}
testPoint();

var IntArrayType = new TO.ArrayType(TO.int32, 3);

function testArray() {
    var arr = new IntArrayType();
    var found;
    Object.defineProperty(IntArrayType.prototype, 5, {set: function(a) { found = a; }});

    assertThrows(function() {
        arr[5] = 5;
    }, RangeError);

    assertThrows(function() {
        arr[4] = 5;
    }, RangeError);

    var p = Object.create(arr);
    p.length = 100;
    assertEq(p.length, 3);

    assertThrows(function() {
        "use strict";
        p.length = 100;
    }, TypeError);
}
testArray();
