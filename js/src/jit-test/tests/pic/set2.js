function f(k) {
    var o1 = { a: 5 };
    var o2 = { b : 7, a : 9 };

    for (var i = 0; i < k; ++i) {
	var o = i % 2 ? o2 : o1;
	o.a = i;
    }

    return o1.a + ',' + o2.a;
}

assertEq(f(5), '4,3')
assertEq(f(6), '4,5')
