function testTypedArrayTrace()
{
    var ar = new Uint32Array(16);

    for (var i = 0; i < ar.length-1; ++i) {
	ar[i+1] = i + ar[i];
    }

    for (var i = 0; i < ar.length; ++i) {
	// deliberate out of bounds access
	ar[i-2] = ar[i+2];
    }

    var t = 0;
    for (var i = 0; i < ar.length; ++i) {
	t += ar[i];
    }

    return t;
}
assertEq(testTypedArrayTrace(), 752);

