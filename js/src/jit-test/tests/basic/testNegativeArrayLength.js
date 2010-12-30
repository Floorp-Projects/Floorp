function f() {
    try {
	for ( var i = HOTLOOP-1; i > -2; i-- )
	    new Array(i).join('*');
    } catch (e) {
	return e instanceof RangeError;
    }
    return false;
}
assertEq(f(), true);
