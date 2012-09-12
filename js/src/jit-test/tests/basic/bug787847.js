var g = true;

function getown(name)
{
    if (g)
	return { value: 8, enumerable: true, writable: false, configurable: true };
}

var p = Proxy.create( { getPropertyDescriptor: getown } );
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
