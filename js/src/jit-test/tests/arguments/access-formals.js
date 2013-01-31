function g1(x, args) {
    args[0] = 88;
}

// We assume we can optimize arguments access in |f|.
//
// Then the apply-call invalidates the arguments optimization,
// and creates a real arguments object.
//
// Test that x and y fetch the values from the args object when
// that happens.
function f1(x, y, o) {
    var res = 0;
    for (var i=0; i<50; i++) {
	res += x + y;
	if (i > 10)
	    o.apply(null, arguments);
    }
    return res;
}

var o1 = {apply: g1};
assertEq(f1(3, 5, o1), 3630);
assertEq(f1(3, 5, o1), 3630);

// In strict mode, the arguments object does not alias formals.
function g2(x, args) {
    args[0] = 88;
}

function f2(x, y, o) {
    "use strict";
    var res = 0;
    for (var i=0; i<50; i++) {
	res += x + y;
	if (i > 10)
	    o.apply(null, arguments);
    }
    return res;
}

var o2 = {apply: g2};
assertEq(f2(3, 5, o2), 400);
assertEq(f2(3, 5, o2), 400);
