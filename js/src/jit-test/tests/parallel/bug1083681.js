if (!getBuildConfiguration().parallelJS)
  quit(0);

function f() {
    Function() * (function() {})()
}
Array.buildPar(1, f)

