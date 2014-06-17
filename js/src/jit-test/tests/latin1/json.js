function testBasic() {
    // Latin1
    var s = toLatin1('[1, 2, "foo", "bar\\r\\n", {"xyz": 3}, [1, 2, 3]]');
    assertEq(JSON.stringify(JSON.parse(s)), '[1,2,"foo","bar\\r\\n",{"xyz":3},[1,2,3]]');

    // TwoByte
    s = '[1, 2, "foo\u1200", "bar\\r\\n", {"xyz": 3}, [1, 2, 3]]';
    assertEq(JSON.stringify(JSON.parse(s)), '[1,2,"foo\u1200","bar\\r\\n",{"xyz":3},[1,2,3]]');
}
testBasic();

function testErrorPos() {
    // Make sure the error location is calculated correctly.

    // Latin1
    var s = toLatin1('[1, \n2,');
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
    var arr = eval(toLatin1("[1, 2, 3, \"abc\"]"));
    assertEq(JSON.stringify(arr), '[1,2,3,"abc"]');

    // TwoByte
    arr = eval("[1, 2, 3, \"abc\u1200\"]");
    assertEq(JSON.stringify(arr), '[1,2,3,"abc\u1200"]');
}
testEvalHack();
