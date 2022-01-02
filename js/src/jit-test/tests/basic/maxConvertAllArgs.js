//Bug 818620

load(libdir + "asserts.js");

assertThrowsInstanceOf(function () {
    Math.max(NaN, { valueOf: function () { throw new Error() } });
}, Error);

assertThrowsInstanceOf(function () {
    Math.min(NaN, { valueOf: function () { throw new Error() } });
}, Error);
