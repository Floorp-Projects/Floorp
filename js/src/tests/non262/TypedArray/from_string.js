// %TypedArray%.from called on Array should also handle strings correctly.
var from = Int8Array.from.bind(Uint32Array);
var toCodePoint = s => s.codePointAt(0);

// %TypedArray%.from on a string iterates over the string.
assertEqArray(from("test string", toCodePoint),
             ['t', 'e', 's', 't', ' ', 's', 't', 'r', 'i', 'n', 'g'].map(toCodePoint));

// %TypedArray%.from on a string handles surrogate pairs correctly.
var gclef = "\uD834\uDD1E"; // U+1D11E MUSICAL SYMBOL G CLEF
assertEqArray(from(gclef, toCodePoint), [gclef].map(toCodePoint));
assertEqArray(from(gclef + " G", toCodePoint), [gclef, " ", "G"].map(toCodePoint));

// %TypedArray%.from on a string calls the @@iterator method.
String.prototype[Symbol.iterator] = function* () { yield 1; yield 2; };
assertEqArray(from("anything"), [1, 2]);

// If the iterator method is deleted, Strings are still arraylike.
delete String.prototype[Symbol.iterator];
assertEqArray(from("works", toCodePoint), ['w', 'o', 'r', 'k', 's'].map(toCodePoint));
assertEqArray(from(gclef, toCodePoint), ['\uD834', '\uDD1E'].map(toCodePoint));

if (typeof reportCompare === "function")
    reportCompare(true, true);
