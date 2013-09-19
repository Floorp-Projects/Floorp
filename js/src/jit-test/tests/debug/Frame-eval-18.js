// yield is not allowed in eval in a star generator.

load(libdir + 'asserts.js');

var g = newGlobal();
var dbg = new Debugger(g);

dbg.onDebuggerStatement = function (frame) {
    assertThrowsInstanceOf(function() { frame.eval('yield 10;') }, SyntaxError);
};

g.eval("(function*g(){ debugger; })()");
