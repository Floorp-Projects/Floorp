// |jit-test| error: ReferenceError
function f1(code) {
    f = Function(code)
    f2()
}
function f2() {
    f()
}
f1("d=this.__defineGetter__(\"x\",gc)")
f1("b(x&=w);function b(){}")

