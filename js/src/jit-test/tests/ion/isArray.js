function f() {
    assertEq(Array.isArray(10), false);
    assertEq(Array.isArray([]), true);
    assertEq(Array.isArray(Math), false);

    var objs = [{}, []];
    for (var i=0; i<objs.length; i++)
        assertEq(Array.isArray(objs[i]), i === 1);
    var arr = [[], [], 1];
    for (var i=0; i<arr.length; i++)
        assertEq(Array.isArray(arr[i]), i < 2);
}
f();
f();
f();
