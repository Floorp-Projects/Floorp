// A generator is left closed after frame.onPop returns a {return:} resumption value.

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
dbg.onDebuggerStatement = frame => {
    frame.onPop = completion => ({return: "ok"});
};
g.eval("function* g() { for (var i = 0; i < 10; i++) { debugger; yield i; } }");
var it = g.g();
var result = it.next();
assertEq(result.value, "ok");
assertEq(result.done, true);
assertEq(it.next().value, undefined);
