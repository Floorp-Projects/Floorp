function f() {
    var o = { a: 5 };

    for (var i = 0; i < 5; ++i) {
	o.a = i;
    }

    assertEq(o.a, 4);
}

f(); 