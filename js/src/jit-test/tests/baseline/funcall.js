function test1() {
    var f = function() { return 1; };

    for (var i=0; i<25; i++) {
	f.call();
	if (i > 20)
	    f = Math.abs;
   }
}
test1();

var origCall = Function.prototype.call;

function test2() {
    var f = function() { return 1; };
    var c = 0;
    for (var i=0; i<25; i++) {
	f.call();
	if (i > 20)
	    Function.prototype.call = function() { c++; };
    }
    assertEq(c, 3);
}
test2();
Function.prototype.call = origCall;

function test3() {
    var f = function() { return 1; };
    for (var i=0; i<25; i++) {
	f.call();
	if (i > 20)
	    Function.prototype.call = undefined;
    }
}
try {
    test3();
    assertEq(0, 1);
} catch(e) {}

Function.prototype.call = origCall;

function test4() {
    var f = function(a, b, c) {
	assertEq(arguments.length, 1);
	assertEq(a, 1);
	assertEq(b, undefined);
	assertEq(c, undefined);
	return 1;
    };
    for (var i=0; i<25; i++) {
	f.call(null, 1);
    }
}
test4();
