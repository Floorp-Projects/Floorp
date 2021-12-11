// |jit-test| allow-overrecursed; skip-if: helperThreadCount() === 0
function eval(source) {
    offThreadCompileModule(source);
    let get = (eval("function w(){}") ++);
};
gczeal(21, 10);
gczeal(11, 8);
eval("");
