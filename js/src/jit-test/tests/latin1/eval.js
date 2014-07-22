function f(s) {
    var x = 3, y = 5;
    var z = eval(s);
    assertEq(z, 8);
}
var s = "x + y";
f(s); // Latin1
f(s);
f("x + y;/*\u1200*/"); // TwoByte
f("x + y;/*\u1200*/");
