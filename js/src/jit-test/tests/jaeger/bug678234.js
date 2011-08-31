a = {}
function f(o) {
    for (x in o) {
        print
    }
}
for (var i = 0; i < 3; i++) {
    new f(a)
    a.__proto__ = null
}
