function f() {}
(function() {
    f()
})()
function g() {
    new f >> 0
}
g()
