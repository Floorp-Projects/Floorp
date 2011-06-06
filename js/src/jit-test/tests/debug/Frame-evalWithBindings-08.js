// |jit-test| debug
// evalWithBindings ignores non-enumerable and non-own properties.

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
var hits = 0;
dbg.hooks = {
    debuggerHandler: function (frame) {
	assertEq(frame.evalWithBindings("toString + constructor + length", []).return, 112233);
	var obj = Object.create({constructor: "FAIL"}, {length: {value: "fail"}});
	assertEq(frame.evalWithBindings("toString + constructor + length", obj).return, 112233);
	hits++;
    }
};
g.eval("function f() { var toString = 111111, constructor = 1111, length = 11; debugger; }");
g.f();
assertEq(hits, 1);
