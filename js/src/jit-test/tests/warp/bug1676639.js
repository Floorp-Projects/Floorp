function foo() {
  return Math.atanh(true === Math.fround(0) | 0) != true;
}
var results = [];
for (var j = 0; j < 50; j++) {
  results.push(foo(0,0));
}
