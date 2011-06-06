// |jit-test| debug
// Dispatching an event to a debugger must keep enough of it gc-alive to avoid
// crashing.

var g = newGlobal('new-compartment');
var hits;

function addDebug() {
    // These loops are here mainly to defeat the conservative GC. :-\
    for (var i = 0; i < 4; i++) {
        var dbg = new Debug(g);
        dbg.hooks = {
            dbg: dbg,
            debuggerHandler: function (stack) {
                hits++;
                for (var j = 0; j < 4; j++) {
                    this.dbg.enabled = false;
                    this.dbg.hooks = {};
                    this.dbg = new Debug(g);
                }
                gc();
            }
        };
        if (i > 0) {
            dbg.enabled = false;
            dbg.hooks = {};
            dbg = null;
        }
    }
}

addDebug();
hits = 0;
g.eval("debugger;");
assertEq(hits, 1);
