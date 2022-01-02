var c = 0;
function g(o) {
    try {
	for(;;)
	    o.next();
    } catch(e) {
	c += e;
    }
    return o.x;
}
function f() {
    var o = {x: 0, next: function() {
	if (this.x++ > 100)
	    throw 3;
    }};

    g(o);
    assertEq(o.x, 102);

    o.x = 0;
    g(o);
    assertEq(o.x, 102);
}
f();
assertEq(c, 6);
