var s1 = toLatin1("abcdefg12345");
var s2 = toLatin1('foo"bar');

function test() {
    assertEq(s1.valueOf(), s1);

    assertEq(s1.bold(), "<b>abcdefg12345</b>");
    assertEq(s1.fontsize("twoByte\u1400"), '<font size="twoByte\u1400">abcdefg12345</font>');
    assertEq(s1.anchor(s1), '<a name="abcdefg12345">abcdefg12345</a>');
    assertEq(s1.link(s2), '<a href="foo&quot;bar">abcdefg12345</a>');

    assertEq(s1.concat("abc"), "abcdefg12345abc");

    var s3 = s1.concat(s1, s1);
    assertEq(isLatin1(s3), true);
    assertEq(s3, "abcdefg12345abcdefg12345abcdefg12345");

    s3 = s1.concat("twoByte\u1400");
    assertEq(isLatin1(s3), false);
    assertEq(s3, "abcdefg12345twoByte\u1400");

    assertEq(s1.codePointAt(3), 100);
    assertEq(s1.codePointAt(10), 52);
    assertEq(s1.codePointAt(12), undefined);

    s3 = s1.repeat(5);
    assertEq(s3, "abcdefg12345abcdefg12345abcdefg12345abcdefg12345abcdefg12345");
    assertEq(isLatin1(s3), true);
}
test();
test();
