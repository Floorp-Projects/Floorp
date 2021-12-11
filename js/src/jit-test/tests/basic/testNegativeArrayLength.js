function f() {
    try {
	for ( var i = 7; i > -2; i-- )
	    new Array(i).join('*');
    } catch (e) {
	return e instanceof RangeError;
    }
    return false;
}
assertEq(f(), true);
