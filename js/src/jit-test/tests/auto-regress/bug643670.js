// |jit-test| error:ReferenceError

// Binary: cache/js-dbg-32-55f463c562d4-linux
// Flags: -m -n -a
//
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
for (;;) {
    new f3(o3);
    f16(o3)
}
