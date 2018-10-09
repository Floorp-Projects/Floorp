// |jit-test| slow; skip-if: !('oomTest' in this)
function f(x) {
    new Int32Array(x);
}

f(0);
oomTest(() => f(2147483647));
