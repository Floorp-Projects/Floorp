// |jit-test| error: ReferenceError
// vim: set ts=4 sw=4 tw=99 et:
try {
    (function () {
        __proto__ = Uint32Array()
    }())
} catch (e) {}(function () {
    length, ([eval()] ? x : 7)
})()
