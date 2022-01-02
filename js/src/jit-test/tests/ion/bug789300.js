function f() {
	return (NaN ? 4 : 5);
}
assertEq(f(), 5);
