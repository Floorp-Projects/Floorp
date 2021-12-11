var re = /.*star.*/i;
var str = "The Shawshank Redemption (1994)";
for (var k = 0; k < 100; k++)
  assertEq(false, re.test(str));
