// |jit-test| debug
// Test removing hooks during dispatch.

var g = newGlobal('new-compartment');
var log = '';

function addDebug(n) {
    for (var i = 0; i < n; i++) {
        var dbg = new Debug(g);
        dbg.hooks = {
            num: i,
            dbg: dbg,
            debuggerHandler: function (stack) {
                log += this.num + ', ';
                this.dbg.enabled = false;
                this.dbg.hooks = {};
                this.dbg = null;
                gc();
            }
        };
    }
    dbg = null;
}

addDebug(10);
g.eval("debugger;");
assertEq(log, '0, 1, 2, 3, 4, 5, 6, 7, 8, 9, ');
