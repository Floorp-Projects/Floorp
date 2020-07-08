// |jit-test| skip-if: !this.TypedObject; --enable-private-fields;
// More complicated IC testing.

class Base {
  constructor(o) {
    return o;
  }
}

class B extends Base {
  #x = 12;
  static gx(o) {
    return o.#x;
  }
  static sx(o) {
    o.#x++;
  }
}

function assertThrows(fun, errorType) {
  try {
    fun();
    throw 'Expected error, but none was thrown';
  } catch (e) {
    if (!(e instanceof errorType)) {
      throw 'Wrong error type thrown ' + e.name + ' ' + e.message;
    }
  }
}

var OneDPoint = new TypedObject.StructType({x: TypedObject.uint8});
var point = new OneDPoint;

var objects = [{}, {}, {}, {}, point, {}, point];
// The points with x in them will throw.
var throw_results = objects.map((value) => 'x' in value);

function test(objects, throws) {
  function testThing(thing, throws) {
    // Not yet stamped.
    assertThrows(() => B.gx(thing), TypeError);
    assertThrows(() => B.sx(thing), TypeError);
    var threw = true;
    var error = undefined;
    // Stamp;
    try {
      new B(thing);
      threw = false;
    } catch (e) {
      threw = true;
      error = e;
    }
    if (throws) {
      assertEq(threw, true);
      assertEq(error instanceof TypeError, true);
    } else {
      assertEq(threw, false);
      assertEq(error, undefined);
      assertEq(B.gx(thing), 12);
      B.sx(thing);
      assertEq(B.gx(thing), 13);
    }
  }

  assertEq(objects.length, throws.length)
  for (i in objects) {
    testThing(objects[i], throws[i]);
  }
}

test(objects, throw_results)