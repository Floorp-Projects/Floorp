// |reftest| skip-if(!xulRuntime.shell)
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// This file is a copy of clone-typed-array.js from before v2 structured clone
// was implemented. If you run this test under a v1-writing engine with the
// environment variable JS_RECORD_RESULTS set, then it will output a log of
// structured clone buffers resulting from running this test. You can then use
// that log as input to another run of this same test on a newer engine, to
// verify that older-format structured clone data can be deserialized properly.

var old_serialize = serialize;
var captured = [];

if (os.getenv("JS_RECORD_RESULTS") !== undefined) {
  serialize = function(o) {
    var data;
    try {
      data = old_serialize(o);
      captured.push(data);
      return data;
    } catch(e) {
      captured.push(e);
      throw(e);
    }
  };
} else {
  loadRelativeToScript("clone-v1-typed-array-data.dat");
  serialize = function(d) {
    var data = captured.shift();
    if (data instanceof Error)
      throw(data);
    else
      return data;
  };
}

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
    check(new ArrayBuffer(0));
    check(new ArrayBuffer(7));
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
        b = new ctor(0);
        check(b);

        // check array with some elements
        b = new ctor(100);
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

    var base = new Int8Array([0, 1, 2, 3]);
    b = [new Int8Array(base.buffer, 0, 3), new Int8Array(base.buffer, 1, 3)];
    var a = deserialize(serialize(b));
    base[1] = -1;
    a[0][2] = -2;
    assertArraysEqual(b[0], new Int8Array([0, -1, 2])); // shared with base
    assertArraysEqual(b[1], new Int8Array([-1, 2, 3])); // shared with base
    assertArraysEqual(a[0], new Int8Array([0, 1, -2])); // not shared with base
    assertArraysEqual(a[1], new Int8Array([1, 2, 3]));  // not shared with base or a[0]
}

test();
reportCompare(0, 0, 'ok');

if (os.getenv("JS_RECORD_RESULTS") !== undefined) {
  print("var captured = [];");
  for (var i in captured) {
    var s = "captured[" + i + "] = ";
    if (captured[i] instanceof Error) {
      print(s + captured[i].toString() + ";");
    } else {
      data = captured[i].clonebuffer.split('').map(c => c.charCodeAt(0));
      print(s + "serialize(0); captured[" + i + "].clonebuffer = String.fromCharCode(" + data.join(", ") + ");");
    }
  }
}
