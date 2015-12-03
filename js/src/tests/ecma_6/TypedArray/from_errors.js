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
    // %TypedArray%.from throws if the argument is undefined or null.
    assertThrowsInstanceOf(() => constructor.from(), TypeError);
    assertThrowsInstanceOf(() => constructor.from(undefined), TypeError);
    assertThrowsInstanceOf(() => constructor.from(null), TypeError);

    // %TypedArray%.from throws if an element can't be defined on the new object.
    function ObjectWithReadOnlyElement() {
        Object.defineProperty(this, "0", {value: null});
        this.length = 0;
    }
    ObjectWithReadOnlyElement.from = constructor.from;
    assertDeepEq(ObjectWithReadOnlyElement.from([]), new ObjectWithReadOnlyElement);
    assertThrowsInstanceOf(() => ObjectWithReadOnlyElement.from([1]), TypeError);

    // The same, but via preventExtensions.
    function InextensibleObject() {
        Object.preventExtensions(this);
    }
    InextensibleObject.from = constructor.from;
    assertThrowsInstanceOf(() => InextensibleObject.from([1]), TypeError);

    // The same, but via a readonly property on its __proto__.
    function ObjectWithReadOnlyElementOnProto() {
        return Object.create({
            get 0(){}
        });
    }
    ObjectWithReadOnlyElementOnProto.from = constructor.from;
    assertThrowsInstanceOf(() => ObjectWithReadOnlyElementOnProto.from([1]), TypeError);

    // Unlike Array.from, %TypedArray%.from doesn't get or set the length property.
    function ObjectWithThrowingLengthGetterSetter() {
        Object.defineProperty(this, "length", {
            configurable: true,
            get() { throw new RangeError("getter!"); },
            set() { throw new RangeError("setter!"); }
        });
    }
    ObjectWithThrowingLengthGetterSetter.from = constructor.from;
    assertEq(ObjectWithThrowingLengthGetterSetter.from(["foo"])[0], "foo");

    // %TypedArray%.from throws if mapfn is neither callable nor undefined.
    assertThrowsInstanceOf(() => constructor.from([3, 4, 5], {}), TypeError);
    assertThrowsInstanceOf(() => constructor.from([3, 4, 5], "also not a function"), TypeError);
    assertThrowsInstanceOf(() => constructor.from([3, 4, 5], null), TypeError);

    // Even if the function would not have been called.
    assertThrowsInstanceOf(() => constructor.from([], JSON), TypeError);

    // If mapfn is not undefined and not callable, the error happens before anything else.
    // Before calling the constructor, before touching the arrayLike.
    var log = "";
    function C() {
        log += "C";
        obj = this;
    }
    var p = new Proxy({}, {
        has: function () { log += "1"; },
        get: function () { log += "2"; },
        getOwnPropertyDescriptor: function () { log += "3"; }
    });
    assertThrowsInstanceOf(() => constructor.from.call(C, p, {}), TypeError);
    assertEq(log, "");

    // If mapfn throws, the new object has already been created.
    var arrayish = {
        get length() { log += "l"; return 1; },
        get 0() { log += "0"; return "q"; }
    };
    log = "";
    var exc = {surprise: "ponies"};
    assertThrowsValue(() => constructor.from.call(C, arrayish, () => { throw exc; }), exc);
    assertEq(log, "lC0");
    assertEq(obj instanceof C, true);

    // It's a TypeError if the @@iterator property is a primitive (except null and undefined).
    for (var primitive of ["foo", 17, Symbol(), true]) {
        assertThrowsInstanceOf(() => constructor.from({[Symbol.iterator] : primitive}), TypeError);
    }
    assertDeepEq(constructor.from({[Symbol.iterator]: null}), new constructor());
    assertDeepEq(constructor.from({[Symbol.iterator]: undefined}), new constructor());

    // It's a TypeError if the iterator's .next() method returns a primitive.
    for (var primitive of [undefined, null, "foo", 17, Symbol(), true]) {
        assertThrowsInstanceOf(
            () => constructor.from({
                [Symbol.iterator]() {
                    return {next() { return primitive; }};
                }
            }),
        TypeError);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
