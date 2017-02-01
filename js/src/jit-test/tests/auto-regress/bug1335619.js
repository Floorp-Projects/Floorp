// jsfunfuzz-generated
x = [];
y = x.push(Set, 1);
Array.prototype.shift.call(x);
// Adapted from randomly chosen test: js/src/tests/ecma_5/String/match-defines-match-elements.js
Object.defineProperty(Array.prototype, 1, {
    set: function() {}
})
// jsfunfuzz-generated
Array.prototype.splice.call(x, 3, {}, y);
new Set(x);
