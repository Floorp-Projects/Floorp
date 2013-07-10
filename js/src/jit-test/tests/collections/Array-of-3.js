// Array.of does not leave holes

load(libdir + "asserts.js");

assertDeepEq(Array.of(undefined), [undefined]);
assertDeepEq(Array.of(undefined, undefined), [undefined, undefined]);
assertDeepEq(Array.of.apply(this, [,,undefined]), [undefined, undefined, undefined]);
assertDeepEq(Array.of.apply(this, Array(4)), [undefined, undefined, undefined, undefined]);
