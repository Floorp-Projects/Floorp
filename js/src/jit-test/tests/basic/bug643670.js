o3 = evalcx("split")
function f3(o) {
    try {
        new o
    } catch(e) {}
}
function f16(o) {
    Object.getOwnPropertyNames(o);
    o.__defineGetter__("prototype", function() {})
}
for (i = 0; i < 10; i++) {
    new f3(o3);
    f16(o3)
}
