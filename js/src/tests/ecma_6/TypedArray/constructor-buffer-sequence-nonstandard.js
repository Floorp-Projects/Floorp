// 22.2.4.5 TypedArray ( buffer [ , byteOffset [ , length ] ] )

// Contrary to the ES2017 specification, we don't allow to create a TypedArray
// object with a detached ArrayBuffer. We consider the current specification to
// be incorrect.

const otherGlobal = newGlobal();

function* createBuffers() {
    var lengths = [0, 8];
    for (let length of lengths) {
        let buffer = new ArrayBuffer(length);
        yield {buffer, detach: () => detachArrayBuffer(buffer)};
    }

    for (let length of lengths) {
        let buffer = new otherGlobal.ArrayBuffer(length);
        yield {buffer, detach: () => otherGlobal.detachArrayBuffer(buffer)};
    }
}

// Ensure we handle the case when ToIndex(length) detaches the array buffer.
for (let {buffer, detach} of createBuffers()) {
    let length = {
        valueOf() {
            detach();
            return 0;
        }
    };

    assertThrowsInstanceOf(() => new Int32Array(buffer, 0, length), TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
