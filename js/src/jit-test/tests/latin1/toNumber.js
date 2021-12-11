function toLatin1(s) {
    assertEq(isLatin1(s), true);
    return s;
}
function testToNumber() {
    // Latin1
    assertEq(+toLatin1("12345.6"), 12345.6);
    assertEq(+toLatin1("+123"), 123);
    assertEq(+toLatin1("0xABC"), 0xABC);
    assertEq(+toLatin1("112."), 112);
    assertEq(+toLatin1("112.A"), NaN);
    assertEq(+toLatin1("-Infinity"), -Infinity);

    // TwoByte
    function twoByte(s) {
	s = "\u1200" + s;
	s = s.substr(1);
	assertEq(isLatin1(s), false);
	return s;
    }
    assertEq(+twoByte("12345.6"), 12345.6);
    assertEq(+twoByte("+123"), 123);
    assertEq(+twoByte("0xABC"), 0xABC);
    assertEq(+twoByte("112."), 112);
    assertEq(+twoByte("112.A"), NaN);
    assertEq(+twoByte("-Infinity"), -Infinity);
}
testToNumber();
