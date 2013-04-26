function f() {
    var res = 0;
    for (var i=0; i<100; i++) {
	var s = "test" + i;
	res += s.length;
	assertEq(s[0], "t");
	assertEq(s[3], "t");
	if (i > 90)
	    assertEq(s[4], "9");
    }
    return res;
}
assertEq(f(), 590);
