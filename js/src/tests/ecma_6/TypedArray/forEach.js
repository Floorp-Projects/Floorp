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

// Tests for TypedArray#forEach
for (var constructor of constructors) {
    assertEq(constructor.prototype.forEach.length, 1);

    var arr = new constructor([1, 2, 3, 4, 5]);
    // Tests for `thisArg` argument.
    function assertThisArg(thisArg, thisValue) {
        // In sloppy mode, `this` could be global object or a wrapper of `thisArg`.
        arr.forEach(function() {
            assertDeepEq(this, thisValue);
            return false;
        }, thisArg);

        // In strict mode, `this` strictly equals `thisArg`.
        arr.forEach(function() {
            "use strict";
            assertDeepEq(this, thisArg);
            return false;
        }, thisArg);

        // Passing `thisArg` has no effect if callback is an arrow function.
        var self = this;
        arr.forEach(() => {
            assertEq(this, self);
            return false;
        }, thisArg);
    }
    assertThisArg([1, 2, 3], [1, 2, 3]);
    assertThisArg(Object, Object);
    assertThisArg(1, Object(1));
    assertThisArg("1", Object("1"));
    assertThisArg(false, Object(false));
    assertThisArg(undefined, this);
    assertThisArg(null, this);

    // Throw an exception in the callback.
    var sum = 0;
    var count = 0;
    var thrown = false;
    try {
        assertEq(arr.forEach((v) => {
            count++;
            sum += v;
            if (v === 3) {
                throw "forEach";
            }
        }), undefined)
    } catch(e) {
        assertEq(e, "forEach");
        thrown = true;
    }
    assertEq(thrown, true);
    assertEq(sum, 6);
    assertEq(count, 3);

    // There is no callback or callback is not a function.
    assertThrowsInstanceOf(() => {
        arr.forEach();
    }, TypeError);
    var invalidCallbacks = [undefined, null, 1, false, "", Symbol(), [], {}, /./];
    invalidCallbacks.forEach(callback => {
        assertThrowsInstanceOf(() => {
            arr.forEach(callback);
        }, TypeError);
    })

    // Callback is a generator.
    arr.forEach(function*(){
        throw "This line will not be executed";
    });

    // Called from other globals.
    if (typeof newGlobal === "function" && !isSharedConstructor(constructor)) {
        var forEach = newGlobal()[constructor.name].prototype.forEach;
        var sum = 0;
        forEach.call(new constructor([1, 2, 3]), v => {
            sum += v;
        });
        assertEq(sum, 6);
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
			    new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.forEach.call(invalidReceiver, () => true);
        }, TypeError, "Assert that some fails if this value is not a TypedArray");
    });
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
