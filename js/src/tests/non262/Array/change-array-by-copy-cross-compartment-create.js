// |reftest|

function test(otherGlobal) {
    let arrays = [
        ["with", otherGlobal.Array.prototype.with.call([1,2,3], 1, 3)],
        ["toSpliced", otherGlobal.Array.prototype.toSpliced.call([1, 2, 3], 0, 1, 4, 5)],
        ["toReversed", otherGlobal.Array.prototype.toReversed.call([1, 2, 3])],
        ["toSorted", otherGlobal.Array.prototype.toSorted.call([1, 2, 3], (x, y) => y > x)]
    ]

    // Test that calling each method in a different compartment returns an array, and that
    // the returned array's prototype matches the other compartment's Array prototype,
    // not this one.
    for (const [name, arr] of arrays) {
        assertEq(arr instanceof Array, false, name + " returned an instance of Array");
        assertEq(arr instanceof otherGlobal.Array, true, name + " did not return an instance of new global's Array");
        assertEq(Object.getPrototypeOf(arr) !== Object.getPrototypeOf([1, 2, 3]), true,
                 name + " returned an object with a prototype from the wrong realm");
    }
}

test(newGlobal());
test(newGlobal({newCompartment: true}));

if (typeof reportCompare === "function")
  reportCompare(0, 0);
