const testArray = [1n];
for (const constructor of anyTypedArrayConstructors) {
    assertThrowsInstanceOf(() => new constructor(testArray), TypeError);
    assertThrowsInstanceOf(() => new constructor(testArray.values()), TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
