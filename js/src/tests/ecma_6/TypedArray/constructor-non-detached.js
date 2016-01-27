// |reftest| skip-if(!xulRuntime.shell) -- needs detachArrayBuffer

const constructors = [
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array
];

for (var constructor of constructors) {
    for (var detachType of ["change-data", "same-data"]) {
        var buf = new constructor();
        detachArrayBuffer(buf.buffer, detachType);
        assertThrowsInstanceOf(() => new constructor(buf), TypeError);

        var buffer = new ArrayBuffer();
        detachArrayBuffer(buffer, detachType);
        assertThrowsInstanceOf(() => new constructor(buffer), TypeError);
    }
}


if (typeof reportCompare === "function")
    reportCompare(true, true);
