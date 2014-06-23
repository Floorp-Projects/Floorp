/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Several distinct Symbol values.
var symbols = [
    Symbol(),
    Symbol("Symbol.iterator"),
    Symbol("Symbol.iterator")   // distinct new symbol with the same description
];

// Distinct symbols are never equal to each other, even if they have the same
// description.
for (var i = 0; i < symbols.length; i++) {
    for (var j = i; j < symbols.length; j++) {
        var expected = (i === j);
        assertEq(symbols[i] == symbols[j], expected);
        assertEq(symbols[i] != symbols[j], !expected);
        assertEq(symbols[i] === symbols[j], expected);
        assertEq(symbols[i] !== symbols[j], !expected);
        assertEq(Object.is(symbols[i], symbols[j]), expected);
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
