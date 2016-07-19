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
    var buf = new constructor();
    detachArrayBuffer(buf.buffer);
    assertThrowsInstanceOf(() => new constructor(buf), TypeError);

    var buffer = new ArrayBuffer();
    detachArrayBuffer(buffer);
    assertThrowsInstanceOf(() => new constructor(buffer), TypeError);
}


if (typeof reportCompare === "function")
    reportCompare(true, true);
