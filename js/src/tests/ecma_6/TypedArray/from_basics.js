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
    // 'from' method is identical for all typed array constructors.
    assertEq(constructors[0].from === constructor.from, true);

    // %TypedArray%.from copies arrays.
    var src = new constructor([1, 2, 3]), copy = constructor.from(src);
    assertEq(copy === src, false);
    assertEq(isSharedConstructor(constructor) || copy instanceof constructor, true);
    assertDeepEq(copy, src);

    // Non-element properties are not copied.
    var a = new constructor([0, 1]);
    a.name = "lisa";
    assertDeepEq(constructor.from(a), new constructor([0, 1]));

    // %TypedArray%.from can copy non-iterable objects, if they're array-like.
    src = {0: 0, 1: 1, length: 2};
    copy = constructor.from(src);
    assertEq(isSharedConstructor(constructor) || copy instanceof constructor, true);
    assertDeepEq(copy, new constructor([0, 1]));

    // Properties past the .length are not copied.
    src = {0: "0", 1: "1", 2: "two", 9: "nine", name: "lisa", length: 2};
    assertDeepEq(constructor.from(src), new constructor([0, 1]));

    // If an object has neither an @@iterator method nor .length,
    // then it's treated as zero-length.
    assertDeepEq(constructor.from({}), new constructor());

    // Primitives will be coerced to primitive wrapper first.
    assertDeepEq(constructor.from(1), new constructor());
    assertDeepEq(constructor.from("123"), new constructor([1, 2, 3]));
    assertDeepEq(constructor.from(true), new constructor());
    assertDeepEq(constructor.from(Symbol()), new constructor());

    // Source object property order doesn't matter.
    src = {length: 2, 1: "1", 0: "0"};
    assertDeepEq(constructor.from(src), new constructor([0, 1]));
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
