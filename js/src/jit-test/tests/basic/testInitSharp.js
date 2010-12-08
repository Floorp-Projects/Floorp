
function test1() {
  return String(#1=[1,2,#1#.length,3,4,delete #1#[0]]);
}
assertEq(test1(), ",2,2,3,4,true");

function test2() {
  var x = #1={a:0,b:1,c:delete #1#.a};
  var y = "";
  for (var z in x) { y += z + ":" + x[z] + ","; }
  return y;
}
assertEq(test2(), "b:1,c:true,");

function test3() {
  return String(#1=[1,2,#1#.foo = 3,4,5,6]);
}
assertEq(test3(), "1,2,3,4,5,6");
