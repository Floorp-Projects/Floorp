// |jit-test| allow-oom; allow-unhandlable-oom; skip-if: getBuildConfiguration("android")
// Disabled on Android due to harness problems (Bug 1532654)

gcparam("maxBytes", gcparam("gcBytes") + 4*1024);
var max = 400;
function f(b) {
    if (b) {
        f(b - 1);
    } else {
        g = {
            apply:function(x,y) {            }
        };
    }
    g.apply(null, arguments);
}
f(max - 1);
