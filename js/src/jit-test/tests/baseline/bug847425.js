// |jit-test| allow-oom; allow-unhandlable-oom
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
