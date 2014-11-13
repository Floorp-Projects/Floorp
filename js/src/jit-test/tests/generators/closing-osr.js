// OSR into a |finally| block while closing a legacy generator should work.
var log = "";
function f() {
    try {
	try {
	    log += "a";
	    yield 2;
	    log += "b";
	    yield 3;
	} finally {
	    log += "c";
	    for (var i=0; i<20; i++) {};
	    log += "d";
	}
    } catch(e) {
	log += "e";
    }
    log += "f";
}

var it = f();
assertEq(it.next(), 2);
it.close();
assertEq(log, "acd");
