function test() {
    var arr = new Int8Array(400);
    var idx = toLatin1("384");

    arr[idx] = 9;
    assertEq(arr[idx], 9);
    arr[idx] = 10;
    assertEq(arr[384], 10);

    idx = toLatin1("512");
    assertEq(arr[idx], undefined);
    assertEq(arr[toLatin1("byteLength")], 400);

    var o = {};
    Object.defineProperty(o, idx, {value: 123});
    assertEq(o[512], 123);

    var propLatin1 = toLatin1("foobar");
    o[propLatin1] = 3;
    assertEq(o.foobar, 3);

    var propTwoByte = "foobar\u1200";
    o[propTwoByte] = 4;
    assertEq(o.foobar\u1200, 4);
}
test();
