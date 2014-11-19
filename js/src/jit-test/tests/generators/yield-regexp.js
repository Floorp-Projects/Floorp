// Bug 1099956

load(libdir + "asserts.js");

// ES6 treating yield as an identifier except in ES6 generators introduces a
// syntax conflict with permissible JS >= 1.7 legacy generator syntax.  Is
// |yield /a/g| inside a function an attempt to convert the function into a
// legacy generator, yielding a RegExp instance?  Or does it instead read as
// |(yield / a) / g|?  Similar ambiguities exist for different textual content
// in place of |a| -- |yield /x+17/g| or |(yield / x) + 17 / g|, and so on.
// (And, much less importantly, is |yield /a/g| a syntax error in global code
// as in JS >= 1.7, or is it |(yield / a) / g|.)
//
// For now, in JS >= 1.7, we preserve the old behavior.  In all other JS we
// conform to ES6: |yield /a/g| is a YieldExpression inside an ES6 generator,
// and it's an IdentifierReference divided twice when not in an ES6 generator.
// This test will need changes if we change our JS >= 1.7 parsing to be
// ES6-compatible.

function f1() {
  yield /abc/g;
}

var g = f1();
var v;
v = g.next();
assertEq(v instanceof RegExp, true);
assertEq(v.toString(), "/abc/g");
assertThrowsValue(() => g.next(), StopIteration);

function* f2() {
  yield /abc/g;
}

g = f2();
v = g.next();
assertEq(v.done, false);
assertEq(v.value instanceof RegExp, true);
assertEq(v.value.toString(), "/abc/g");
v = g.next();
assertEq(v.done, true);
