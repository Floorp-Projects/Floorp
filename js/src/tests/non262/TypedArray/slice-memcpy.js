const otherGlobal = newGlobal();

// Create with new ArrayBuffer and offset.
for (var constructor of typedArrayConstructors) {
    const elementSize = constructor.BYTES_PER_ELEMENT;
    let ab = new constructor(10).map((v, i) => i).buffer;
    let ta = new constructor(ab, 2 * elementSize, 6);
    ta.constructor = {
        [Symbol.species]: function(len) {
            return new constructor(new ArrayBuffer((len + 4) * elementSize), 4 * elementSize);
        }
    };
    let tb = ta.slice(0);

    assertEqArray(ta, [2, 3, 4, 5, 6, 7]);
    assertEqArray(tb, [2, 3, 4, 5, 6, 7]);
}

// Source and target arrays use the same ArrayBuffer, target range before source range.
for (var constructor of typedArrayConstructors) {
    const elementSize = constructor.BYTES_PER_ELEMENT;
    let ab = new constructor(10).map((v, i) => i).buffer;
    let ta = new constructor(ab, 2 * elementSize, 6);
    ta.constructor = {
        [Symbol.species]: function(len) {
            return new constructor(ab, 0, len);
        }
    };
    let tb = ta.slice(0);

    assertEqArray(ta, [4, 5, 6, 7, 6, 7]);
    assertEqArray(tb, [2, 3, 4, 5, 6, 7]);
    assertEqArray(new constructor(ab), [2, 3, 4, 5, 6, 7, 6, 7, 8, 9]);
}

// Source and target arrays use the same ArrayBuffer, target range after source range.
for (var constructor of typedArrayConstructors) {
    const elementSize = constructor.BYTES_PER_ELEMENT;
    let ab = new constructor(10).map((v, i) => i + 1).buffer;
    let ta = new constructor(ab, 0, 6);
    ta.constructor = {
        [Symbol.species]: function(len) {
            return new constructor(ab, 2 * elementSize, len);
        }
    };
    let tb = ta.slice(0);

    assertEqArray(ta, [1, 2, 1, 2, 1, 2]);
    assertEqArray(tb, [1, 2, 1, 2, 1, 2]);
    assertEqArray(new constructor(ab), [1, 2, 1, 2, 1, 2, 1, 2, 9, 10]);
}

// Tricky: Same as above, but with SharedArrayBuffer and different compartments.
if (typeof setSharedArrayBuffer === "function") {
    for (var constructor of typedArrayConstructors) {
        const elementSize = constructor.BYTES_PER_ELEMENT;
        let ts = new constructor(new SharedArrayBuffer(10 * elementSize));
        for (let i = 0; i < ts.length; i++) {
            ts[i] = i + 1;
        }
        let ab = ts.buffer;
        let ta = new constructor(ab, 0, 6);
        ta.constructor = {
            [Symbol.species]: function(len) {
                setSharedArrayBuffer(ab);
                try {
                    return otherGlobal.eval(`
                        new ${constructor.name}(getSharedArrayBuffer(), ${2 * elementSize}, ${len});
                    `);
                } finally {
                    setSharedArrayBuffer(null);
                }
            }
        };
        let tb = ta.slice(0);

        assertEqArray(ta, [1, 2, 1, 2, 1, 2]);
        assertEqArray(tb, [1, 2, 1, 2, 1, 2]);
        assertEqArray(new constructor(ab), [1, 2, 1, 2, 1, 2, 1, 2, 9, 10]);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
