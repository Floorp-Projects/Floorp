// %TypedArray%.from called on Array should also handle strings correctly.
var from = Int8Array.from.bind(Array);

// %TypedArray%.from on a string iterates over the string.
assertDeepEq(from("test string"),
             ['t', 'e', 's', 't', ' ', 's', 't', 'r', 'i', 'n', 'g']);

// %TypedArray%.from on a string handles surrogate pairs correctly.
var gclef = "\uD834\uDD1E"; // U+1D11E MUSICAL SYMBOL G CLEF
assertDeepEq(from(gclef), [gclef]);
assertDeepEq(from(gclef + " G"), [gclef, " ", "G"]);

// %TypedArray%.from on a string calls the @@iterator method.
String.prototype[Symbol.iterator] = function* () { yield 1; yield 2; };
assertDeepEq(from("anything"), [1, 2]);

// If the iterator method is deleted, Strings are still arraylike.
delete String.prototype[Symbol.iterator];
assertDeepEq(from("works"), ['w', 'o', 'r', 'k', 's']);
assertDeepEq(from(gclef), ['\uD834', '\uDD1E']);

if (typeof reportCompare === "function")
    reportCompare(true, true);
