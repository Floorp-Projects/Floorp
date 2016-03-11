var g = true;

function get(name)
{
    if (g)
	   return 8;
}

var p = new Proxy({}, { get: get } );
var o2 = Object.create(p);

function test(x, expected) {
    for (var i=0; i<3; i++) {
	var v = x.hello;
	if (g) assertEq(v, 8);
    }
}

g = false
test(o2);
g = true;
test(o2);
