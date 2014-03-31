// |jit-test| slow
function x() {}
for (var j = 0; j < 9999; ++j) {
    (function() {
        x += x.watch("endsWith", ArrayBuffer);
        return 0 >> Function(x)
    })()
}
