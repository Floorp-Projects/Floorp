function testBasicTypedArrays()
{
    var ar, aridx, idx;

    var a = new Uint8Array(16);
    var b = new Uint16Array(16);
    var c = new Uint32Array(16);
    var d = new Int8Array(16);
    var e = new Int16Array(16);
    var f = new Int32Array(16);

    var g = new Float32Array(16);
    var h = new Float64Array(16);

    var iarrays = [ a, b, c, d, e, f ];
    for (aridx = 0; aridx < iarrays.length; ++aridx) {
	ar = iarrays[aridx];

	for (idx = 0; idx < ar.length-4; ++idx) {
	    ar[idx] = 22;
	    ar[idx+1] = 12.7;
	    ar[idx+2] = "99";
	    ar[idx+3] = { k: "thing" };
	    ar[idx+4] = Infinity;
	}

	assertEq(ar[ar.length-5], 22);
	assertEq(ar[ar.length-4], 12);
	assertEq(ar[ar.length-3], 99);
	assertEq(ar[ar.length-2], 0);
	assertEq(ar[ar.length-1], 0);
    }

    var farrays = [ g, h ];
    for (aridx = 0; aridx < farrays.length; ++aridx) {
	ar = farrays[aridx];

	for (idx = 0; idx < ar.length-4; ++idx) {
	    ar[idx] = 22;
	    ar[idx+1] = 12.25;
	    ar[idx+2] = "99";
	    ar[idx+3] = { k: "thing" };
	    ar[idx+4] = Infinity;
	}

	assertEq(ar[ar.length-5], 22);
	assertEq(ar[ar.length-4], 12.25);
	assertEq(ar[ar.length-3], 99);
	assertEq(!(ar[ar.length-2] == ar[ar.length-2]), true);
	assertEq(ar[ar.length-1], Infinity);
    }
}

function testSpecialTypedArrays()
{
    var ar, aridx, idx;

    ar = new Uint8ClampedArray(16);
    for (idx = 0; idx < ar.length-4; ++idx) {
	ar[idx] = -200;
	ar[idx+1] = 127.5;
	ar[idx+2] = 987;
	ar[idx+3] = Infinity;
	ar[idx+4] = "hello world";
    }

    assertEq(ar[ar.length-5], 0);
    assertEq(ar[ar.length-4], 128);
    assertEq(ar[ar.length-3], 255);
    assertEq(ar[ar.length-2], 255);
    assertEq(ar[ar.length-1], 0);
}

function testTypedArrayOther()
{
    var ar = new Int32Array(16);
    for (var i = 0; i < ar.length; ++i) {
	ar[i] = i;
    }

    for (var i = 0; i < ar.length; ++i) {
	// deliberate out of bounds access
	ar[i-2] = ar[i+2];
    }

    var t = 0;
    for (var i = 0; i < ar.length; ++i) {
	t += ar[i];
    }

    assertEq(t, 143);
}

testBasicTypedArrays();
testSpecialTypedArrays();
testTypedArrayOther();
