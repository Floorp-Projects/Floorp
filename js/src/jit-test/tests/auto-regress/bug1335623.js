// jsfunfuzz-generated
x = [];
// Adapted from randomly chosen test: js/src/tests/ecma_5/String/match-defines-match-elements.js
y = Object.defineProperty(Array.prototype, 1, {
    set: function () {}
});
// jsfunfuzz-generated
x.splice(0, 1, y, y);
new Float64Array(x);
