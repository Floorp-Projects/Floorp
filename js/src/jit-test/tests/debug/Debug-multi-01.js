// |jit-test| debug
// When there are multiple debuggers, their hooks are called in order.

var g = newGlobal('new-compartment');
var log;
var arr = [];

function addDebug(msg) {
    var dbg = new Debug(g);
    dbg.hooks = {
        debuggerHandler: function (stack) {
            log += msg;
        }
    };
    arr.push(dbg);
}

addDebug('a');
addDebug('b');
addDebug('c');

log = '';
assertEq(g.eval("debugger; 0;"), 0);
assertEq(log, 'abc');

// Calling debugger hooks stops as soon as any hook returns a resumption value
// other than undefined.

arr[0].hooks = {
    debuggerHandler: function (stack) {
        log += 'a';
        return {return: 1};
    }
};

log = '';
assertEq(g.eval("debugger; 0;"), 1);
assertEq(log, 'a');
