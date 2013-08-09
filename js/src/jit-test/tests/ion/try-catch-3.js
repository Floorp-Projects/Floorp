// Don't fail if code after try statement is unreachable.
function f() {
    try {
	throw 1;
    } catch(e) {
	throw 5;
    }

    // Unreachable.
    assertEq(0, 2);
    var res = 0;
    for (var i=0; i<10; i++)
	res += 2;
    return res;
}

var c = 0;

for (var i=0; i<5; i++) {
    try {
	f();
	assertEq(0, 1);
    } catch(e) {
	c += e;
    }
}
assertEq(c, 25);
