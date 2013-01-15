function f() {
    var x = Math.pow(2, 31); // take it as argument if constant propagation comes in you way.
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 32
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 33
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 34
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 35
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 36
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 37
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 38
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 39
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 40
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 41
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 42
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 43
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 44
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 45
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 46
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 47
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 48
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 49
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 50
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 51
    x = x + x; assertEq((x + 1) | 0, 1); // 2 ** 52
    x = x + x; assertEq((x + 1) | 0, 0); // 2 ** 53
}

for (var i = 0; i <= 100000; i++)
    f();
