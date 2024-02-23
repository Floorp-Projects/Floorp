// |jit-test| slow
function f(x) {
    new Int32Array(x);
}

f(0);
oomTest(() => f(2147483647));
