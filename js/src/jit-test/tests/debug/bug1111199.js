g = newGlobal()
g.parent = this
g.eval("Debugger(parent).onExceptionUnwind=(function(){})")
try {
function f(code) {
    n = parseInt('', 0);
    return g("try{}catch(e){}", n)
}
function g(s, n) {
    s2 = s + s
    d = (n - (function  ()  {
            return "" + this.id + eval.id;
        } )().abstract) / 2
    m = g(s2, d)
}
f("switch(''){default:break;}")
} catch(exc1) {}
