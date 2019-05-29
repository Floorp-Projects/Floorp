"use strict";

var target = {};
Object.defineProperty(target, "test", {configurable: false, writable: true, value: 5});

var proxy = new Proxy(target, {
    defineProperty(target, property) {
        assertEq(property, "test");
        return true;
    }
});

assertThrowsInstanceOf(
    () => Object.defineProperty(proxy, "test", {writable: false}), TypeError);

assertThrowsInstanceOf(
    () => Reflect.defineProperty(proxy, "test", {writable: false}), TypeError);

reportCompare(0, 0);
