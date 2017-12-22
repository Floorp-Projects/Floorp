// Make sure duplicated name is allowed in non-strict.
function f0(a, a) {
}

// SyntaxError should be thrown if rest parameter name is duplicated.
assertThrowsInstanceOf(() => eval(`
function f1(a, ...a) {
}
`), SyntaxError);

// SyntaxError should be thrown if there is a duplicated parameter.
assertThrowsInstanceOf(() => eval(`
function f2(a, a, ...b) {
}
`), SyntaxError);

reportCompare(0, 0, 'ok');
