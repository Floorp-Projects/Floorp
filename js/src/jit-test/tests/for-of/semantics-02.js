// Replacing Array.prototype.iterator with something non-callable makes for-of throw.

load(libdir + "asserts.js");
function test(v) {
    Array.prototype.iterator = v;
    assertThrowsInstanceOf(function () { for (var x of []) ; }, TypeError);
}
test(undefined);
test(null);
test({});
