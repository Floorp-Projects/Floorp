// Make sure duplicated name is allowed in non-strict.
function f0(a, a) {
}

// SyntaxError should be thrown if arrow function has duplicated name.
assertThrowsInstanceOf(() => eval(`
(a, a) => {
};
`), SyntaxError);
assertThrowsInstanceOf(() => eval(`
(a, ...a) => {
};
`), SyntaxError);

reportCompare(0, 0, 'ok');
