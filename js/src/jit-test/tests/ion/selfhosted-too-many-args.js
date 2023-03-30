// Calling self-hosted functions that use GetArgument(i) with more arguments than
// we support for inlining.
function f() {
    var arr = [1, 2, 3];
    var add1 = x => x + 1;
    for (var i = 0; i < 2000; i++) {
        assertEq(arr.map(add1).toString(), "2,3,4");
        assertEq(arr.map(add1, 1, 2, 3).toString(), "2,3,4");
        assertEq(Reflect.get(arr, 1), 2);
        assertEq(Reflect.get(arr, 1, 2), 2);
        assertEq(Reflect.get(arr, 1, 2, 3, 4), 2);
    }
}
f();
