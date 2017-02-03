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

function testQuote() {
    for (var data of [["abc", "abc"],
		      ["abc\t", "abc\\t"],
		      ["abc\t\t\0", "abc\\t\\t\\x00"],
		      ["abc\\def", "abc\\\\def"]]) {
	try {
	    assertEq(newExternalString(data[0]), false);
	} catch(e) {
	    assertEq(e.toString().includes('got "' + data[1] + '",'), true)
	}
    }
}
testQuote();
