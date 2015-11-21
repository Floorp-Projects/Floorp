// Frame.this and evalInFrame in the global scope.
var g = newGlobal();
g.eval("x = 4; this['.this'] = 222;");
var dbg = new Debugger(g);
var res;
dbg.onDebuggerStatement = function (frame) {
    res = frame.eval("this.x").return;
    res += frame.this.unsafeDereference().x;
};
g.eval("debugger;");
assertEq(res, 8);

// And inside eval.
g.eval("x = 3; eval('debugger')");
assertEq(res, 6);
g.eval("x = 2; eval('eval(\\'debugger\\')')");
assertEq(res, 4);

// And inside arrow functions.
g.eval("x = 1; (() => { debugger; })()");
assertEq(res, 2);
g.eval("x = 5; (() => { eval('debugger'); })()");
assertEq(res, 10);
