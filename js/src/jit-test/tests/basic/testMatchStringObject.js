function testMatchStringObject() {
    var a = new String("foo");
    var b;
    for (i = 0; i < 300; i++)
        b = a.match(/bar/);
    return b;
}
assertEq(testMatchStringObject(), null);
