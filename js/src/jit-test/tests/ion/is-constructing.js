var isConstructing = getSelfHostedValue("_IsConstructing");

function testBasic() {
    var f = function(expected) {
	with(this) {}; // Don't inline.
	assertEq(isConstructing(), expected);
    };
    for (var i=0; i<40; i++) {
	f(false);
	new f(true);
    }
}
testBasic();

function testGeneric() {
    var f1 = function(expected) {
	with(this) {};
	assertEq(isConstructing(), expected);
    };
    var f2 = function(expected) {
	assertEq(isConstructing(), expected);
    }
    var funs = [f1, f2];
    for (var i=0; i<40; i++) {
	var f = funs[i % 2];
	f(false);
	new f(true);
    }
}
testGeneric();

function testArgsRectifier() {
    var f = function(x) {
	with(this) {};
	assertEq(isConstructing(), true);
    };
    for (var i=0; i<40; i++)
	new f();
}
testArgsRectifier();

function testBailout() {
    var f1 = function(x, expected) {
	if (x > 20) {
	    bailout();
	    assertEq(isConstructing(), expected);
	}
    };
    var f2 = function(x) {
	return new f1(x, true);
    };
    var f3 = function(x) {
	return f1(x, false);
    };
    for (var i=0; i<40; i++) {
	f2(i);
	f3(i);
    }
    for (var i=0; i<40; i++)
	f1(i, false);
}
testBailout();
