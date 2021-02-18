function foo(trigger) {
    var x = Math.fround(1.5);
    var a = Math.sqrt(2**53);
    if (trigger) {
	x = a + 1;
    }
    return x;
}

with ({}) {}
for (var i = 0; i < 40; i++) {
    foo(false);
}
assertEq(foo(true), Math.sqrt(2**53) + 1);
