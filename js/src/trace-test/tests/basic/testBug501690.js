function testBug501690() {
    // Property cache assertion when 3 objects along a prototype chain have the same shape.
    function B(){}
    B.prototype = {x: 123};

    function D(){}
    D.prototype = new B;
    D.prototype.x = 1;    // [1] shapeOf(B.prototype) == shapeOf(D.prototype)

    arr = [new D, new D, new D, D.prototype];  // [2] all the same shape
    for (var i = 0; i < 4; i++)
        assertEq(arr[i].x, 1);  // same kshape [2], same vshape [1]
}
testBug501690();
