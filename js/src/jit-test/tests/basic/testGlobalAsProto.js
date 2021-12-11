var b = Object.create(this);
for (var i = 0; i < 9; i++)
    assertEq(b.i, i);
