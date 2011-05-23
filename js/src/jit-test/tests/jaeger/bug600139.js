// |jit-test| error: ReferenceError
// vim: set ts=4 sw=4 tw=99 et:
function f(a, b, c) {
    if (!a.__SSi) {
        throw Components.returnCode = Cr.NS_ERROR_INVALID_ARG;
    }
    this.restoreWindow(a, b, c);
    eval();
}
f(1, 2, 3);
