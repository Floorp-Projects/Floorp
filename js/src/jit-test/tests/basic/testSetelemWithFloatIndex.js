var x, a = {};
for (var i = 0; i < 9; i++)
    x = a[-3.5] = "ok";
assertEq(x, "ok");
