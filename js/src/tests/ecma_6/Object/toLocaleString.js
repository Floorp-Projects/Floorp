"use strict";

Object.defineProperty(String.prototype, "toString", {
    get() {
        assertEq(typeof this, "string");

        return function() { return typeof this; };
    }
})
assertEq(Object.prototype.toLocaleString.call("test"), "string");

if (typeof reportCompare === "function")
    reportCompare(true, true);
