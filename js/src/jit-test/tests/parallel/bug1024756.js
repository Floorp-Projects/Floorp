// Failure to track frame size properly in code generation for NewDenseArray and rest arguments.

if (!getBuildConfiguration().parallelJS)
  quit(0);

var x =
(function() {
    return Array.buildPar(15891, function() {
        return [].map(function() {})
    })
})();
assertEq(x.length, 15891);
