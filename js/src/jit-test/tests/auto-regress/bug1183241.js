// |jit-test| error:TypeError
"use strict";

// Make it impossible to create a toSource() representation of |o|, because
// 100 * 2**22 exceeds the maximum string length.
var s = "a".repeat(2 ** 22);
var o = {};
for (var i = 0; i < 100; ++i) {
    o["$" + i] = s;
}

Object.seal(o);

// Before the patch the toSource() represention was generated when an
// assignment failed in strict mode. And because it's not possible to create
// a toSource() representation for |o| (see above), this line caused an
// InternalError to be thrown. With the patch this line throws a TypeError.
o.foo = 0;
