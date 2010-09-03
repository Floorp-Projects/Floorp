var arr = [];

function f() {}

function g(n, h) {
    var a = f;
    if (n <= 0)
    return f;

    var t = g(n - 1, h);
    var r = function(x) {
    if (x)
        return a;
    return a(h(function() { return t(); }));
    };
    arr.push(r); 
    return r;
}

g(80, f);
g(80, f);
g(80, f);
for (var i = 0; i < arr.length; i++)
    assertEq(arr[i](1), f);
