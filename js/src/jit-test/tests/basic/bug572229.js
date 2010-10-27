// Don't crash

function f(o, s) {
    var k = s.substr(0, 1);
    var c;
    for (var i = 0; i < 10; ++i) {
	c = k in o;
    }
    return c;
}

assertEq(f({ a: 66 }, 'abc'), true);
