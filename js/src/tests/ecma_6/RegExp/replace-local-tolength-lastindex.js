// RegExp.prototype[@@replace] always executes ToLength(regExp.lastIndex) for
// non-global RegExps.

for (var flag of ["", "g", "y", "gy"]) {
    var regExp = new RegExp("a", flag);

    var called = false;
    regExp.lastIndex = {
        valueOf() {
            assertEq(called, false);
            called = true;
            return 0;
        }
    };

    assertEq(called, false);
    regExp[Symbol.replace]("");
    assertEq(called, !flag.includes("g"));
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
