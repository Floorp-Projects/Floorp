// When there are multiple debuggers, their hooks are called in order.

var g = newGlobal({newCompartment: true});
var log;
var arr = [];

function addDebug(msg) {
    var dbg = new Debugger(g);
    dbg.onDebuggerStatement = function (stack) { log += msg; };
    arr.push(dbg);
}

addDebug('a');
addDebug('b');
addDebug('c');

log = '';
assertEq(g.eval("debugger; 0;"), 0);
assertEq(log, 'abc');

// Calling debugger hooks continues, even if one returns a resumption value
// other than undefined.

arr[0].onDebuggerStatement = function (stack) {
    log += 'a';
    return {return: 1};
};

log = '';
assertEq(g.eval("debugger; 0;"), 1);
assertEq(log, 'abc');
