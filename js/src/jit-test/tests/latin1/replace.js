function testDollarReplacement() {
    // Latin1 input, pat and replacement
    var s = toLatin1("Foobarbaz123");
    var pat = toLatin1("bar");
    assertEq(s.replace(pat, toLatin1("AA")), "FooAAbaz123");
    assertEq(s.replace(pat, toLatin1("A$$A")), "FooA$Abaz123");
    assertEq(s.replace(pat, toLatin1("A$`A")), "FooAFooAbaz123");
    assertEq(s.replace(pat, toLatin1("A$&A")), "FooAbarAbaz123");
    assertEq(s.replace(pat, toLatin1("A$'A")), "FooAbaz123Abaz123");

    // Latin1 input and pat, TwoByte replacement
    assertEq(s.replace(pat, "A\u1200"), "FooA\u1200baz123");
    assertEq(s.replace(pat, "A$$\u1200"), "FooA$\u1200baz123");
    assertEq(s.replace(pat, "A$`\u1200"), "FooAFoo\u1200baz123");
    assertEq(s.replace(pat, "A$&\u1200"), "FooAbar\u1200baz123");
    assertEq(s.replace(pat, "A$'\u1200"), "FooAbaz123\u1200baz123");

    // TwoByte input, Latin1 pat and replacement
    s = "Foobarbaz123\u1200";
    assertEq(s.replace(pat, toLatin1("A")), "FooAbaz123\u1200");
    assertEq(s.replace(pat, toLatin1("A$$")), "FooA$baz123\u1200");
    assertEq(s.replace(pat, toLatin1("A$`")), "FooAFoobaz123\u1200");
    assertEq(s.replace(pat, toLatin1("A$&")), "FooAbarbaz123\u1200");
    assertEq(s.replace(pat, toLatin1("A$'")), "FooAbaz123\u1200baz123\u1200");

    // TwoByte input and pat, Latin1 replacement
    s = "Foobar\u1200baz123";
    pat += "\u1200";
    assertEq(s.replace(pat, toLatin1("AB")), "FooABbaz123");
    assertEq(s.replace(pat, toLatin1("A$$B")), "FooA$Bbaz123");
    assertEq(s.replace(pat, toLatin1("A$`B")), "FooAFooBbaz123");
    assertEq(s.replace(pat, toLatin1("A$&B")), "FooAbar\u1200Bbaz123");
    assertEq(s.replace(pat, toLatin1("A$'B")), "FooAbaz123Bbaz123");

    // TwoByte input, pat and replacement
    assertEq(s.replace(pat, "A\u1300"), "FooA\u1300baz123");
    assertEq(s.replace(pat, "A$$\u1300"), "FooA$\u1300baz123");
    assertEq(s.replace(pat, "A$`\u1300"), "FooAFoo\u1300baz123");
    assertEq(s.replace(pat, "A$&\u1300"), "FooAbar\u1200\u1300baz123");
    assertEq(s.replace(pat, "A$'\u1300"), "FooAbaz123\u1300baz123");
}
testDollarReplacement();
