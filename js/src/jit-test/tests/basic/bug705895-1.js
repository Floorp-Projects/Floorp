c = (0).__proto__
function f(o) {
    o.__proto__ = null
    for (x in o) {}
}
for (i = 0; i < 9; i++) {
    f(c)
    Function.prototype.__proto__.__proto__ = c
    for (x in Function.prototype.__proto__) {}
    f(Math.__proto__)
}
