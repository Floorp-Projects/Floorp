var x = eval("gc(); 30");
var y = x.toString();
isNaN(x);
assertEq(y, "30");
