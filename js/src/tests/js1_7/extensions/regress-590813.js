
var actual = '';
var expected = 'A0B1B2C0C1C2';

var x = Iterator([1,2,3], true);

for (var a in x) {
  actual += 'A' + a;
  for (var b in x) {
    actual += 'B' + b;
  }
}

var y = Iterator([1,2,3], true);

for (var c in y) {
  actual += 'C' + c;
}
for (var d in y) {
  actual += 'D' + d;
}

reportCompare(expected, actual, "Handle nested Iterator iteration right");
