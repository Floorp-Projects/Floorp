function assertSameEntries(actual, expected) {
    assertEq(actual.length, expected.length);
    for (let i = 0; i < expected.length; ++i)
        assertEqArray(actual[i], expected[i]);
}

// Test Object.keys/values/entries on objects with indexed properties.

// Empty dense elements, test with array and plain object.
{
    let array = [];
    assertEqArray(Object.keys(array), []);
    assertEqArray(Object.values(array), []);
    assertSameEntries(Object.entries(array), []);

    let object = {};
    assertEqArray(Object.keys(object), []);
    assertEqArray(Object.values(object), []);
    assertSameEntries(Object.entries(object), []);
}

// Dense elements only, test with array and plain object.
{
    let array = [1, 2, 3];
    assertEqArray(Object.keys(array), ["0", "1", "2"]);
    assertEqArray(Object.values(array), [1, 2, 3]);
    assertSameEntries(Object.entries(array), [["0", 1], ["1", 2], ["2", 3]]);

    let object = {0: 4, 1: 5, 2: 6};
    assertEqArray(Object.keys(object), ["0", "1", "2"]);
    assertEqArray(Object.values(object), [4, 5, 6]);
    assertSameEntries(Object.entries(object), [["0", 4], ["1", 5], ["2", 6]]);
}

// Extra indexed properties only, test with array and plain object.
{
    let array = [];
    Object.defineProperty(array, 0, {configurable: true, enumerable: true, value: 123});

    assertEqArray(Object.keys(array), ["0"]);
    assertEqArray(Object.values(array), [123]);
    assertSameEntries(Object.entries(array), [["0", 123]]);

    let object = [];
    Object.defineProperty(object, 0, {configurable: true, enumerable: true, value: 123});

    assertEqArray(Object.keys(object), ["0"]);
    assertEqArray(Object.values(object), [123]);
    assertSameEntries(Object.entries(object), [["0", 123]]);
}

// Dense and extra indexed properties, test with array and plain object.
{
    let array = [1, 2, 3];
    Object.defineProperty(array, 0, {writable: false});

    assertEqArray(Object.keys(array), ["0", "1", "2"]);
    assertEqArray(Object.values(array), [1, 2, 3]);
    assertSameEntries(Object.entries(array), [["0", 1], ["1", 2], ["2", 3]]);

    let object = {0: 4, 1: 5, 2: 6};
    Object.defineProperty(object, 0, {writable: false});

    assertEqArray(Object.keys(object), ["0", "1", "2"]);
    assertEqArray(Object.values(object), [4, 5, 6]);
    assertSameEntries(Object.entries(object), [["0", 4], ["1", 5], ["2", 6]]);
}


if (typeof reportCompare === "function")
    reportCompare(true, true);
