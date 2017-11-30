var evalInFrame = (function evalInFrame(global) {
    var dbgGlobal = newGlobal();
    var dbg = new dbgGlobal.Debugger();
    return function evalInFrame(upCount, code) {
	dbg.addDebuggee(global);
	var frame = dbg.getNewestFrame().older;
	for (var i = 0; i < upCount; i++)
	    frame = frame.older;
	var completion = frame.eval(code);
	if (completion.throw)
	    throw 1;
    };
})(this);
function f() {
    for (var i = 0; i < 10; - i)
	g();
}
function h() {
    evalInFrame(0, "a.push(1)");
    evalInFrame(1, "a.push(2)");
}
function g() {
    h();
}
try {
    f();
} catch(e) {}
var a = [];
for (var i = 0; i < 3; i++)
    g();
assertEq(a.length, 6);
