var asmdiv2 = (function(m) {
    "use asm"
    function f(x) {
        x = x|0;
        var z = 0;
        z = ((x>>>0) / 2)>>>0;
        return z|0;
    }
    return f;
})()

var plaindiv2 = function(x) {
    x = x|0;
    var z = 0;
    z = ((x>>>0) / 2)>>>0;
    return z|0;
}

var k2 = 0xf0000000;

assertEq(asmdiv2(k2), 0x78000000);
assertEq(plaindiv2(k2), 0x78000000);

var asmdiv3 = (function(m) {
    "use asm"
    function f(x) {
        x = x|0;
        var z = 0;
        z = ((x>>>0) / 3)>>>0;
        return z|0;
    }
    return f;
})()

var plaindiv3 = function(x) {
    x = x|0;
    var z = 0;
    z = ((x>>>0) / 3)>>>0;
    return z|0;
}

var k3 = 3<<30;

assertEq(asmdiv3(k3), 1<<30);
assertEq(plaindiv3(k3), 1<<30);

var asmdiv7 = (function(m) {
    "use asm"
    function f(x) {
        x = x|0;
        var z = 0;
        z = ((x>>>0) / 7)>>>0;
        return z|0;
    }
    return f;
})()

var plaindiv7 = function(x) {
    x = x|0;
    var z = 0;
    z = ((x>>>0) / 7)>>>0;
    return z|0;
}

var k7 = (1<<29)*7 + 4;

assertEq(asmdiv7(k7), 1<<29);
assertEq(plaindiv7(k7), 1<<29);

var asmmod3 = (function(m) {
    "use asm"
    function f(x) {
        x = x|0;
        var z = 0;
        z = ((x>>>0) % 3)>>>0;
        return z|0;
    }
    return f;
})()

var plainmod3 = function(x) {
    x = x|0;
    var z = 0;
    z = ((x>>>0) % 3)>>>0;
    return z|0;
}

var kmod = (3<<30) + 2;

assertEq(asmmod3(kmod), 2);
assertEq(plainmod3(kmod), 2);
