function baz() { with({}) {}}

function foo(a,b,c) {
    var x1 = a + b;
    var y1 = b + c;
    var z1 = a + c;
    var x2 = a * b;
    var y2 = b * c;
    var z2 = a * c;
    var x3 = a - b;
    var y3 = b - c;
    var z3 = a - c;
    var x4 = b - a;
    var y4 = c - b;
    var z4 = c - a;
    var x1b = 1 + a + b;
    var y1b = 1 + b + c;
    var z1b = 1 + a + c;
    var x2b = 1 + a * b;
    var y2b = 1 + b * c;
    var z2b = 1 + a * c;
    var x3b = 1 + a - b;
    var y3b = 1 + b - c;
    var z3b = 1 + a - c;
    var x4b = 1 + b - a;
    var y4b = 1 + c - b;
    var z4b = 1 + c - a;

    var arg = arguments[a];

    baz(x1, y1, z1, x1b, y1b, z1b,
	x2, y2, z2, x2b, y2b, z2b,
	x3, y3, z3, x3b, y3b, z3b,
	x4, y4, z4, x4b, y4b, z4b);

    return arg;
}

function bar(a,b,c) {
    return foo(a+b,b+c,c+a);
}

with ({}) {}
for (var i = 0; i < 100; i++) {
    bar(0,0,0)
}
