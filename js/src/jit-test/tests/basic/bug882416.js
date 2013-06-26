// |jit-test| error: can't convert
Error.prototype.toString = Function;
evaluate("n f", {
    noScriptRval: true,
    saveFrameChain: true
});
