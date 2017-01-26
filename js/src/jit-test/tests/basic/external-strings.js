assertEq(newExternalString(""), "");
assertEq(newExternalString("abc"), "abc");
assertEq(newExternalString("abc\0def\u1234"), "abc\0def\u1234");

var o = {foo: 2, "foo\0": 4};
var ext = newExternalString("foo");
assertEq(o[ext], 2);
var ext2 = newExternalString("foo\0");
assertEq(o[ext2], 4);

eval(newExternalString("assertEq(1, 1)"));

// Make sure ensureFlat does the right thing for external strings.
ext = newExternalString("abc\0defg\0");
assertEq(ensureFlatString(ext), "abc\0defg\0");
assertEq(ensureFlatString(ext), "abc\0defg\0");
