// |jit-test| --enable-private-fields; --more-compartments

a = newGlobal()
b = a.Debugger(this)
function c() {
    b.getNewestFrame().eval("")
}
c()
d = class {
    #e
}