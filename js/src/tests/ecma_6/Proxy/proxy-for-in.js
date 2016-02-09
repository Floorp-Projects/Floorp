// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
"use strict";

let steps = [];

const object = {
    __proto__: {
        "xyz": 42
    }
};
const proxy = new Proxy(object, {
    ownKeys(target) {
        steps.push("ownKeys")
        return ["a", "b"];
    },

    getOwnPropertyDescriptor(target, property) {
        steps.push("getOwn-" + property);
        return {
            value: undefined,
            configurable: true,
            writable: true,
            enumerable: (property === "a")
        };
    }
});

let iterated = [];
for (let x in proxy)
    iterated.push(x);

assertEq(iterated.toString(), "a,xyz");
assertEq(steps.toString(), "ownKeys,getOwn-a,getOwn-b");

if (typeof reportCompare === "function")
    reportCompare(true, true);
