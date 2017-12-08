// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

assertEq(uneval(Symbol.iterator), "Symbol.iterator");
assertEq(uneval(Symbol()), "Symbol()");
assertEq(uneval(Symbol("")), 'Symbol("")');
assertEq(uneval(Symbol("ponies")), 'Symbol("ponies")');
assertEq(uneval(Symbol.for("ponies")), 'Symbol.for("ponies")');

assertEq({glyph: Symbol(undefined)}.toSource(), "({glyph:Symbol()})");

if (typeof reportCompare === "function")
    reportCompare(0, 0);
