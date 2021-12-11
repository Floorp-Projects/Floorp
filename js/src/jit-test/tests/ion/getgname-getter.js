var accesses = 100000;

for (var i = 0; i < accesses; i++)
    assertEq(timesAccessed, i+1);

gc();

assertEq(timesAccessed, accesses + 1);
