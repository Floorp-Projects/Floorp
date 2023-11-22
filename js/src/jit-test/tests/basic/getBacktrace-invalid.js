const backtrace = evaluate(`
this.getBacktrace(this);
`, { fileName: "\u86D9" });

assertEq(backtrace.includes(`["\u86D9":2:6]`), true);
