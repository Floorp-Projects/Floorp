var x = "one";
var y = "two";
const a = x + y;
var hit = false;
eval('switch("onetwo") { case a: hit = true; };');
assertEq(hit, true);
