
var x = [1,2.5,"three",true,false,null,[1,2,3],{a:0,b:1}];
assertEq(String(x), "1,2.5,three,true,false,,1,2,3,[object Object]");
assertEq(x.length, 8);
assertEq(x[7].a, 0);
assertEq(x[7].b, 1);

var y = {a:0,a:1,a:2};
assertEq(y.a,2);

var z = {a:0,b:1,__proto__:{c:2,d:3}};
assertEq(z.a,0);
assertEq(z.b,1);
assertEq(z.c,2);
assertEq(z.d,3);

function foo() {
  var q = eval("[1,2,3]");
  var r = eval("[1,2,3]");
  assertEq(q === r, false);
}
foo();

var q = {0x4fffffff: 0, 0x7fffffff: 1, 0xffffffff: 2};
assertEq(q[1342177279], 0);
assertEq(q[2147483647], 1);
assertEq(q[4294967295], 2);

try {
  [1,2,3,{a:0,b:1}].foo.bar;
} catch (e) { assertEq(e.message, "[1, 2, 3, {a:0, b:1}].foo is undefined"); }
