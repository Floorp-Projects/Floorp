function f() {
    var a = [];
    for (var i=0; i<100; i++)
        a.push({x: i});

    var vals = [1, "", true, null];

    for (var j=0; j<100; j++) {
        var v = vals[j % vals.length];
        a[95].y = v;
        assertEq(a[95].y, v);
    }
}
f();
