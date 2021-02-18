function foo(x) {
    var result;
    if (x) {
	result = Math.fround(~x);
    } else {
	var temp = Math.sqrt(2**53);
	for (var i = 0; i < 1000; i++) {} // Trigger OSR
	result = temp + 1;
    }
    return result;
}

foo(true);
for (var i = 0; i < 10; i++) {
    assertEq(foo(false), Math.sqrt(2**53) + 1);
}
