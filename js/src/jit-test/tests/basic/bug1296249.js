// |jit-test| slow
if (!('oomTest' in this))
    quit();

function f(x) {
    new Int32Array(x);
}

f(0);
oomTest(() => f(2147483647));
