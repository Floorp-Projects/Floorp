// In `for (let x = EXPR; ;)`, if `x` appears within EXPR, it refers to the
// loop variable. Actually doing this is typically a TDZ error.

load(libdir + "asserts.js");

assertThrowsInstanceOf(() => {
    for (let x = x; null.foo; null.foo++) {}
}, ReferenceError);

assertThrowsInstanceOf(() => {
    for (let x = eval('x'); null.foo; null.foo++) {}
}, ReferenceError);

assertThrowsInstanceOf(() => {
    for (let x = function () { with ({}) return x; }(); null.foo; null.foo++) {}
}, ReferenceError);
