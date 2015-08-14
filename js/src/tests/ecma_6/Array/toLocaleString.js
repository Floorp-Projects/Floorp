"use strict";

Object.defineProperty(String.prototype, "toLocaleString", {
    get() {
        // Congratulations! You probably fixed primitive-this getters.
        // Change "object" to "string".
        assertEq(typeof this, "object");

        return function() { return typeof this; };
    }
})

assertEq(["test"].toLocaleString(), "string");

if (typeof reportCompare === "function")
    reportCompare(true, true);
