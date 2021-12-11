// Case 1: splice() removes an element from the array.
{
    let array = [];
    array.push(0, 1, 2);

    array.constructor = {
        [Symbol.species]: function(n) {
            // Increase the initialized length of the array.
            array.push(3, 4, 5);

            // Make the length property non-writable.
            Object.defineProperty(array, "length", {writable: false});

            return new Array(n);
        }
    }

    assertThrowsInstanceOf(() => Array.prototype.splice.call(array, 0, 1), TypeError);

    assertEq(array.length, 6);
    assertEqArray(array, [1, 2, /* hole */, 3, 4, 5]);
}

// Case 2: splice() adds an element to the array.
{
    let array = [];
    array.push(0, 1, 2);

    array.constructor = {
        [Symbol.species]: function(n) {
            // Increase the initialized length of the array.
            array.push(3, 4, 5);

            // Make the length property non-writable.
            Object.defineProperty(array, "length", {writable: false});

            return new Array(n);
        }
    }

    assertThrowsInstanceOf(() => Array.prototype.splice.call(array, 0, 0, 123), TypeError);

    assertEq(array.length, 6);
    assertEqArray(array, [123, 0, 1, 2, 4, 5]);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
