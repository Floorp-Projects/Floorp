var a, b;

function f(str) {
    var n;
    var k;
    for (var i = 0; i < 18; ++i) {
	n = str;
	k = n++;
	if (k) { }
    }
    return [k, n];
}

[a, b] = f("10");
assertEq(a, 10);
assertEq(b, 11);

[a, b] = f("5");
assertEq(a, 5);
assertEq(b, 6);
