if (typeof disassemble !== "function") {
  quit();
}

const out = evaluate(`
disassemble();
`, {
  fileName: String.fromCharCode(3823486100),
});
assertEq(out.includes(`"file": "\uC494",`), true);
