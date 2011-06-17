// |jit-test| debug

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
var hits = 0;
dbg.hooks = {
    debuggerHandler: function (frame) {
        var arr = frame.arguments;
        assertEq(arr[0].script instanceof Debug.Script, true);
        assertEq(arr[1].script, undefined);
        assertEq(arr[2].script, undefined);
        hits++;
    }
};

g.eval("(function () { debugger; })(function g(){}, {}, Math.atan2);");
assertEq(hits, 1);
