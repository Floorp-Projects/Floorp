
compare = (function() {
  function inner() { return inner.caller; };
  globalInner = inner;
  globalClosure = inner();
  return function(f) { return f === inner; }
})();

assertEq(compare(globalInner), true);
globalClosure();
assertEq(compare(globalInner), false);
