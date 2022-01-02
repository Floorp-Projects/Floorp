function testSlowNativeBail() {
    var a = ['0', '1', '2', '3', '+'];
    try {
	for (var i = 0; i < a.length; i++)
	    new RegExp(a[i]);
	assertEq(true, false);
    } catch (exc) {
        assertEq(exc instanceof SyntaxError, true);
    }
}
testSlowNativeBail();
