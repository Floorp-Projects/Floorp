// |jit-test| allow-overrecursed
if (helperThreadCount() === 0)
    quit();
function eval(source) {
    offThreadCompileModule(source);
    let get = (eval("function w(){}") ++);
};
gczeal(21, 10);
gczeal(11, 8);
eval("");
