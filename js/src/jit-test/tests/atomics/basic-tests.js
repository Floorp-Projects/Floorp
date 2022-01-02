// Basic functional tests for the Atomics primitives.
//
// These do not test atomicity, just that calling and coercions and
// indexing and exception behavior all work right.
//
// These do not test the wait/wake operations.

load(libdir + "asserts.js");

var DEBUG = false;		// Set to true for useful printouts

function dprint(...xs) {
    if (!DEBUG)
	return;
    var s = "";
    for ( var x in xs )
	s += String(xs[x]);
    print(s);
}

// Clone a function so that we get reliable inlining of primitives with --ion-eager.
// For eg testMethod and testFunction that are polymorphic in the array a,
// the inliner gets confused and stops inlining after Int8 -- not what we want.
function CLONE(f) {
    return this.eval("(" + f.toString() + ")");
}

function testMethod(a, ...indices) {
    dprint("Method: " + a.constructor.name);
    var poison;
    switch (a.BYTES_PER_ELEMENT) {
    case 1: poison = 0x5A; break;
    case 2: poison = 0x5A5A; break;
    case 4: poison = 0x5A5A5A5A; break;
    }
    for ( var i=0 ; i < indices.length ; i++ ) {
	var x = indices[i];
	if (x > 0)
	    a[x-1] = poison;
	if (x < a.length-1)
	    a[x+1] = poison;

	// val = 0
	assertEq(Atomics.compareExchange(a, x, 0, 37), 0);
	// val = 37
	assertEq(Atomics.compareExchange(a, x, 37, 5), 37);
	// val = 5
	assertEq(Atomics.compareExchange(a, x, 7, 8), 5); // ie should fail
	// val = 5
	assertEq(Atomics.compareExchange(a, x, 5, 9), 5);
	// val = 9
	assertEq(Atomics.compareExchange(a, x, 5, 0), 9); // should also fail

 	// val = 9
	assertEq(Atomics.exchange(a, x, 4), 9);
	// val = 4
	assertEq(Atomics.exchange(a, x, 9), 4);

	// val = 9
	assertEq(Atomics.load(a, x), 9);
	// val = 9
	assertEq(Atomics.store(a, x, 14), 14); // What about coercion?
	// val = 14
	assertEq(Atomics.load(a, x), 14);
	// val = 14
	Atomics.store(a, x, 0);
	// val = 0

	// val = 0
	assertEq(Atomics.add(a, x, 3), 0);
	// val = 3
	assertEq(Atomics.sub(a, x, 2), 3);
	// val = 1
	assertEq(Atomics.or(a, x, 6), 1);
	// val = 7
	assertEq(Atomics.and(a, x, 14), 7);
	// val = 6
	assertEq(Atomics.xor(a, x, 5), 6);
	// val = 3
	assertEq(Atomics.load(a, x), 3);
	// val = 3
	Atomics.store(a, x, 0);
	// val = 0

	// Check adjacent elements were not affected
	if (x > 0) {
	    assertEq(a[x-1], poison);
	    a[x-1] = 0;
	}
	if (x < a.length-1) {
	    assertEq(a[x+1], poison);
	    a[x+1] = 0;
	}
    }
}

function testFunction(a, ...indices) {
    dprint("Function: " + a.constructor.name);
    var poison;
    switch (a.BYTES_PER_ELEMENT) {
    case 1: poison = 0x5A; break;
    case 2: poison = 0x5A5A; break;
    case 4: poison = 0x5A5A5A5A; break;
    }
    for ( var i=0 ; i < indices.length ; i++ ) {
	var x = indices[i];
	if (x > 0)
	    a[x-1] = poison;
	if (x < a.length-1)
	    a[x+1] = poison;

	// val = 0
	assertEq(gAtomics_compareExchange(a, x, 0, 37), 0);
	// val = 37
	assertEq(gAtomics_compareExchange(a, x, 37, 5), 37);
	// val = 5
	assertEq(gAtomics_compareExchange(a, x, 7, 8), 5); // ie should fail
	// val = 5
	assertEq(gAtomics_compareExchange(a, x, 5, 9), 5);
	// val = 9
	assertEq(gAtomics_compareExchange(a, x, 5, 0), 9); // should also fail

	// val = 9
	assertEq(gAtomics_exchange(a, x, 4), 9);
	// val = 4
	assertEq(gAtomics_exchange(a, x, 9), 4);

	// val = 9
	assertEq(gAtomics_load(a, x), 9);
	// val = 9
	assertEq(gAtomics_store(a, x, 14), 14); // What about coercion?
	// val = 14
	assertEq(gAtomics_load(a, x), 14);
	// val = 14
	gAtomics_store(a, x, 0);
	// val = 0

	// val = 0
	assertEq(gAtomics_add(a, x, 3), 0);
	// val = 3
	assertEq(gAtomics_sub(a, x, 2), 3);
	// val = 1
	assertEq(gAtomics_or(a, x, 6), 1);
	// val = 7
	assertEq(gAtomics_and(a, x, 14), 7);
	// val = 6
	assertEq(gAtomics_xor(a, x, 5), 6);
	// val = 3
	assertEq(gAtomics_load(a, x), 3);
	// val = 3
	gAtomics_store(a, x, 0);
	// val = 0

	// Check adjacent elements were not affected
	if (x > 0) {
	    assertEq(a[x-1], poison);
	    a[x-1] = 0;
	}
	if (x < a.length-1) {
	    assertEq(a[x+1], poison);
	    a[x+1] = 0;
	}
    }
}

function testTypeCAS(a) {
    dprint("Type: " + a.constructor.name);

    var thrown = false;
    try {
	Atomics.compareExchange([0], 0, 0, 1);
    }
    catch (e) {
	thrown = true;
	assertEq(e instanceof TypeError, true);
    }
    assertEq(thrown, true);

    // All these variants should be OK
    Atomics.compareExchange(a, 0, 0.7, 1.8);
    Atomics.compareExchange(a, 0, "0", 1);
    Atomics.compareExchange(a, 0, 0, "1");
    Atomics.compareExchange(a, 0, 0);
}

function testTypeBinop(a, op) {
    dprint("Type: " + a.constructor.name);

    var thrown = false;
    try {
	op([0], 0, 1);
    }
    catch (e) {
	thrown = true;
	assertEq(e instanceof TypeError, true);
    }
    assertEq(thrown, true);

    // These are all OK
    op(a, 0, 0.7);
    op(a, 0, "0");
    op(a, 0);
}

var globlength = 0;		// Will be set later

function testRangeCAS(a) {
    dprint("Range: " + a.constructor.name);

    var msg = /out-of-range index/; // A generic message

    assertErrorMessage(() => Atomics.compareExchange(a, -1, 0, 1), RangeError, msg);
    assertEq(a[0], 0);

    // Converted to 0
    assertEq(Atomics.compareExchange(a, "hi", 0, 33), 0);
    assertEq(a[0], 33);
    a[0] = 0;

    assertErrorMessage(() => Atomics.compareExchange(a, a.length + 5, 0, 1), RangeError, msg);
    assertEq(a[0], 0);

    assertErrorMessage(() => Atomics.compareExchange(a, globlength, 0, 1), RangeError, msg);
    assertEq(a[0], 0);
}

// Ad-hoc tests for extreme and out-of-range values.
// None of these should throw

function testInt8Extremes(a) {
    dprint("Int8 extremes");

    a[10] = 0;
    a[11] = 0;

    Atomics.store(a, 10, 255);
    assertEq(a[10], -1);
    assertEq(Atomics.load(a, 10), -1);

    Atomics.add(a, 10, 255); // should coerce to -1
    assertEq(a[10], -2);
    assertEq(Atomics.load(a, 10), -2);

    Atomics.add(a, 10, -1);
    assertEq(a[10], -3);
    assertEq(Atomics.load(a, 10), -3);

    Atomics.sub(a, 10, 255);	// should coerce to -1
    assertEq(a[10], -2);
    assertEq(Atomics.load(a, 10), -2);

    Atomics.sub(a, 10, 256);	// should coerce to 0
    assertEq(a[10], -2);
    assertEq(Atomics.load(a, 10), -2);

    Atomics.and(a, 10, -1);	// Preserve all
    assertEq(a[10], -2);
    assertEq(Atomics.load(a, 10), -2);

    Atomics.and(a, 10, 256);	// Preserve none
    assertEq(a[10], 0);
    assertEq(Atomics.load(a, 10), 0);

    Atomics.store(a, 10, 255);
    assertEq(Atomics.exchange(a, 10, 0), -1);

    assertEq(a[11], 0);
}

function testUint8Extremes(a) {
    dprint("Uint8 extremes");

    a[10] = 0;
    a[11] = 0;

    Atomics.store(a, 10, 255);
    assertEq(a[10], 255);
    assertEq(Atomics.load(a, 10), 255);

    Atomics.add(a, 10, 255);
    assertEq(a[10], 254);
    assertEq(Atomics.load(a, 10), 254);

    Atomics.add(a, 10, -1);
    assertEq(a[10], 253);
    assertEq(Atomics.load(a, 10), 253);

    Atomics.sub(a, 10, 255);
    assertEq(a[10], 254);
    assertEq(Atomics.load(a, 10), 254);

    Atomics.and(a, 10, -1);	// Preserve all
    assertEq(a[10], 254);
    assertEq(Atomics.load(a, 10), 254);

    Atomics.and(a, 10, 256);	// Preserve none
    assertEq(a[10], 0);
    assertEq(Atomics.load(a, 10), 0);

    Atomics.store(a, 10, 255);
    assertEq(Atomics.exchange(a, 10, 0), 255);

    assertEq(a[11], 0);
}

function testInt16Extremes(a) {
    dprint("Int16 extremes");

    a[10] = 0;
    a[11] = 0;

    Atomics.store(a, 10, 65535);
    assertEq(a[10], -1);
    assertEq(Atomics.load(a, 10), -1);

    Atomics.add(a, 10, 65535); // should coerce to -1
    assertEq(a[10], -2);
    assertEq(Atomics.load(a, 10), -2);

    Atomics.add(a, 10, -1);
    assertEq(a[10], -3);
    assertEq(Atomics.load(a, 10), -3);

    Atomics.sub(a, 10, 65535);	// should coerce to -1
    assertEq(a[10], -2);
    assertEq(Atomics.load(a, 10), -2);

    Atomics.sub(a, 10, 65536);	// should coerce to 0
    assertEq(a[10], -2);
    assertEq(Atomics.load(a, 10), -2);

    Atomics.and(a, 10, -1);	// Preserve all
    assertEq(a[10], -2);
    assertEq(Atomics.load(a, 10), -2);

    Atomics.and(a, 10, 65536);	// Preserve none
    assertEq(a[10], 0);
    assertEq(Atomics.load(a, 10), 0);

    assertEq(a[11], 0);
}

function testUint32(a) {
    var k = 0;
    for ( var i=0 ; i < 20 ; i++ ) {
	a[i] = i+5;
	k += a[i];
    }

    var sum = 0;
    for ( var i=0 ; i < 20 ; i++ )
	sum += Atomics.add(a, i, 1);

    assertEq(sum, k);
}

// This test is a reliable test of sign extension in the JIT where
// testInt8Extremes is not (because there may not be enough type
// information without a loop - see bug 1181062 for a description
// of the general problem).

function exchangeLoop(ta) {
    var sum = 0;
    for ( var i=0 ; i < 100000 ; i++ )
	sum += Atomics.exchange(ta, i & 15, 255);
    return sum;
}

function adHocExchange(SharedOrUnsharedArrayBuffer) {
    var a = new Int8Array(new SharedOrUnsharedArrayBuffer(16));
    for ( var i=0 ; i < a.length ; i++ )
	a[i] = 255;
    assertEq(exchangeLoop(a), -100000);
}

// isLockFree(n) may return true only if there is an integer array
// on which atomic operations is allowed whose byte size is n,
// ie, it must return false for n=7.
//
// SpiderMonkey has isLockFree(1), isLockFree(2), isLockFree(8), isLockFree(4)
// on all supported platforms, only the last is guaranteed by the spec.

var sizes   = [    1,     2,     3,     4,     5,     6,     7,  8,
                   9,    10,    11,    12];
var answers = [ true,  true, false,  true, false, false, false, true,
	       false, false, false, false];

function testIsLockFree() {
    // This ought to defeat most compile-time resolution.
    for ( var i=0 ; i < sizes.length ; i++ ) {
	var v = Atomics.isLockFree(sizes[i]);
	var a = answers[i];
	assertEq(typeof v, 'boolean');
	assertEq(v, a);
    }

    // This ought to be optimizable.
    assertEq(Atomics.isLockFree(1), true);
    assertEq(Atomics.isLockFree(2), true);
    assertEq(Atomics.isLockFree(3), false);
    assertEq(Atomics.isLockFree(4), true);
    assertEq(Atomics.isLockFree(5), false);
    assertEq(Atomics.isLockFree(6), false);
    assertEq(Atomics.isLockFree(7), false);
    assertEq(Atomics.isLockFree(8), true);
    assertEq(Atomics.isLockFree(9), false);
    assertEq(Atomics.isLockFree(10), false);
    assertEq(Atomics.isLockFree(11), false);
    assertEq(Atomics.isLockFree(12), false);
}

function testIsLockFree2() {
    assertEq(Atomics.isLockFree(0), false);
    assertEq(Atomics.isLockFree(0/-1), false);
    assertEq(Atomics.isLockFree(3.5), false);
    assertEq(Atomics.isLockFree(Number.NaN), false);  // NaN => +0
    assertEq(Atomics.isLockFree(Number.POSITIVE_INFINITY), false);
    assertEq(Atomics.isLockFree(Number.NEGATIVE_INFINITY), false);
    assertEq(Atomics.isLockFree(-4), false);
    assertEq(Atomics.isLockFree('4'), true);
    assertEq(Atomics.isLockFree('-4'), false);
    assertEq(Atomics.isLockFree('4.5'), true);
    assertEq(Atomics.isLockFree('5.5'), false);
    assertEq(Atomics.isLockFree(new Number(4)), true);
    assertEq(Atomics.isLockFree(new String('4')), true);
    assertEq(Atomics.isLockFree(new Boolean(true)), true);
    var thrown = false;
    try {
	Atomics.isLockFree(Symbol('1'));
    } catch (e) {
	thrown = e;
    }
    assertEq(thrown instanceof TypeError, true);
    assertEq(Atomics.isLockFree(true), true);
    assertEq(Atomics.isLockFree(false), false);
    assertEq(Atomics.isLockFree(undefined), false);
    assertEq(Atomics.isLockFree(null), false);
    assertEq(Atomics.isLockFree({toString: () => '4'}), true);
    assertEq(Atomics.isLockFree({valueOf: () => 4}), true);
    assertEq(Atomics.isLockFree({valueOf: () => 5}), false);
    assertEq(Atomics.isLockFree({password: "qumquat"}), false);
}

function testUint8Clamped(sab) {
    var ta = new Uint8ClampedArray(sab);
    var thrown = false;
    try {
	CLONE(testMethod)(ta, 0);
    }
    catch (e) {
	thrown = true;
	assertEq(e instanceof TypeError, true);
    }
    assertEq(thrown, true);
}

function testWeirdIndices(SharedOrUnsharedArrayBuffer) {
    var a = new Int8Array(new SharedOrUnsharedArrayBuffer(16));
    a[3] = 10;
    assertEq(Atomics.load(a, "0x03"), 10);
    assertEq(Atomics.load(a, {valueOf: () => 3}), 10);
}

function isLittleEndian() {
    var xxx = new ArrayBuffer(2);
    var xxa = new Int16Array(xxx);
    var xxb = new Int8Array(xxx);
    xxa[0] = 37;
    var is_little = xxb[0] == 37;
    return is_little;
}

function runTests(SharedOrUnsharedArrayBuffer) {
    var is_little = isLittleEndian();

    // Currently the SharedArrayBuffer needs to be a multiple of 4K bytes in size.
    var sab = new SharedOrUnsharedArrayBuffer(4096);

    // Test that two arrays created on the same storage alias
    var t1 = new Int8Array(sab);
    var t2 = new Uint16Array(sab);

    assertEq(t1[0], 0);
    assertEq(t2[0], 0);
    t1[0] = 37;
    if (is_little)
	assertEq(t2[0], 37);
    else
	assertEq(t2[0], 37 << 8);
    t1[0] = 0;

    // Test that invoking as Atomics.whatever() works, on correct arguments.
    CLONE(testMethod)(new Int8Array(sab), 0, 42, 4095);
    CLONE(testMethod)(new Uint8Array(sab), 0, 42, 4095);
    CLONE(testMethod)(new Int16Array(sab), 0, 42, 2047);
    CLONE(testMethod)(new Uint16Array(sab), 0, 42, 2047);
    CLONE(testMethod)(new Int32Array(sab), 0, 42, 1023);
    CLONE(testMethod)(new Uint32Array(sab), 0, 42, 1023);

    // Test that invoking as v = Atomics.whatever; v() works, on correct arguments.
    gAtomics_compareExchange = Atomics.compareExchange;
    gAtomics_exchange = Atomics.exchange;
    gAtomics_load = Atomics.load;
    gAtomics_store = Atomics.store;
    gAtomics_add = Atomics.add;
    gAtomics_sub = Atomics.sub;
    gAtomics_and = Atomics.and;
    gAtomics_or = Atomics.or;
    gAtomics_xor = Atomics.xor;

    CLONE(testFunction)(new Int8Array(sab), 0, 42, 4095);
    CLONE(testFunction)(new Uint8Array(sab), 0, 42, 4095);
    CLONE(testFunction)(new Int16Array(sab), 0, 42, 2047);
    CLONE(testFunction)(new Uint16Array(sab), 0, 42, 2047);
    CLONE(testFunction)(new Int32Array(sab), 0, 42, 1023);
    CLONE(testFunction)(new Uint32Array(sab), 0, 42, 1023);

    // Test various range and type conditions
    var v8 = new Int8Array(sab);
    var v32 = new Int32Array(sab);

    CLONE(testTypeCAS)(v8);
    CLONE(testTypeCAS)(v32);

    CLONE(testTypeBinop)(v8, Atomics.add);
    CLONE(testTypeBinop)(v8, Atomics.sub);
    CLONE(testTypeBinop)(v8, Atomics.and);
    CLONE(testTypeBinop)(v8, Atomics.or);
    CLONE(testTypeBinop)(v8, Atomics.xor);

    CLONE(testTypeBinop)(v32, Atomics.add);
    CLONE(testTypeBinop)(v32, Atomics.sub);
    CLONE(testTypeBinop)(v32, Atomics.and);
    CLONE(testTypeBinop)(v32, Atomics.or);
    CLONE(testTypeBinop)(v32, Atomics.xor);

    // Test out-of-range references
    globlength = v8.length + 5;
    CLONE(testRangeCAS)(v8);
    globlength = v32.length + 5;
    CLONE(testRangeCAS)(v32);

    // Test extreme values
    testInt8Extremes(new Int8Array(sab));
    testUint8Extremes(new Uint8Array(sab));
    testInt16Extremes(new Int16Array(sab));
    testUint32(new Uint32Array(sab));

    // Test that Uint8ClampedArray is not accepted.
    testUint8Clamped(sab);

    // Misc ad-hoc tests
    adHocExchange(SharedOrUnsharedArrayBuffer);

    // Misc
    testIsLockFree();
    testIsLockFree2();
    testWeirdIndices(SharedOrUnsharedArrayBuffer);

    assertEq(Atomics[Symbol.toStringTag], "Atomics");
}

runTests(SharedArrayBuffer);
runTests(ArrayBuffer);
