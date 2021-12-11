// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var a = 9;

function directArg(eval, s) {
    var a = 1;
    return eval(s);
}

function directVar(f, s) {
    var eval = f;
    var a = 1;
    return eval(s);
}

function directWith(obj, s) {
    var f;
    with (obj) {
	f = function () {
	    var a = 1;
	    return eval(s);
	};
    }
    return f();
}

// direct eval, even though 'eval' is an argument
assertEq(directArg(eval, 'a+1'), 2);

// direct eval, even though 'eval' is a var
assertEq(directVar(eval, 'a+1'), 2);

// direct eval, even though 'eval' is found via a with block
assertEq(directWith(this, 'a+1'), 2);
assertEq(directWith({eval: eval, a: -1000}, 'a+1'), 2);

reportCompare(0, 0);
