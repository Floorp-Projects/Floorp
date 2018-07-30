let lfPreamble = `
assertEq = function(a,b) {
  try { print(a); print(b); } catch(exc) {}
}
`;
evaluate(lfPreamble, {});
var a = [1, 2, 3];
var b = [4, 5, 6];
function testFold() {
    for (var i = 0; i < 10; i++) {
        var x = a[i];
        var z = b[i];
        if (((x / x) | 0) < 3) assertEq(x !== z, true);
    }
}
for (var i = 0; i < 10; i++) testFold();
