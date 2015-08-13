"use strict";

Object.defineProperty(String.prototype, "toString", {
    get() {
        // Congratulations! You probably fixed primitive-this getters.
        // Change "object" to "string".
        assertEq(typeof this, "object");

        return function() { return typeof this; };
    }
})
assertEq(Object.prototype.toLocaleString.call("test"), "string");

if (typeof reportCompare === "function")
    reportCompare(true, true);
