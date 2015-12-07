// |reftest| skip-if(!xulRuntime.shell)

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
    for (var neuterType of ["change-data", "same-data"]) {
        var buf = new constructor();
        neuter(buf.buffer, neuterType);
        assertThrowsInstanceOf(()=> new constructor(buf), TypeError);

        var buffer = new ArrayBuffer();
        neuter(buffer, neuterType);
        assertThrowsInstanceOf(()=> new constructor(buffer), TypeError);
    }
}


if (typeof reportCompare === "function")
    reportCompare(true, true);
