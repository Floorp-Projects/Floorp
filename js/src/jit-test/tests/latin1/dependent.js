function testSubstrLatin1() {
    var s1 = toLatin1("abcdefghijklmnopqrstuvwxyz12345678900000a");

    // Static strings.
    assertEq(s1.substr(s1.length - 1), "a");
    assertEq(s1.substr(s1.length - 2), "0a");
    assertEq(s1.substr(26, 3), "123");

    // (Fat) inline strings.
    var s2 = s1.substr(3, 5);
    assertEq(isLatin1(s2), true);
    assertEq(s2, "defgh");
    s2 = s1.substr(0, 20);
    assertEq(isLatin1(s2), true);
    assertEq(s2, "abcdefghijklmnopqrst");

    // Dependent string.
    s2 = s1.substr(1, s1.length - 2);
    assertEq(isLatin1(s2), true);
    assertEq(s2.length, 39);
    assertEq(s2, "bcdefghijklmnopqrstuvwxyz12345678900000");

    s2 = s2.substr(2).substr(1);
    assertEq(isLatin1(s2), true);
    assertEq(s2, "efghijklmnopqrstuvwxyz12345678900000");
}
testSubstrLatin1();

function testSubstrTwoByte() {
    // Two byte string.
    var s1 = "abcdefghijklmnopqrstuvwxyz12345678900000a\u1480";
    assertEq(isLatin1(s1), false);

    // Static string.
    var s2 = s1.substr(28, 1);
    assertEq(s2, "3");

    // Inline string.
    s2 = s1.substr(3, 5);
    assertEq(s2, "defgh");

    // Dependent string.
    s2 = s1.substr(2);
    assertEq(isLatin1(s2), false);
    assertEq(s2, "cdefghijklmnopqrstuvwxyz12345678900000a\u1480");

    s2 = s2.substr(2).substr(1);
    assertEq(isLatin1(s2), false);
    assertEq(s2, "fghijklmnopqrstuvwxyz12345678900000a\u1480");
}
testSubstrTwoByte();

function testSubstring() {
    var s1 = toLatin1("abcdefghijklmnopqrstuvwxyz123456789000ab");
    var s2 = s1.substring(1, 8);
    assertEq(isLatin1(s2), true);
    assertEq(s2, "bcdefgh");
    s2 = s1.substring(0, s1.length - 1);
    assertEq(isLatin1(s2), true);
    assertEq(s2, "abcdefghijklmnopqrstuvwxyz123456789000a");
}
testSubstring();

function testSlice() {
    var s1 = toLatin1("abcdefghijklmnopqrstuvwxyz123456789000ABC");
    var s2 = s1.slice(1, 8);
    assertEq(isLatin1(s2), true);
    assertEq(s2, "bcdefgh");
    s2 = s1.slice(0, -2);
    assertEq(isLatin1(s2), true);
    assertEq(s2, "abcdefghijklmnopqrstuvwxyz123456789000A");
}
testSlice();
