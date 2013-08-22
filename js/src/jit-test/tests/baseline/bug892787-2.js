// Constructing calls should throw if !callee->isInterpretedConstructor().
// This tests the polymorphic call path.

for (var i=0; i<20; i++)
    Function.prototype();

var funs = [
    function() { return 1; },
    function() { return 2; },
    function() { return 3; },
    function() { return 4; },
    function() { return 5; },
    function() { return 6; },
    function() { return 7; },
    function() { return 8; },
    function() { return 9; },
    function() { return 10; },
    Function.prototype
];

function f(callee) {
    new callee;
}
function g() {
    var c = 0;
    for (var i=0; i<50; i++) {
	try {
	    f(funs[i % funs.length]);
	} catch (e) {
	    assertEq(e.message.contains("not a constructor"), true);
	    c++;
	}
    }
    assertEq(c, 4);
}
g();
