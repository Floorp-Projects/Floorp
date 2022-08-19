let hasStringRepresentation = typeof stringRepresentation === "function";

function testLatin1() {
  var s = "abcdefghijklmnopqrstuvwxyz";
  for (var i = 0; i < 100; i++) {
    var sub8 = s.substring(0, 8);
    assertEq(sub8, "abcdefgh");
    if (hasStringRepresentation) {
      assertEq(stringRepresentation(sub8).startsWith("((JSThinInlineString"), true);
    }

    var sub24 = s.substring(0, 24);
    assertEq(sub24, "abcdefghijklmnopqrstuvwx");
    if (hasStringRepresentation) {
      assertEq(stringRepresentation(sub24).startsWith("((JSFatInlineString"), true);
    }

    var sub25 = s.substring(0, 25);
    assertEq(sub25, "abcdefghijklmnopqrstuvwxy");
    if (hasStringRepresentation) {
      assertEq(stringRepresentation(sub25).startsWith("((JSDependentString"), true);
    }
  }
}
testLatin1();

function testTwoByte() {
  var s = "\u1000bcdefghijklmnopqrstuvwxyz";
  for (var i = 0; i < 100; i++) {
    var sub4 = s.substring(0, 4);
    assertEq(sub4, "\u1000bcd");
    if (hasStringRepresentation) {
      assertEq(stringRepresentation(sub4).startsWith("((JSThinInlineString"), true);
    }

    var sub12 = s.substring(0, 12);
    assertEq(sub12, "\u1000bcdefghijkl");
    if (hasStringRepresentation) {
      assertEq(stringRepresentation(sub12).startsWith("((JSFatInlineString"), true);
    }

    var sub13 = s.substring(0, 13);
    assertEq(sub13, "\u1000bcdefghijklm");
    if (hasStringRepresentation) {
      assertEq(stringRepresentation(sub13).startsWith("((JSDependentString"), true);
    }
  }
}
testTwoByte();
