var x = 5;
let (x = x)
    assertEq(x, 5);
let (x = eval("x"))
    assertEq(x, 5);
let (x = function () { with ({}) return x; })
    assertEq(x(), 5);
