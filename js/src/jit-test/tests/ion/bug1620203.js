"use strict";
var proxy = new Proxy({}, {
    defineProperty(target, [...x27], ...[{p10: q34}]) {
        return true;
    }
});
Object.defineProperty(proxy, "test", {writable: false});
