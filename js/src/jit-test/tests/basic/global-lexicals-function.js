class Foo {};
let x = 1;
const y = 2;
var z = 3;

var obj = globalLexicals();
assertEq(obj.Foo, Foo);
assertEq(obj.x, 1);
assertEq(obj.y, 2);
assertEq("z" in obj, false);

// It's just a copy.
obj.x = 2;
assertEq(x, 1);
