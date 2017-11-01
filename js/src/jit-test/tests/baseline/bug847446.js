// |jit-test| error: ReferenceError
var k = 0;
function test() {
    function* gen()  {
	try {
	    try {
		yield 1;
	    } finally {
		if (k++ < 60)
		    actual += "Inner finally";
	    }
	} finally { }
    }
    try {
	var g = gen();
	assertEq(g.next().value, 1);
	g.next();
    } catch (e) {
	throw e;
    }
}
test();
