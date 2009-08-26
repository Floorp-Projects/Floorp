function testObjectVsPrototype() {
    function D() {}
    var b = D.prototype = {x: 1};
    var d = new D;
    var arr = [b, b, b, d];
    for (var i = 0; i < 4; i++)
        arr[i].x = i;

    d.y = 12;
    assertEq(d.x, 3);
}
testObjectVsPrototype();
