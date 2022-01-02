load(libdir + "asserts.js");

const MaxStringLength = 2**30 - 2;

// First check MaxStringLength is accurate.
assertThrowsInstanceOf(() =>  "a".repeat(MaxStringLength + 1),
                       RangeError);

// escape() must handle uint32_t overflow correctly.
var s = "\u0390".repeat(MaxStringLength);
assertEq(s.length, MaxStringLength);
var ex = null;
try {
    escape(s);
} catch (e) {
    ex = e;
}
assertEq(ex === "out of memory" || (ex instanceof InternalError), true);
