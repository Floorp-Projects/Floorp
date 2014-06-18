s = toLatin1("a%2b%20def%00A0");
assertEq(decodeURI(s), "a%2b def\x00A0");
assertEq(decodeURIComponent(s), "a+ def\x00A0");

s += "\u1200";
assertEq(decodeURI(s), "a%2b def\x00A0\u1200");
assertEq(decodeURIComponent(s), "a+ def\x00A0\u1200");

try {
    decodeURI(toLatin1("abc%80"));
    assertEq(0, 1);
} catch(e) {
    assertEq(e instanceof URIError, true);
}
try {
    decodeURI("abc%80\u1200");
    assertEq(0, 1);
} catch(e) {
    assertEq(e instanceof URIError, true);
}
