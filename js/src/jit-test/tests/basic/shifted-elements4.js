function f() {
    var arr = [];
    for (var i = 0; i < 2; i++) {
	for (var j = 0; j < 90000; j++)
	    arr.push(j);
	for (var j = 0; j < 90000; j++)
	    assertEq(arr.shift(), j);
	assertEq(arr.length, 0);
    }
}
f();
