function f1() {
    var a = new Int32Array(50);
    for (var i=0; i<100; i++) {
        var x = a[i];
        assertEq(typeof x, i < a.length ? "number" : "undefined");
    }

    var b = new Float32Array(50);
    for (var i=0; i<100; i++) {
        var x = b[i];
        assertEq(typeof x, i < b.length ? "number" : "undefined");
    }
}
f1();

function f2() {
    Object.prototype[50] = 4.4;
    Object.prototype[55] = Math;

    var a = new Int16Array(50);
    for (var i=0; i<100; i++) {
        var x = a[i];
        if (i < a.length)
            assertEq(x, 0);
        else if (i === 50)
            assertEq(x, 4.4);
        else if (i === 55)
            assertEq(x, Math);
        else
            assertEq(x, undefined);
    }
}
f2();
