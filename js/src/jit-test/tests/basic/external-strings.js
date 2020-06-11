assertEq(newString("", {external: true}), "");
assertEq(newString("abc", {external: true}), "abc");
assertEq(newString("abc\0def\u1234", {external: true}), "abc\0def\u1234");

var o = {foo: 2, "foo\0": 4};
var ext = newString("foo", {external: true});
assertEq(o[ext], 2);
var ext2 = newString("foo\0", {external: true});
assertEq(o[ext2], 4);

eval(newString("assertEq(1, 1)", {external: true}));

// Make sure ensureLinearString does the right thing for external strings.
ext = newString("abc\0defg\0", {external: true});
assertEq(ensureLinearString(ext), "abc\0defg\0");
assertEq(ensureLinearString(ext), "abc\0defg\0");

for (var s of representativeStringArray())
    assertEq(ensureLinearString(s), s);

for (var s of representativeStringArray())
    assertEq(newString(s, {external: true}), s);

function testQuote() {
    for (var data of [["abc", "abc"],
		      ["abc\t", "abc\\t"],
		      ["abc\t\t\0", "abc\\t\\t\\x00"],
		      ["abc\\def", "abc\\\\def"]]) {
	try {
	    assertEq(newString(data[0], {external: true}), false);
	} catch(e) {
	    assertEq(e.toString().includes('got "' + data[1] + '",'), true)
	}
    }
}
testQuote();

function testMaybeExternal() {
    for (var i=0; i<10; i++) {
        var s = "abcdef4321" + i;
        assertEq(newString(s, {maybeExternal: true}), s);
        if ((i % 2) === 0)
            assertEq(ensureLinearString(newString(s, {maybeExternal: true})), s);
    }
}
testMaybeExternal();
gc();
testMaybeExternal();
