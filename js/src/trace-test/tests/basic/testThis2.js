var cx = evalcx("");
evalcx("function f() {var x; for (var i=0;i<9;i++) x=this; return x;}", cx);
var f = cx.f;
assertEq(f(), cx);
