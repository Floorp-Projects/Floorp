function f() {
    var arr = [];
    for (var i=0; i<80; i++) {
        var o3 = {foo: i};
        var o2 = {owner: o3};
        arr.push(o2);
    }
    for (var i=0; i<80; i++) {
        var o2 = arr[i];
        var o3 = o2.owner;
        Object.defineProperty(o3, "bar", {value: arr, enumerable: false});
    }
    assertEq(JSON.stringify(arr).length, 1671);
}
f();

function g() {
    var arr = [];
    for (var i=0; i<100; i++) {
        arr.push([1, i]);
    }
    for (var i=0; i<100; i++) {
        for (var p in arr[i]) {
            assertEq(p === "0" || p === "1", true);
        }
    }
}
g();
