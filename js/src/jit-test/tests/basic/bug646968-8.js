// |jit-test| debug

var x = 5;
let (x = eval("x++")) {
    assertEq(evalInFrame(0, "x"), 5);
}
assertEq(x, 6);
