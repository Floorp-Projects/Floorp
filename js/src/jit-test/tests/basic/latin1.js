var s = "Foo123";
assertEq(isLatin1(s), false);

s = toLatin1(s);
assertEq(isLatin1(s), true);

function testEq(s) {
    assertEq(isLatin1(s), true);
    assertEq(s === "foo02", false);
    assertEq(s == "foo02", false);

    // Non-atomized to force char comparison.
    var nonAtomized = "\u1234foo01\u00c7".substr(1);
    assertEq(isLatin1(nonAtomized), false);
    assertEq(s === nonAtomized, true);
    assertEq(nonAtomized !== s, false);
    assertEq(nonAtomized == s, true);
    assertEq(s, nonAtomized);

    nonAtomized = "\u1234foo02".substr(1);
    assertEq(isLatin1(nonAtomized), false);
    assertEq(s === nonAtomized, false);
    assertEq(nonAtomized == s, false);
}

s = "foo01\u00c7";
s = toLatin1(s);
testEq(s);
testEq(s);
