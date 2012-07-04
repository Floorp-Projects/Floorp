// for-of works on CData objects of array type, but throws for other CData objects.

if (this.ctypes) {
    load(libdir + "asserts.js");
    var v = ctypes.int32_t(17);
    assertThrowsInstanceOf(function () { for (var x of v) ; }, TypeError);

    var intarray_t = ctypes.int32_t.array();
    var a = new intarray_t([0, 1, 1, 2, 3]);
    var b = [x for (x of a)];
    assertEq(b.join(), '0,1,1,2,3');

    var it = a.iterator();
    assertEq(it.next(), 0);
    a[1] = -100;
    assertEq(it.next(), -100);
}
