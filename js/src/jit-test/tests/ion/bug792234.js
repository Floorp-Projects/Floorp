
function foo() {
	return '' / undefined;
}

foo();
assertEq(foo(), NaN);
