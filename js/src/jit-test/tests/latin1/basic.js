assertEq(isLatin1("Foo123\u1200"), false);

s = toLatin1("Foo123");
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

function testConcat() {
    function concat(s1, s2) {
	return s1 + s2;
    }

    // Following tests create fat inline strings.
    assertEq(concat("abc", "def"), "abcdef");
    var s1 = toLatin1("ABC");
    var s2 = toLatin1("DEF");
    assertEq(concat(s1, s2), "ABCDEF");
    assertEq(concat(s1, "GHI\u0580"), "ABCGHI\u0580");
    assertEq(concat("GHI\u0580", s2), "GHI\u0580DEF");
    assertEq(concat(concat("GHI\u0580", s2), s1), "GHI\u0580DEFABC");
    assertEq(isLatin1(s1), true);
    assertEq(isLatin1(s2), true);

    // Create a Latin1 rope.
    var s3 = toLatin1("0123456789012345678901234567890123456789");
    var rope = concat(s1, s3);
    assertEq(isLatin1(rope), true);
    assertEq(rope, "ABC0123456789012345678901234567890123456789");
    assertEq(isLatin1(rope), true); // Still Latin1 after flattening.

    // Latin1 + TwoByte => TwoByte rope.
    assertEq(isLatin1(s3), true);
    rope = concat(s3, "someTwoByteString\u0580");
    assertEq(isLatin1(rope), false);
    assertEq(rope, "0123456789012345678901234567890123456789someTwoByteString\u0580");
    assertEq(isLatin1(rope), false);

    assertEq(isLatin1(s3), true);
    rope = concat("twoByteString\u0580", concat(s3, "otherTwoByte\u0580"));
    assertEq(isLatin1(rope), false);
    assertEq(rope, "twoByteString\u05800123456789012345678901234567890123456789otherTwoByte\u0580");
    assertEq(isLatin1(rope), false);

    // Build a Latin1 rope with left-most string an extensible string.
    var s4 = toLatin1("adsfasdfjkasdfkjasdfasasdfasdf");
    for (var i=0; i<5; i++) {
	s4 = concat(s4, s1);
	assertEq(s4 === ".".repeat(s4.length), false); // Flatten rope.
    }

    assertEq(isLatin1(s4), true);

    // Appending a TwoByte string must inflate.
    s4 = concat(s4, "--\u0580");
    assertEq(s4, "adsfasdfjkasdfkjasdfasasdfasdfABCABCABCABCABC--\u0580");
}
testConcat();

function testFlattenDependent() {
    function concat(s1, s2) {
	return s1 + s2;
    }

    // Create some latin1 strings.
    var s1 = toLatin1("Foo0123456789bar012345---");
    var s2 = toLatin1("Foo0123456789bar012345+++");
    assertEq(isLatin1(s1), true);
    assertEq(isLatin1(s2), true);

    // And some ropes.
    var rope1 = concat(s1, s1);
    assertEq(isLatin1(rope1), true);

    var rope2 = concat(rope1, s2);
    assertEq(isLatin1(rope2), true);

    var rope3 = concat("twoByte\u0581", rope2);
    assertEq(isLatin1(rope3), false);

    // Flatten everything.
    assertEq(rope3, "twoByte\u0581Foo0123456789bar012345---Foo0123456789bar012345---Foo0123456789bar012345+++");
    assertEq(isLatin1(rope3), false);

    // rope1 and rope2 were Latin1, but flattening rope3 turned them into
    // dependent strings, so rope1 and rope2 must also be TwoByte now.
    assertEq(isLatin1(rope1), false);
    assertEq(isLatin1(rope2), false);

    assertEq(rope1, "Foo0123456789bar012345---Foo0123456789bar012345---");
    assertEq(rope2, "Foo0123456789bar012345---Foo0123456789bar012345---Foo0123456789bar012345+++");
}
testFlattenDependent();
