function f() {
    try {
	for ( var i = 1; i > -2; i-- )
	    new Array(i).join('*');
    } catch (e) {
	return e instanceof RangeError;
    }
    return false;
}
assertEq(f(), true);
