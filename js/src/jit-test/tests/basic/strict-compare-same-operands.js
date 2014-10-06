function f(l, m) {
    var a = NaN;
    var b = 13;
    var c = "test";
    var d = undefined;
    var e = null;
    var f = 15.7;
    var g = Math.fround(189777.111);
    var h = "ABC";
    var i = String.fromCharCode(65, 65, 65);
    var j = {};
    var k = Math.fround("".charCodeAt(15));

    // Special case rigt here:
    assertEq(a === a, false);
    assertEq(a !== a, true);
    assertEq(k === k, false);
    assertEq(k !== k, true);
    assertEq(l === l, false);
    assertEq(l !== l, true);

    assertEq(b === b, true);
    assertEq(b !== b, false);
    assertEq(c === c, true);
    assertEq(c !== c, false);
    assertEq(d === d, true);
    assertEq(d !== d, false);
    assertEq(e === e, true);
    assertEq(e !== e, false);
    assertEq(f === f, true);
    assertEq(f !== f, false);
    assertEq(g === g, true);
    assertEq(g !== g, false);
    assertEq(h === h, true);
    assertEq(h !== h, false);
    assertEq(i === i, true);
    assertEq(i !== i, false);
    assertEq(j === j, true);
    assertEq(j !== j, false);
    assertEq(m === m, true);
    assertEq(m !== m, false);
}

function test() {
    for (var i = 0; i < 100; i++)
        f("".charCodeAt(15), 42);
}

test();
