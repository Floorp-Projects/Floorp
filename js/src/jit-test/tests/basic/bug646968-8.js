load(libdir + "evalInFrame.js");

var x = 5;
{
    let x = eval("this.x++");
    assertEq(evalInFrame(0, "x"), 5);
}
assertEq(x, 6);
