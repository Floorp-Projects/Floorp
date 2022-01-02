class Foo {};
let x = 1;
const y = 2;
var z = 3;

var obj = globalLexicals();
assertEq(Object.keys(obj).length >= 3, true);
assertEq(obj.Foo, Foo);
assertEq(obj.x, 1);
assertEq(obj.y, 2);
assertEq("z" in obj, false);

assertEq("uninit" in obj, false);
let uninit;

// It's just a copy.
obj.x = 2;
assertEq(x, 1);
