const constructors = [
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array ];

if (typeof SharedArrayBuffer != "undefined")
    constructors.push(sharedConstructor(Int8Array),
		      sharedConstructor(Uint8Array),
		      sharedConstructor(Int16Array),
		      sharedConstructor(Uint16Array),
		      sharedConstructor(Int32Array),
		      sharedConstructor(Uint32Array),
		      sharedConstructor(Float32Array),
		      sharedConstructor(Float64Array));

for (var constructor of constructors) {
    // Check superficial features of %TypeArray%.from.
    var desc = Object.getOwnPropertyDescriptor(constructor.__proto__, "from");
    if (!isSharedConstructor(constructor)) {
	assertEq(desc.configurable, true);
	assertEq(desc.enumerable, false);
	assertEq(desc.writable, true);
    }
    assertEq(constructor.from.length, 1);
    assertThrowsInstanceOf(() => new constructor.from(), TypeError);  // not a constructor
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
