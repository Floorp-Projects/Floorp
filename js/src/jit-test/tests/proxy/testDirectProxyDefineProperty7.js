// Bug 1133085 - Test that descriptors are properly reconstituted
// when only .get or only .set is present.

load(libdir + "asserts.js");

var input, output;
var p = new Proxy({x: 0}, {
    defineProperty(t, k, desc) { output = desc; print("ok"); return true; }
});

input = {get: function () {}};
Object.defineProperty(p, "x", input);
assertDeepEq(output, input);

input = {set: function () {}};
Object.defineProperty(p, "x", input);
assertDeepEq(output, input);
