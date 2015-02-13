// Bug 1073910
(function() {
    function roundf(y) {
        return Math.round(Math.fround(y));
    }

    var x = -1;
    assertEq(roundf(x), x);
    assertEq(roundf(x), x);

    var x = -2;
    assertEq(roundf(x), x);
    assertEq(roundf(x), x);

    var x = -1024;
    assertEq(roundf(x), x);

    var x = -14680050;
    assertEq(roundf(x), Math.fround(x));

    var x = -8388610;
    assertEq(roundf(x), Math.fround(x));
})();

// Bug 1000606
(function() {
    function f() {
        var d = Math.fround(0.4999999701976776);
        return Math.round(d);
    }
    assertEq(f(), f());

    function g() {
        var c = Math.fround(8886111);
        return Math.round(c);
    }
    assertEq(g(), g());
})();

// Bug 1124485
(function() {
    function h(x) {
        var y = Math.fround(x);
        assertEq(y, Math.pow(y, 1));
    }
    h(0);
    h(2147483647);
})();

// Bug 1122344
(function() {
    function f() {
        return Math.round(Math.fround(-13527757));
    };
    assertEq(f(), f());
})();

(function() {
    // Test values around -0.5 and +0.5
    var f32 = new Float32Array(1);
    var i32 = new Int32Array(f32.buffer);

    function round(x) { return Math.round(x); }
    function roundf(x) { return Math.round(Math.fround(x)); }

    // Warm up
    round(2.5);
    round(3.5);
    roundf(2.5);
    roundf(3.5);

    f32[0] = 0.5;
    i32[0] += 1;
    print('0.5+e =', f32[0]);

    var x = f32[0];
    assertEq(round(x), 1);
    assertEq(roundf(x), 1);

    f32[0] = 0.5;
    i32[0] -= 1;
    print('0.5-e =', f32[0]);

    var x = f32[0];
    assertEq(round(x), 0);
    assertEq(roundf(x), 0);

    f32[0] = -0.5;
    i32[0] += 1;
    print('-0.5-e =', f32[0]);

    var x = f32[0];
    assertEq(round(x), -1);
    assertEq(roundf(x), -1);

    f32[0] = -0.5;
    i32[0] -= 1;
    print('-0.5+e =', f32[0]);

    var x = f32[0];
    assertEq(round(x), -0);
    assertEq(roundf(x), -0);
})();
