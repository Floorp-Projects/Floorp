// Bug 1770609 - Ensure the line number reported is correctly the source of the
//               syntax error, not just the first observation of the name.
try {
    eval(`class A { #declared; }
    m.#declared;`)
} catch (e) {
    assertEq(e instanceof SyntaxError, true)
    assertEq(e.lineNumber, 2);
}

