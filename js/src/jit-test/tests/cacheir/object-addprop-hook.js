function test() {
    var sym = Symbol();
    for (var i = 0; i < 100; i++) {
        var obj = newObjectWithAddPropertyHook();
        assertEq(obj._propertiesAdded, 0);
        obj.x = 1;
        obj.y = 2;
        obj.z = 3;
        obj[sym] = 4;
        obj[0] = 1;
        obj[1234567] = 1;
        assertEq(obj._propertiesAdded, 6);
        assertEq(obj.x, 1);
        assertEq(obj[sym], 4);
        assertEq(obj[0], 1);
    }
}
test();
