// Test that we can't confuse %ArrayIteratorPrototype% for an
// ArrayIterator object.
function TestArrayIteratorPrototypeConfusion() {
    var iter = [][Symbol.iterator]();
    try {
        iter.next.call(Object.getPrototypeOf(iter))
        throw new Error("Call did not throw");
    } catch (e) {
        assertEq(e instanceof TypeError, true);
        assertEq(e.message, "next method called on incompatible Array Iterator");
    }
}
TestArrayIteratorPrototypeConfusion();

// Tests that we can use %ArrayIteratorPrototype%.next on a
// cross-compartment iterator.
function TestArrayIteratorWrappers() {
    var iter = [][Symbol.iterator]();
    assertDeepEq(iter.next.call(newGlobal().eval('[5][Symbol.iterator]()')),
		 { value: 5, done: false })
}
if (typeof newGlobal === "function") {
    TestArrayIteratorWrappers();
}

// Tests that calling |next| on an array iterator after iteration has finished
// doesn't get the array's |length| property.
function TestIteratorNextGetLength() {
  var lengthCalledTimes = 0;
  var array = {
    __proto__: Array.prototype,
    get length() {
      lengthCalledTimes += 1;
      return {
        valueOf() {
          return 0;
        }
      };
    }
  };
  var it = array[Symbol.iterator]();
  it.next();
  it.next();
  assertEq(1, lengthCalledTimes);
}
TestIteratorNextGetLength();


if (typeof reportCompare === "function")
  reportCompare(true, true);
