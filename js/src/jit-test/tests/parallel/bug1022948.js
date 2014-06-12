if (!getBuildConfiguration().parallelJS)
  quit(0);

var a = Array.buildPar(999, function() {
    return (Math.asin(Math.tanh(1)))
});
var count = 0;
for (let e of a) {
  count++;
  print(count);
  assertEq(e, a[0])
}
