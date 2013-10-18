// |jit-test| debug
// Forced return from a star generator frame.

load(libdir + 'asserts.js')
load(libdir + 'iteration.js')

var g = newGlobal();
g.debuggeeGlobal = this;
g.eval("var dbg = new Debugger(debuggeeGlobal);" +
       "dbg.onDebuggerStatement = function () { return {return: '!'}; };");

function* gen() {
    yield '1';
    debugger;  // Force return here. The value is ignored.
    yield '2';
}
var iter = gen();
assertIteratorNext(iter, '1');
assertEq(iter.next(), '!');
assertThrowsInstanceOf(iter.next.bind(iter), TypeError);
