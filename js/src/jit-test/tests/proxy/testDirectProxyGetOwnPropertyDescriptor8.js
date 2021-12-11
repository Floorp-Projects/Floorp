load(libdir + "asserts.js");

var target = {};
var handler = {
    getOwnPropertyDescriptor: function () { return { value: 2, configurable: true}; }
};

for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy])
    Object.getOwnPropertyDescriptor(p, 'foo');

Object.preventExtensions(target);
for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy])
    assertThrowsInstanceOf(() => Object.getOwnPropertyDescriptor(p, 'foo'), TypeError);
