// Ensure that we're using [[ArrayLength]] to determine the number of
// values to produce instead of the length property.

function testIterationCount(iterator, expectedLength) {
    for (let i = 0; i < expectedLength; i++)
        assertEq(iterator.next().done, false);
    assertEq(iterator.next().done, true);
}

let i8Array = new Int8Array(4);
Object.defineProperty(i8Array, "length", {value: 0});
let i8Iterator = i8Array[Symbol.iterator]();
testIterationCount(i8Iterator, 4);

// Veryify that the length property isn't even touched
i8Array = new Int8Array();
Object.defineProperty(i8Array, "length", {
    get() {
        throw TypeError;
    }
});
i8Iterator = i8Array[Symbol.iterator]();
testIterationCount(i8Iterator, 0);

// Verify that it works for set as well
i8Array = new Uint8Array(1);
// Try setting a high length which would trigger failure
Object.defineProperty(i8Array, "length", {value: 15});
// Works if the fake length is ignored
(new Uint8Array(4)).set(i8Array, 3);

// Ensure that it works across globals
let g2 = newGlobal();

i8Array = new Int8Array(8);
Object.defineProperty(i8Array, "length", {value: 0});
let iterator = i8Array[Symbol.iterator]();
iterator.next = g2.Array.prototype[Symbol.iterator]().next;
testIterationCount(iterator, 8);

if (typeof reportCompare === "function")
    reportCompare(true, true);
