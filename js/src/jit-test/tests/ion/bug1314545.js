function f() {
    Object.prototype[0] = 10;

    var arr = [];
    for (var i=3; i<20; i++) {
        arr[0] = i;
        Object.freeze(arr);
        while (!inIon()) {}
    }
    assertEq(arr[0], 3);
}
f();

function g() {
    var c = 0;
    Object.defineProperty(Object.prototype, 18, {set: function() { c++; }});

    var arrays = [];
    for (var i=0; i<2; i++)
        arrays.push([1, 2]);

    for (var i=0; i<20; i++) {
        arrays[0][i] = 1;
        arrays[1][i] = 2;
        if (i === 0)
            Object.freeze(arrays[0]);
        while (!inIon()) {}
    }
    assertEq(c, 2);
}
g();
