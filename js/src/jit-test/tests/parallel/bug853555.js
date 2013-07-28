function test() {
  Object.prototype[0] = /a/;
  function getterFunction(v) { return "getter"; }
  Object.defineProperty(Array.prototype, 1, { get: getterFunction });
  gczeal(4);
  var p = new ParallelArray([1,2,3,4,5]);
  p.scatter([0,1,0,3,01], 9, function (a,b) { return a+b; });
}

if (getBuildConfiguration().parallelJS)
  test();

