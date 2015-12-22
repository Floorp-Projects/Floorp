"use strict";

Object.defineProperty(String.prototype, "toLocaleString", {
    get() {
        assertEq(typeof this, "string");

        return function() { return typeof this; };
    }
})

assertEq(["test"].toLocaleString(), "string");

if (typeof reportCompare === "function")
    reportCompare(true, true);
