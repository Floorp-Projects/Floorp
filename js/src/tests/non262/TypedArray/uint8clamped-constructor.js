for (var v of [-300, 255.6, 300, 3.5, -3.9]) {
    var a = new Uint8ClampedArray([v]);
    var b = new Uint8ClampedArray(1);
    b[0] = v;

    assertEq(a[0], b[0]);
}

reportCompare(true, true);
