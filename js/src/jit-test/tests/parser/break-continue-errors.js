load(libdir + "asserts.js");

function test(s, expected) {
    assertErrorMessage(() => Function(s), SyntaxError, expected);
}

test("A: continue;", "continue must be inside loop");
test("A: continue B;", "continue must be inside loop");
test("A: if (false) { continue; }", "continue must be inside loop");
test("A: if (false) { continue B; }", "continue must be inside loop");
test("while (false) { (() => { continue B; })(); }", "continue must be inside loop");
test("B: while (false) { (() => { continue B; })(); }", "continue must be inside loop");

test("do { continue B; } while (false);", "label not found");
test("A: for (;;) { continue B; }", "label not found");
test("A: while (false) { if (x) { continue B; } }", "label not found");
test("A: while (false) { B: if (x) { continue B; } }", "label not found");

test("A: if (false) { break B; }", "label not found");
test("A: while (false) { break B; }", "label not found");
test("A: while (true) { if (x) { break B; } }", "label not found");
test("B: while (false) { (() => { break B; })(); }", "label not found");
