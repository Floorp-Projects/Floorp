var a = [0,1,2,3,4,5,6,7,8,9,10];
for (var i = 0; i < 10; i++)
    delete a[9 - i];
assertEq(a.length, 11);
for (i = 0; i < 10; i++)
    assertEq(a.hasOwnProperty(i), false);

