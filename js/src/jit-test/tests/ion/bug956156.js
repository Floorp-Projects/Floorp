// |jit-test| error:TypeError
function f() {
    ((function g(x) {
        g(x.slice)
    })([]))
}
new f
