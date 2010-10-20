function f()
{
    var a = [];
    a.length = 10;
    for (var i = 0; i < 100; i++) {
        var y = a[a.length];
    }
}
f();
// No assertEq() call, the test is just that it shouldn't assert or crash.
