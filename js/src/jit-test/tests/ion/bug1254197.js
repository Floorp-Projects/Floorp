setJitCompilerOption("ion.warmup.trigger", 1);
function f(x) {
    var w = [];
    var i = 0;
    for (var count = 0; count < 3; count++) {
        for (var j = 0; j < 60; j++) {
            if (j < 1) {
                w[0] = x[i];
            } else {
                w[0][0];
            }
        }
        i = 1;
    }
}
f([NaN, 0]);
