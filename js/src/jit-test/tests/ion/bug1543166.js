function f() {
    var arr = [];
    for (var i = 0; i < 12; i++) {
        // Create a new global to get "DOM" objects with different groups.
        var g = newGlobal();
        var o = new g.FakeDOMObject();
        o[0] = 1;
        arr.push(o);
    }
    var res;
    for (var i = 0; i < 2000; i++) {
        var o = arr[i % arr.length];
        res = o[0];
    }
    return res;
}
f();
