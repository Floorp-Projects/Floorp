// Disabling a Debugger object causes events to stop being delivered to it
// immediately, even if we're in the middle of dispatching.

var g = newGlobal({newCompartment: true});
var log;

var arr = [];
for (var i = 0; i < 4; i++) {
    arr[i] = new Debugger(g);
    arr[i].num = i;
    arr[i].onDebuggerStatement = function () {
        log += this.num;
        // Disable them all.
        for (var j = 0; j < arr.length; j++)
            arr[j].enabled = false;
    };
}

log = '';
g.eval("debugger; debugger;");
assertEq(log, '0');
