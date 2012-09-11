// Don't assert.
function a(p0) {
	var x = 0;
	if (p0|0) {
		x = p0;
	}
	return x;
}
assertEq(a(1), 1);
