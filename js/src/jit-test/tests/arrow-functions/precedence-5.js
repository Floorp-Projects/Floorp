// map(x => x, 32) is two arguments, not one

assertEq("" + [1, 2, 3, 4].map(x => x, 32), "1,2,3,4");
