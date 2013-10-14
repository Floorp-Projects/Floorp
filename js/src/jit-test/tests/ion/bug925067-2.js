// |jit-test| error: 4
function g(o) {
    if (o.x >= 0) {
	for(;;)
	    o.next();
    }
    return o.x;
}
function f() {
    var o = {x: 0, next: function() {
	if (this.x++ > 100)
	    throw 4;
    }};
    g(o);
}
f();
