function testOverflowOneSplat() {
    var threw = false;
    var c;
    try {
        c = (new Array(4294967295)).concat(['a']);
    } catch(e) {
        threw = true;
    }
    assertEq(threw, false);
    assertEq(c[4294967295], 'a');
    assertEq(c.length, 0);
}
testOverflowOneSplat();

function testOverflowOneArg() {
    var threw = false;
    var c;
    try {
        c = (new Array(4294967295)).concat('a');
    } catch(e) {
        threw = true;
    }
    assertEq(threw, false);
    assertEq(c[4294967295], 'a');
    assertEq(c.length, 0);
}
testOverflowOneArg();

function testOverflowManySplat() {
    var threw = false;
    var c;
    try {
        c = (new Array(4294967294)).concat(['a', 'b', 'c']);
    } catch(e) {
        threw = true;
    }
    assertEq(threw, false);
    assertEq(c[4294967294], 'a');
    assertEq(c[4294967295], 'b');
    assertEq(c[4294967296], 'c');
    assertEq(c.length, 4294967295);
}
testOverflowManySplat();

function testOverflowManyArg() {
    var threw = false;
    var c;
    try {
        c = (new Array(4294967294)).concat('a', 'b', 'c');
    } catch(e) {
        threw = true;
    }
    assertEq(threw, false);
    assertEq(c[4294967294], 'a');
    assertEq(c[4294967295], 'b');
    assertEq(c[4294967296], 'c');
    assertEq(c.length, 4294967295);
}
testOverflowManyArg();

/*
 * Ensure we run for side-effects, even when overflowing.
 */
var count;
function MakeFunky() {
    var o = new Array();
    Object.defineProperty(o, "0", { get: function() { count++; return 'a'; } });
    o.length = 1;
    return o;
}

function testReadWhenOverflowing() {
    count = 0;
    var threw = false;
    var c;
    try {
        c = (new Array(4294967294)).concat(MakeFunky(), MakeFunky(), MakeFunky());
    } catch(e) {
        threw = true;
    }
    assertEq(threw, false);
    assertEq(c[4294967294], 'a');
    assertEq(c[4294967295], 'a');
    assertEq(c[4294967296], 'a');
    assertEq(c.length, 4294967295);
    assertEq(count, 3);
}
testReadWhenOverflowing();

function testDenseFastpathCallsGetters() {
    count = 0;
    var threw = false;
    var c;
    try {
        c = MakeFunky().concat([]);
    } catch(e) {
        threw = true;
    }
    assertEq(threw, false);
    assertEq(c[0], 'a');
    assertEq(c.length, 1);
    assertEq(count, 1);
}
testDenseFastpathCallsGetters();
