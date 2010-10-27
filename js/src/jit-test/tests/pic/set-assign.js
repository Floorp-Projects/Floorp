function f() {
    var o = { a: 555 };

    for (var j = 0; j < 10; ++j) {
	var i = o.a = 100 + j;
	assertEq(i, 100 + j);
    }
}

f()