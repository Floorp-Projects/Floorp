function g(aa) {
    assertEq(aa, 123);
}
function f(x, yy) {
    if (yy < 0) {
	for (var j=0; j<100; j++) {}
    }
    var o = yy < 2000 ? o1 : o2;
    o.fun.apply(22, arguments);
}

function test() {
    o1 = {};
    o1.fun = g;

    o2 = {};
    o2.x = 3;
    o2.fun = g;

    for (var i=0; i<3000; i++)
	f(123, i);
}
test();
