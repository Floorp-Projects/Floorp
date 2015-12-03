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
    // %TypedArray%.from works on arguments objects.
    (function () {
        assertDeepEq(constructor.from(arguments), new constructor(["0", "1", undefined]));
    })("0", "1", undefined);

    // If an object has both .length and [@@iterator] properties, [@@iterator] is used.
    var a = ['0', '1', '2', '3', '4'];
    a[Symbol.iterator] = function* () {
        for (var i = 5; i--; )
            yield this[i];
    };

    var log = '';
    function f(x) {
        log += x;
        return x + x;
    }

    var b = constructor.from(a, f);
    assertDeepEq(b, new constructor(['44', '33', '22', '11', '00']));
    assertEq(log, '43210');

    // In fact, if [@@iterator] is present, .length isn't queried at all.
    var pa = new Proxy(a, {
        has: function (target, id) {
            if (id === "length")
                throw new Error(".length should not be queried (has)");
            return id in target;
        },
        get: function (target, id) {
            if (id === "length")
                throw new Error(".length should not be queried (get)");
            return target[id];
        },
        getOwnPropertyDescriptor: function (target, id) {
            if (id === "length")
                throw new Error(".length should not be queried (getOwnPropertyDescriptor)");
            return Object.getOwnPropertyDescriptor(target, id)
        }
    });
    log = "";
    b = constructor.from(pa, f);
    assertDeepEq(b, new constructor(['44', '33', '22', '11', '00']));
    assertEq(log, '43210');
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
