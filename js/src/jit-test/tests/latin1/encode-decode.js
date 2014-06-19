// Latin1
s = toLatin1("a%2b%20def%00A0");
assertEq(decodeURI(s), "a%2b def\x00A0");
assertEq(decodeURIComponent(s), "a+ def\x00A0");

// TwoByte
s += "\u1200";
assertEq(decodeURI(s), "a%2b def\x00A0\u1200");
assertEq(decodeURIComponent(s), "a+ def\x00A0\u1200");

// Latin1 malformed
try {
    decodeURI(toLatin1("abc%80"));
    assertEq(0, 1);
} catch(e) {
    assertEq(e instanceof URIError, true);
}

// TwoByte malformed
try {
    decodeURI("abc%80\u1200");
    assertEq(0, 1);
} catch(e) {
    assertEq(e instanceof URIError, true);
}

// Latin1
assertEq(encodeURI(toLatin1("a%2b def\x00A0")), "a%252b%20def%00A0");
assertEq(encodeURIComponent(toLatin1("a+ def\x00A0")), "a%2B%20def%00A0");

// TwoByte
assertEq(encodeURI("a%2b def\x00A0\u1200"), "a%252b%20def%00A0%E1%88%80");
assertEq(encodeURIComponent("a+ def\x00A0\u1200"), "a%2B%20def%00A0%E1%88%80");

// TwoByte malformed
try {
    encodeURI("a\uDB00");
    assertEq(0, 1);
} catch(e) {
    assertEq(e instanceof URIError, true);
}
