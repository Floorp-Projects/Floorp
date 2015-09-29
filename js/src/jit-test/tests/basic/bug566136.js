load(libdir + "immutable-prototype.js");

Object.prototype.length = 0;
if (globalPrototypeChainIsMutable())
    this.__proto__ = [];

function f() {
    eval('Math');
    length = 2;
}
f();
