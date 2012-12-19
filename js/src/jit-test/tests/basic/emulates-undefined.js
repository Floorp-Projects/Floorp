function test() {
    var values = [undefined, null, Math, objectEmulatingUndefined()];
    var expected = [true, true, false, true];

    for (var i=0; i<100; i++) {
        var idx = i % values.length;
        if (values[idx] == undefined)
            assertEq(expected[idx], true);
        else
            assertEq(expected[idx], false);

        if (null != values[idx])
            assertEq(expected[idx], false);
        else
            assertEq(expected[idx], true);
    }
}
test();
