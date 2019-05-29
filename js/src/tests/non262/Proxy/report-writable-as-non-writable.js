"use strict";

var target = {};
Object.defineProperty(target, "test",
    {configurable: false, writable: true, value: 1});

var proxy = new Proxy(target, {
    getOwnPropertyDescriptor(target, property) {
        assertEq(property, "test");
        return {configurable: false, writable: false, value: 1};
    }
});

assertThrowsInstanceOf(() => Object.getOwnPropertyDescriptor(proxy, "test"),
                       TypeError);

assertThrowsInstanceOf(() => Reflect.getOwnPropertyDescriptor(proxy, "test"),
                       TypeError);

reportCompare(0, 0);
