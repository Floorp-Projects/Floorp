// Control flow does not reach end of try block, code after try statement is
// reachable by catch block.
function f() {
    try {
	throw 3;
    } catch(e) {
    }

    var res = 0;
    for (var i=0; i<40; i++)
	res += 2;
    return res;
}
assertEq(f(), 80);
