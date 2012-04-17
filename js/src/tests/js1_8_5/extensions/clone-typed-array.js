// |reftest| skip-if(!xulRuntime.shell)
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function assertArraysEqual(a, b) {
    assertEq(a.constructor, b.constructor);
    assertEq(a.length, b.length);
    for (var i = 0; i < a.length; i++)
        assertEq(a[i], b[i]);
}

function check(b) {
    var a = deserialize(serialize(b));
    assertArraysEqual(a, b);
}

function checkPrototype(ctor) {
    var threw = false;
    try {
	serialize(ctor.prototype);
	throw new Error("serializing " + ctor.name + ".prototype should throw a TypeError");
    } catch (exc) {
	if (!(exc instanceof TypeError))
	    throw exc;
    }
}

function test() {
    // Test cloning ArrayBuffer objects.
    check(ArrayBuffer(0));
    check(ArrayBuffer(7));
    checkPrototype(ArrayBuffer);

    // Test cloning typed array objects.
    var ctors = [
        Int8Array,
        Uint8Array,
        Int16Array,
        Uint16Array,
        Int32Array,
        Uint32Array,
        Float32Array,
        Float64Array,
        Uint8ClampedArray];

    var b;
    for (var i = 0; i < ctors.length; i++) {
        var ctor = ctors[i];

        // check empty array
        b = ctor(0);
        check(b);

        // check array with some elements
        b = ctor(100);
        var v = 1;
        for (var j = 0; j < 100; j++) {
            b[j] = v;
            v *= 7;
        }
        b[99] = NaN; // check serializing NaNs too
        check(b);

	// try the prototype
	checkPrototype(ctor);
    }

    // Cloning should separately copy two TypedArrays backed by the same
    // ArrayBuffer. This also tests cloning TypedArrays where the arr->data
    // pointer is not 8-byte-aligned.

    var base = Int8Array([0, 1, 2, 3]);
    b = [Int8Array(base.buffer, 0, 3), Int8Array(base.buffer, 1, 3)];
    var a = deserialize(serialize(b));
    base[1] = -1;
    a[0][2] = -2;
    assertArraysEqual(b[0], Int8Array([0, -1, 2])); // shared with base
    assertArraysEqual(b[1], Int8Array([-1, 2, 3])); // shared with base
    assertArraysEqual(a[0], Int8Array([0, 1, -2])); // not shared with base
    assertArraysEqual(a[1], Int8Array([1, 2, 3]));  // not shared with base or a[0]
}

test();
reportCompare(0, 0, 'ok');
