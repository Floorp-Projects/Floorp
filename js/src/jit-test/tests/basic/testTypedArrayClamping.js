function f() {
    var input = [];
    var expected = [];

    var add = function(inp, exp) {
        input.push(inp);
        expected.push(exp);
    };

    add(-0, 0);
    add(-0.1, 0);
    add(-0.7, 0);
    add(0.1, 0);
    add(NaN, 0);
    add(-Infinity, 0);
    add(Infinity, 255);
    add(0.7, 1);
    add(1.2, 1);
    add(3.5, 4);
    add(4.5, 4);
    add(254.4, 254);
    add(254.5, 254);
    add(254.6, 255);
    add(254.9, 255);
    add(255.1, 255);
    add(255.4, 255);
    add(255.5, 255);
    add(255.9, 255);

    var len = input.length;
    var a = new Uint8ClampedArray(len);

    for (var i=0; i<len; i++) {
        a[i] = input[i];
    }
    for (var i=0; i<len; i++) {
        assertEq(a[i], expected[i], "Failed: " + input[i]);
    }
}
f();
f();
