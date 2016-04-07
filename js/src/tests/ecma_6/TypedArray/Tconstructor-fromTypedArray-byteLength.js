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

const sharedConstructors = [];

if (typeof SharedArrayBuffer != "undefined")
    sharedConstructors.push(sharedConstructor(Int8Array),
                            sharedConstructor(Uint8Array),
                            sharedConstructor(Int16Array),
                            sharedConstructor(Uint16Array),
                            sharedConstructor(Int32Array),
                            sharedConstructor(Uint32Array),
                            sharedConstructor(Float32Array),
                            sharedConstructor(Float64Array));

var g = newGlobal();

var arr = [1, 2, 3];
for (var constructor of constructors.concat(sharedConstructors)) {
    var tarr = new constructor(arr);
    for (var constructor2 of constructors) {
        var copied = new constructor2(tarr);
        assertEq(copied.buffer.byteLength, arr.length * constructor2.BYTES_PER_ELEMENT);

        g.tarr = tarr;
        copied = g.eval(`new ${constructor2.name}(tarr);`);
        assertEq(copied.buffer.byteLength, arr.length * constructor2.BYTES_PER_ELEMENT);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
