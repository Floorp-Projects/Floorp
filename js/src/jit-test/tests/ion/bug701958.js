function f(v)
{
    return v.x + v.x + v.y;
}

assertEq(f({ x: 1, y: 2 }), 4);
assertEq(f({ __proto__: {x: 1, y: 2} }), 4);
assertEq(f({ __proto__: {x: 0, y: 2}, x: 1 }), 4);

// Ensure there is no aliasing.
var global = 1;
assertEq(f(Object.defineProperty({ y : 2 }, "x", { get : function(){ return global++; } })), 5);
