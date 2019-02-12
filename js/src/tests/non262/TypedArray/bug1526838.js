// |reftest| skip-if(!this.hasOwnProperty("BigInt"))

const testArray = [1n];
for (const constructor of anyTypedArrayConstructors) {
    assertThrows(() => new constructor(testArray), TypeError);
    assertThrows(() => new constructor(testArray.values()), TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
