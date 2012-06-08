function escapeme() {}

function f1(x) {
    escapeme(arguments);
    var y = ++x;
    return x + y;
}
for (var i = 0; i < 100; ++i)
    assertEq(f1(2), 6);

function f2(x) {
    escapeme(arguments);
    var y = --x;
    return x + y;
}
for (var i = 0; i < 100; ++i)
    assertEq(f2(2), 2);

function f3(x) {
    escapeme(arguments);
    var y = x++;
    return x + y;
}
for (var i = 0; i < 100; ++i)
    assertEq(f3(2), 5);

function f4(x) {
    escapeme(arguments);
    var y = x--;
    return x + y;
}
for (var i = 0; i < 100; ++i)
    assertEq(f4(2), 3);
