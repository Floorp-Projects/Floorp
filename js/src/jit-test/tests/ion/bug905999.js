// |jit-test| --ion-eager
function reportCompare (expected) {
  typeof expected;
}
var expect = 'No Crash';
var array = new Array(10);
for (var i = 0; i != array.length; ++i) {
  gc();
}
var expect = array.length;
reportCompare(expect);
