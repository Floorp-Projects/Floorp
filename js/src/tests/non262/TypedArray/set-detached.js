// Tests for detached ArrayBuffer checks in %TypedArray%.prototype.set(array|typedArray, offset).

function* createTypedArrays(lengths = [0, 1, 4, 4096]) {
    for (let length of lengths) {
        let buffer = new ArrayBuffer(length * Int32Array.BYTES_PER_ELEMENT);
        let typedArray = new Int32Array(buffer);

        yield {typedArray, buffer};
    }
}

if (typeof detachArrayBuffer === "function") {
    class ExpectedError extends Error {}

    // No detached check on function entry.
    for (let {typedArray, buffer} of createTypedArrays()) {
        detachArrayBuffer(buffer);

        assertThrowsInstanceOf(() => typedArray.set(null, {
            valueOf() {
                throw new ExpectedError();
            }
        }), ExpectedError);
    }

    // Check for detached buffer after calling ToInteger(offset). Test with:
    // - valid offset,
    // - too large offset,
    // - and negative offset.
    for (let [offset, error] of [[0, TypeError], [1000000, TypeError], [-1, RangeError]]) {
        for (let source of [[], [0], new Int32Array(0), new Int32Array(1)]) {
            for (let {typedArray, buffer} of createTypedArrays()) {
                assertThrowsInstanceOf(() => typedArray.set(source, {
                    valueOf() {
                        detachArrayBuffer(buffer);
                        return offset;
                    }
                }), error);
            }
        }
    }

    // Tests when called with detached typed array as source.
    for (let {typedArray} of createTypedArrays()) {
        for (let {typedArray: source, buffer: sourceBuffer} of createTypedArrays()) {
            detachArrayBuffer(sourceBuffer);

            assertThrowsInstanceOf(() => typedArray.set(source, {
                valueOf() {
                    throw new ExpectedError();
                }
            }), ExpectedError);
        }
    }

    // Check when detaching source buffer in ToInteger(offset). Test with:
    // - valid offset,
    // - too large offset,
    // - and negative offset.
    for (let [offset, error] of [[0, TypeError], [1000000, TypeError], [-1, RangeError]]) {
        for (let {typedArray} of createTypedArrays()) {
            for (let {typedArray: source, buffer: sourceBuffer} of createTypedArrays()) {
                assertThrowsInstanceOf(() => typedArray.set(source, {
                    valueOf() {
                        detachArrayBuffer(sourceBuffer);
                        return offset;
                    }
                }), error);
            }
        }
    }

    // Test when target and source use the same underlying buffer and
    // ToInteger(offset) detaches the buffer. Test with:
    // - same typed array,
    // - different typed array, but same element type,
    // - and different element type.
    for (let src of [ta => ta, ta => new Int32Array(ta.buffer), ta => new Float32Array(ta.buffer)]) {
        for (let {typedArray, buffer} of createTypedArrays()) {
            let source = src(typedArray);
            assertThrowsInstanceOf(() => typedArray.set(source, {
                valueOf() {
                    detachArrayBuffer(buffer);
                    return 0;
                }
            }), TypeError);
        }
    }

    // Test when Get(src, "length") detaches the buffer, but srcLength is 0.
    // Also use different offsets to ensure bounds checks use the typed array's
    // length value from before detaching the buffer.
    for (let offset of [() => 0, ta => Math.min(1, ta.length), ta => Math.max(0, ta.length - 1)]) {
        for (let {typedArray, buffer} of createTypedArrays()) {
            let source = {
                get length() {
                    detachArrayBuffer(buffer);
                    return 0;
                }
            };
            typedArray.set(source, offset(typedArray));
        }
    }

    // Test when ToLength(Get(src, "length")) detaches the buffer, but
    // srcLength is 0. Also use different offsets to ensure bounds checks use
    // the typed array's length value from before detaching the buffer.
    for (let offset of [() => 0, ta => Math.min(1, ta.length), ta => Math.max(0, ta.length - 1)]) {
        for (let {typedArray, buffer} of createTypedArrays()) {
            let source = {
                length: {
                    valueOf() {
                        detachArrayBuffer(buffer);
                        return 0;
                    }
                }
            };
            typedArray.set(source, offset(typedArray));
        }
    }

    // Test a TypeError is thrown when the typed array is detached and
    // srcLength > 0.
    for (let {typedArray, buffer} of createTypedArrays()) {
        let source = {
            length: {
                valueOf() {
                    detachArrayBuffer(buffer);
                    return 1;
                }
            }
        };
        let err = typedArray.length === 0 ? RangeError : TypeError;
        assertThrowsInstanceOf(() => typedArray.set(source), err);
    }

    // Same as above, but with side-effect when executing Get(src, "0").
    for (let {typedArray, buffer} of createTypedArrays()) {
        let source = {
            get 0() {
                throw new ExpectedError();
            },
            length: {
                valueOf() {
                    detachArrayBuffer(buffer);
                    return 1;
                }
            }
        };
        let err = typedArray.length === 0 ? RangeError : ExpectedError;
        assertThrowsInstanceOf(() => typedArray.set(source), err);
    }

    // Same as above, but with side-effect when executing ToNumber(Get(src, "0")).
    for (let {typedArray, buffer} of createTypedArrays()) {
        let source = {
            get 0() {
                return {
                    valueOf() {
                        throw new ExpectedError();
                    }
                };
            },
            length: {
                valueOf() {
                    detachArrayBuffer(buffer);
                    return 1;
                }
            }
        };
        let err = typedArray.length === 0 ? RangeError : ExpectedError;
        assertThrowsInstanceOf(() => typedArray.set(source), err);
    }

    // Side-effects when getting the source elements detach the buffer.
    for (let {typedArray, buffer} of createTypedArrays()) {
        let source = Object.defineProperties([], {
            0: {
                get() {
                    detachArrayBuffer(buffer);
                    return 1;
                }
            }
        });
        let err = typedArray.length === 0 ? RangeError : TypeError;
        assertThrowsInstanceOf(() => typedArray.set(source), err);
    }

    // Side-effects when getting the source elements detach the buffer. Also
    // ensure other elements aren't accessed.
    for (let {typedArray, buffer} of createTypedArrays()) {
        let source = Object.defineProperties([], {
            0: {
                get() {
                    detachArrayBuffer(buffer);
                    return 1;
                }
            },
            1: {
                get() {
                    throw new Error("Unexpected access");
                }
            }
        });
        let err = typedArray.length <= 1 ? RangeError : TypeError;
        assertThrowsInstanceOf(() => typedArray.set(source), err);
    }

    // Side-effects when converting the source elements detach the buffer.
    for (let {typedArray, buffer} of createTypedArrays()) {
        let source = [{
            valueOf() {
                detachArrayBuffer(buffer);
                return 1;
            }
        }];
        let err = typedArray.length === 0 ? RangeError : TypeError;
        assertThrowsInstanceOf(() => typedArray.set(source), err);
    }

    // Side-effects when converting the source elements detach the buffer. Also
    // ensure other elements aren't accessed.
    for (let {typedArray, buffer} of createTypedArrays()) {
        let source = [{
            valueOf() {
                detachArrayBuffer(buffer);
                return 1;
            }
        }, {
            valueOf() {
                throw new Error("Unexpected access");
            }
        }];
        let err = typedArray.length <= 1 ? RangeError : TypeError;
        assertThrowsInstanceOf(() => typedArray.set(source), err);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
