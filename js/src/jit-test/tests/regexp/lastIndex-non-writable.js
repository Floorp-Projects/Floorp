function testGlobalExec() {
    var re = /abc.+de/g;
    var c = 0;
    for (var i = 0; i < 100; i++) {
        re.lastIndex = 0;
        if (i === 60) {
            Object.freeze(re);
        }
        try {
            re.exec("abcXdef");
        } catch (e) {
            assertEq(e.toString().includes("lastIndex"), true);
            c++;
        }
        assertEq(re.lastIndex, i >= 60 ? 0 : 6);
    }
    assertEq(c, 40);
}
testGlobalExec();

function testStickyTest() {
    var re = /abc.+de/y;
    var c = 0;
    for (var i = 0; i < 100; i++) {
        re.lastIndex = 0;
        if (i === 60) {
            Object.freeze(re);
        }
        try {
            re.test("abcXdef");
        } catch (e) {
            assertEq(e.toString().includes("lastIndex"), true);
            c++;
        }
        assertEq(re.lastIndex, i >= 60 ? 0 : 6);
    }
    assertEq(c, 40);
}
testStickyTest();

// Must not reset too-large .lastIndex to 0 if non-writable.
function testLastIndexOutOfRange() {
    var re = /abc.+de/g;
    re.lastIndex = 123;
    Object.freeze(re);
    for (var i = 0; i < 100; i++) {
        var ex = null;
        try {
            re.exec("abcXdef");
        } catch (e) {
            ex = e;
        }
        assertEq(ex.toString().includes("lastIndex"), true);
        assertEq(re.lastIndex, 123);
    }
}
testLastIndexOutOfRange();

// .lastIndex is ignored for non-global/non-sticky regexps.
function testPlainExec() {
    var re = /abc.+de/;
    re.lastIndex = 1234;
    for (var i = 0; i < 100; i++) {
        if (i === 60) {
            Object.freeze(re);
        }
        assertEq(re.exec("abcXdef")[0], "abcXde");
        assertEq(re.lastIndex, 1234);
    }
}
testPlainExec();
