// |jit-test| debug
// Debug.Script instances with live referents stay alive.

var N = 4;
var g = newGlobal('new-compartment');
var dbg = new Debug(g);
var i;
dbg.hooks = {
    debuggerHandler: function (frame) {
        assertEq(frame.script instanceof Debug.Script, true);
        frame.script.id = i;
    }
};

g.eval('var arr = [];')
for (i = 0; i < N; i++)  // loop to defeat conservative GC
    g.eval("arr.push(function () { debugger }); arr[arr.length - 1]();");

gc();

var hits;
dbg.hooks = {
    debuggerHandler: function (frame) {
        hits++;
        assertEq(frame.script.id, i);
    }
};
hits = 0;
for (i = 0; i < N; i++)
    g.arr[i]();
assertEq(hits, N);
