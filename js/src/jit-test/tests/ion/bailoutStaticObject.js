// Test bailouts from loading particular non-singleton static objects.

function wrap(fun) {
    function wrapper() {
	return fun.apply(this, arguments);
    }
    return wrapper;
}

var adder = wrap(function(a, b) { return a + b; });
var subber = wrap(function(a, b) { return a - b; });
var tmp = adder;
adder = subber;
adder = tmp;

function foo() {
    var i = 0;
    var a = 0;
    for (var i = 0; i < 10000; i++) {
	a = adder(a, 1);
	a = subber(a, 1);
    }
    return a;
}

assertEq(foo(), 0);
adder = subber;
assertEq(foo(), -20000);
