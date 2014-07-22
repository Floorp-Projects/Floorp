function testBasic() {
    // Latin1
    var s = '[1, 2, "foo", "bar\\r\\n", {"xyz": 3}, [1, 2, 3]]';
    assertEq(isLatin1(s), true);
    assertEq(JSON.stringify(JSON.parse(s)), '[1,2,"foo","bar\\r\\n",{"xyz":3},[1,2,3]]');

    // TwoByte
    s = '[1, 2, "foo\u1200", "bar\\r\\n", {"xyz": 3}, [1, 2, 3]]';
    assertEq(JSON.stringify(JSON.parse(s)), '[1,2,"foo\u1200","bar\\r\\n",{"xyz":3},[1,2,3]]');
}
testBasic();

function testErrorPos() {
    // Make sure the error location is calculated correctly.

    // Latin1
    var s = '[1, \n2,';
    try {
	JSON.parse(s);
	assertEq(0, 1);
    } catch(e) {
	assertEq(e instanceof SyntaxError, true);
	assertEq(e.toString().contains("line 2 column 3"), true);
    }

    s = '[1, "\u1300",\n2,';
    try {
	JSON.parse(s);
	assertEq(0, 1);
    } catch(e) {
	assertEq(e instanceof SyntaxError, true);
	assertEq(e.toString().contains("line 2 column 3"), true);
    }
}
testErrorPos();

function testEvalHack() {
    // Latin1
    var arr = eval("[1, 2, 3, \"abc\"]");
    assertEq(JSON.stringify(arr), '[1,2,3,"abc"]');
    assertEq(isLatin1(JSON.stringify(arr)), true);

    // TwoByte
    arr = eval("[1, 2, 3, \"abc\u1200\"]");
    assertEq(JSON.stringify(arr), '[1,2,3,"abc\u1200"]');
}
testEvalHack();

function testEvalHackNotJSON() {
    // Latin1
    var arr = eval("[]; var q; [1, 2, 3, \"abc\"]");
    assertEq(JSON.stringify(arr), '[1,2,3,"abc"]');

    // TwoByte
    arr = eval("[]; var z; [1, 2, 3, \"abc\u1200\"]");
    assertEq(JSON.stringify(arr), '[1,2,3,"abc\u1200"]');

    try {
	eval("[1, 2, 3, \"abc\u2028\"]");
	throw new Error("U+2028 shouldn't eval");
    } catch (e) {
	assertEq(e instanceof SyntaxError, true,
		 "should have thrown a SyntaxError, instead got " + e);
    }
}
testEvalHackNotJSON();

function testQuote() {
    // Latin1
    var s = "abc--\x05-'\"-\n-\u00ff++";
    var res = JSON.stringify(s);
    assertEq(res, '"abc--\\u0005-\'\\"-\\n-\xFF++"');
    assertEq(isLatin1(res), true);

    // TwoByte
    s += "\uAAAA";
    assertEq(JSON.stringify(s), '"abc--\\u0005-\'\\"-\\n-\xFF++\uAAAA"');
}
testQuote();
