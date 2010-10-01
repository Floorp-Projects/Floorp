var x = 42;

function f() {
    var a = [{}, {}, {}, {}, {}];
    for (var i = 0; i < 5; i++)
        a[i].m = function () {return x};
    for (i = 0; i < 4; i++)
        if (a[i].m == a[i+1].m)
            throw "FAIL!";
}
f();
