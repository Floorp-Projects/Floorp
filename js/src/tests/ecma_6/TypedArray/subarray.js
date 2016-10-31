// Bug 1291003
if (typeof detachArrayBuffer === "function") {
    for (let constructor of typedArrayConstructors) {
        const elementSize = constructor.BYTES_PER_ELEMENT;

        let targetOffset;
        let buffer = new ArrayBuffer(2 * elementSize);
        let typedArray = new constructor(buffer, 1 * elementSize, 1);
        typedArray.constructor = {
            [Symbol.species]: function(ab, offset, length) {
                targetOffset = offset;
                return new constructor(1);
            }
        };

        let beginIndex = {
            valueOf() {
                detachArrayBuffer(buffer);
                return 0;
            }
        };
        typedArray.subarray(beginIndex);

        assertEq(targetOffset, 1 * elementSize);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
