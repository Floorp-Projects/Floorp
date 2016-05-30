// |jit-test| error: SyntaxError
Error.prototype.toString = Function;
evaluate("n f", {
    noScriptRval: true,
});
