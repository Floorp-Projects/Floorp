function foo() { eval(); boundName += 1; }

boundName = 0;
for (var i = 0; i < 10; i++) {
  eval("var x" + i + " = 0;");
  foo();
}

// Redefine variable as const
evaluate(`
  const boundName = 0;
  for (var i = 0; i < 2; i++) {
    try { foo(); } catch {}
  }
`);
