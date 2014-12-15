// Test that we can't confuse %ArrayIteratorPrototype% for an
// ArrayIterator object.
function TestArrayIteratorPrototypeConfusion() {
    var iter = [][Symbol.iterator]();
    try {
        iter.next.call(Object.getPrototypeOf(iter))
        throw new Error("Call did not throw");
    } catch (e) {
        assertEq(e instanceof TypeError, true);
        assertEq(e.message, "ArrayIterator next called on incompatible [object Array Iterator]");
    }
}
TestArrayIteratorPrototypeConfusion();

if (typeof reportCompare === "function")
  reportCompare(true, true);
