load(libdir + "asserts.js");

function testPopSimple() {
    var a = [4, 3, 2, 1, 0];
    Object.preventExtensions(a);
    for (var i = 0; i < 5; i++)
        assertEq(a.pop(), i);
    assertEq(a.length, 0);

    a = [1, 2, 3];
    Object.seal(a);
    for (var i = 0; i < 10; i++)
        assertThrowsInstanceOf(() => a.pop(), TypeError);
    assertEq(a.toString(), "1,2,3");

    a = [1, 2, 3];
    Object.freeze(a);
    for (var i = 0; i < 10; i++)
        assertThrowsInstanceOf(() => a.pop(), TypeError);
    assertEq(a.toString(), "1,2,3");
}
testPopSimple();

function testPopHoles() {
    var a = [1, , 3];
    Object.preventExtensions(a);
    assertEq(a.pop(), 3);
    assertEq(a.pop(), undefined);
    assertEq(a.pop(), 1);
    assertEq(a.length, 0);

    a = [1, ,];
    Object.seal(a);
    assertEq(a.pop(), undefined);
    for (var i = 0; i < 10; i++)
        assertThrowsInstanceOf(() => a.pop(), TypeError);
    assertEq(a.toString(), "1");

    a = [1, ,];
    Object.freeze(a);
    for (var i = 0; i < 10; i++)
        assertThrowsInstanceOf(() => a.pop(), TypeError);
    assertEq(a.toString(), "1,");
}
testPopHoles();

function testPushSimple() {
    var a = [1, 2, 3];
    Object.preventExtensions(a);
    for (var i = 0; i < 10; i++)
	assertThrowsInstanceOf(() => a.push(4), TypeError);
    assertEq(a.length, 3);
    assertEq(a.toString(), "1,2,3");

    a = [1, 2, 3];
    Object.seal(a);
    for (var i = 0; i < 10; i++)
	assertThrowsInstanceOf(() => a.push(4), TypeError);
    assertEq(a.toString(), "1,2,3");

    a = [1, 2, 3];
    Object.freeze(a);
    for (var i = 0; i < 10; i++)
	assertThrowsInstanceOf(() => a.push(4), TypeError);
    assertEq(a.toString(), "1,2,3");
}
testPushSimple();

function testPushHoles() {
    var a = [,,,];
    Object.preventExtensions(a);
    for (var i = 0; i < 10; i++)
	assertThrowsInstanceOf(() => a.push(4), TypeError);
    assertEq(a.length, 3);
    assertEq(a.toString(), ",,");

    a = [,,,];
    Object.seal(a);
    for (var i = 0; i < 10; i++)
	assertThrowsInstanceOf(() => a.push(4), TypeError);
    assertEq(a.toString(), ",,");

    a = [,,,];
    Object.freeze(a);
    for (var i = 0; i < 10; i++)
	assertThrowsInstanceOf(() => a.push(4), TypeError);
    assertEq(a.toString(), ",,");
}
testPushHoles();

function testReverseSimple() {
    var a = [1, 2, 3];
    Object.preventExtensions(a);
    a.reverse();
    assertEq(a.toString(), "3,2,1");

    a = [1, 2, 3];
    Object.seal(a);
    a.reverse();
    assertEq(a.toString(), "3,2,1");

    a = [1, 2, 3];
    Object.freeze(a);
    assertThrowsInstanceOf(() => a.reverse(), TypeError);
    assertEq(a.toString(), "1,2,3");
}
testReverseSimple();

function testReverseHoles() {
    var a = [1, 2, , 4];
    Object.preventExtensions(a);
    assertThrowsInstanceOf(() => a.reverse(), TypeError);
    assertEq(a.toString(), "4,,,1");

    a = [1, 2, , 4];
    Object.seal(a);
    assertThrowsInstanceOf(() => a.reverse(), TypeError);
    assertEq(a.toString(), "4,2,,1");

    a = [1, 2, , 4];
    Object.freeze(a);
    assertThrowsInstanceOf(() => a.reverse(), TypeError);
    assertEq(a.toString(), "1,2,,4");
}
testReverseHoles();

function testShiftSimple() {
    var a = [1, 2, 3];
    Object.preventExtensions(a);
    assertEq(a.shift(), 1);
    assertEq(a.toString(), "2,3");
    for (var i = 0; i < 10; i++)
	a.shift();
    assertEq(a.length, 0);

    a = [1, 2, 3];
    Object.seal(a);
    for (var i = 0; i < 10; i++)
	assertThrowsInstanceOf(() => a.shift(), TypeError);
    assertEq(a.toString(), "3,3,3");

    a = [1, 2, 3];
    Object.freeze(a);
    for (var i = 0; i < 10; i++)
	assertThrowsInstanceOf(() => a.shift(), TypeError);
    assertEq(a.toString(), "1,2,3");
}
testShiftSimple();
testShiftSimple();

function testShiftHoles() {
    var a = [1, 2, , 4];
    Object.preventExtensions(a);
    for (var i = 0; i < 10; i++)
	assertThrowsInstanceOf(() => a.shift(), TypeError);
    assertEq(a.toString(), ",,,4");

    a = [1, 2, , 4];
    Object.seal(a);
    for (var i = 0; i < 10; i++)
	assertThrowsInstanceOf(() => a.shift(), TypeError);
    assertEq(a.toString(), "2,2,,4");

    a = [1, 2, , 4];
    Object.freeze(a);
    for (var i = 0; i < 10; i++)
	assertThrowsInstanceOf(() => a.shift(), TypeError);
    assertEq(a.toString(), "1,2,,4");
}
testShiftHoles();
testShiftHoles();

function testUnshiftSimple() {
    var a = [1, 2, 3];
    Object.preventExtensions(a);
    assertThrowsInstanceOf(() => a.unshift(0), TypeError);
    assertEq(a.toString(), "1,2,3");

    a = [1, 2, 3];
    Object.seal(a);
    assertThrowsInstanceOf(() => a.unshift(0), TypeError);
    assertEq(a.toString(), "1,2,3");

    a = [1, 2, 3];
    Object.freeze(a);
    assertThrowsInstanceOf(() => a.unshift(0), TypeError);
    assertEq(a.toString(), "1,2,3");
}
testUnshiftSimple();

function testUnshiftHoles() {
    var a = [,,,];
    Object.preventExtensions(a);
    assertThrowsInstanceOf(() => a.unshift(0), TypeError);
    assertEq(a.toString(), ",,");

    a = [,,,];
    Object.seal(a);
    assertThrowsInstanceOf(() => a.unshift(0), TypeError);
    assertEq(a.toString(), ",,");

    a = [,,,];
    Object.freeze(a);
    assertThrowsInstanceOf(() => a.unshift(0), TypeError);
    assertEq(a.toString(), ",,");
}
testUnshiftHoles();

function testSpliceDelete() {
    var a = [1, 2, 3];
    Object.preventExtensions(a);
    a.splice(1, 2);
    assertEq(a.toString(), "1");

    a = [1, 2, 3];
    Object.seal(a);
    assertThrowsInstanceOf(() => a.splice(1, 2), TypeError);
    assertEq(a.toString(), "1,2,3");

    a = [1, 2, 3];
    Object.freeze(a);
    assertThrowsInstanceOf(() => a.splice(1, 2), TypeError);
    assertEq(a.toString(), "1,2,3");
}
testSpliceDelete();

function testSpliceAdd() {
    var a = [1, 2, 3];
    Object.preventExtensions(a);
    assertThrowsInstanceOf(() => a.splice(2, 1, 4, 5), TypeError);
    assertEq(a.toString(), "1,2,4");

    a = [1, 2, 3];
    Object.seal(a);
    assertThrowsInstanceOf(() => a.splice(2, 1, 4, 5), TypeError);
    assertEq(a.toString(), "1,2,4");

    a = [1, 2, 3];
    Object.freeze(a);
    assertThrowsInstanceOf(() => a.splice(2, 1, 4, 5), TypeError);
    assertEq(a.toString(), "1,2,3");
}
testSpliceAdd();

function testSortSimple() {
    var a = [3, 1, 2];
    Object.preventExtensions(a);
    a.sort();
    assertEq(a.toString(), "1,2,3");

    a = [3, 1, 2];
    Object.seal(a);
    a.sort();
    assertEq(a.toString(), "1,2,3");

    a = [3, 1, 2];
    Object.freeze(a);
    assertThrowsInstanceOf(() => a.sort(), TypeError);
    assertEq(a.toString(), "3,1,2");
}
testSortSimple();

function testSortHoles() {
    var a = [, , 3, 1, 2];
    Object.preventExtensions(a);
    assertThrowsInstanceOf(() => a.sort(), TypeError);
    assertEq(a.toString(), ",,3,1,2");

    a = [, , 3, 1, 2];
    Object.seal(a);
    assertThrowsInstanceOf(() => a.sort(), TypeError);
    assertEq(a.toString(), ",,3,1,2");

    a = [, , 3, 1, 2];
    Object.freeze(a);
    assertThrowsInstanceOf(() => a.sort(), TypeError);
    assertEq(a.toString(), ",,3,1,2");
}
testSortHoles();
