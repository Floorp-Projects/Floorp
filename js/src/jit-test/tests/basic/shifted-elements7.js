function test1() {
    var a = [];
    for (var i = 0; i < 100; i++)
        a.unshift("foo" + i);
    for (var i = 99; i >= 0; i--) {
        assertEq(a.shift(), "foo" + i);
        a.unshift("foo" + (i - 1));
    }
    assertEq(a.length, 100);
}
test1();

function sum(arr) {
    var res = 0;
    for (var i = 0; i < arr.length; i++)
        res += arr[i];
    return res;
}
function test2() {
    var a = [];
    for (var i = 0; i < 200; i++)
        a.push(i);
    for (var i = 0; i < 100; i++)
        a.shift();
    for (var i = 0; i < 200; i++)
        a.unshift(i);
    assertEq(a.length, 300);
    assertEq(sum(a), 34850);
}
test2();

function test3() {
    var a = [];
    for (var i = 0; i < 200; i++)
        a.push(i);
    var toAdd = [];
    var step = 1;
    for (var i = 0; i < 2500; i += step) {
        for (var j = 0; j < step; j++)
            toAdd.unshift(i + j);
        a.unshift(...toAdd);
        step = Math.max((i / 16)|0, 1);
    }
    assertEq(a.length, 41463);
    assertEq(sum(a), 26657756);
}
test3();
