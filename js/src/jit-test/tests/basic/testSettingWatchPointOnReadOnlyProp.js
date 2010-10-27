for (var i = 0; i < 5; ++i) {
    var o = {}
    Object.defineProperty(o, 'x', { value:"cow", writable:false });
    var r = o.watch('x', function() {});
    assertEq(r, undefined);
    o.x = 4;
}
