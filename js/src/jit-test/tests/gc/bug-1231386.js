if (!('oomTest' in this))
    quit();

function f1() {}
function f2() {}
r = [function() {}, function() {}, [], function() {}, f1, function() {}, f2];
l = [0];
function f3() {
    return {
        a: 0
    };
}
var x = f3();
var h = newGlobal();
var dbg = new Debugger;
dbg.addDebuggee(h);
oomTest(() => getBacktrace({
    thisprops: gc()
}));
