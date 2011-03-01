
var g = 10;

function bar(n) {
    return g;
}

function foo(n, v) {
    for (var i = 0; i < n; i++)
	assertEq(bar(i), v);
}

foo(10, 10);

gc();

eval("g = 10.5");

foo(10, 10.5);
