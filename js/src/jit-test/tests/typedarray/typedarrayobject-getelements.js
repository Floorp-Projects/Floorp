function test() {
    var view = new Uint8Array([72, 101, 108, 108, 111]);
    let s = String.fromCharCode.apply(null, view);
    assertEq("Hello", s);
}
test();
