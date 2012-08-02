// |jit-test| error: ReferenceError
gczeal(2,1);
(function () {
  var m = {}
  return { stringify: stringify };
})();
