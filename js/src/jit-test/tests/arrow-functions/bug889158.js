var f = x => arguments.callee;

for (var i=0; i<5; i++)
    assertEq(f(), f);
