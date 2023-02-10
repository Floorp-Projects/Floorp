if (typeof disassemble !== "function") {
  quit();
}

// Invalid UTF-8 should be replaced with U+FFFD.
const out = evaluate(`
disassemble();
`, {
  fileName: String.fromCharCode(3823486100),
});
assertEq(out.includes(`"file": "\uFFFD",`), true);
