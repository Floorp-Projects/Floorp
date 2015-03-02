
function test(x) {
    return typeof x != "object"
}

var obj = {};
var func = function() {};

assertEq(test(""), true)
assertEq(test(""), true)
assertEq(test(1), true)
assertEq(test(1), true)
assertEq(test(1.5), true)
assertEq(test(1.5), true)
assertEq(test(undefined), true)
assertEq(test(undefined), true)
assertEq(test(func), true)
assertEq(test(func), true)

function test2(x) {
    return typeof x != "string"
}

assertEq(test2(1), true)
assertEq(test2(1), true)
assertEq(test2(1.5), true)
assertEq(test2(1.5), true)
assertEq(test2(undefined), true)
assertEq(test2(undefined), true)
assertEq(test2(func), true)
assertEq(test2(func), true)
assertEq(test2(obj), true)
assertEq(test2(obj), true)

function test3(x) {
    return typeof x != "undefined"
}

assertEq(test3(1), true)
assertEq(test3(1), true)
assertEq(test3(1.5), true)
assertEq(test3(1.5), true)
assertEq(test3(func), true)
assertEq(test3(func), true)
assertEq(test3(obj), true)
assertEq(test3(obj), true)
assertEq(test(""), true)
assertEq(test(""), true)
