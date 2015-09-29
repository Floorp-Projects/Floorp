load(libdir + "immutable-prototype.js");

c = Number.prototype;
function f(o) {
    o.__proto__ = null
    for (x in o) {}
}
for (i = 0; i < 9; i++) {
    f(c)
    if (globalPrototypeChainIsMutable())
        Object.prototype.__proto__ = c;
    for (x in Object.prototype)
        continue;
    f(Math.__proto__);
}
