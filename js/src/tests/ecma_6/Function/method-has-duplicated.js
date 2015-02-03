// Make sure duplicated name is allowed in non-strict.
function f0(a) {
}

// SyntaxError should be thrown if method definition has duplicated name.
assertThrowsInstanceOf(() => eval(`
({
  m1(a, a) {
  }
});
`), SyntaxError);
assertThrowsInstanceOf(() => eval(`
({
  m2(a, ...a) {
  }
});
`), SyntaxError);

reportCompare(0, 0, 'ok');
