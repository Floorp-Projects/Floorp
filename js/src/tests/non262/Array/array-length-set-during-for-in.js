var a = [0, 1];
var iterations = 0;
for (var k in a) {
  iterations++;
  a.length = 1;
}
assertEq(iterations, 1);

if (typeof reportCompare === "function")
  reportCompare(true, true);
