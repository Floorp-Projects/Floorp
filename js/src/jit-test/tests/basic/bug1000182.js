// |jit-test| allow-unhandlable-oom;
function range(n, m) {
  var result = [];
    result.push(i);
  return result;
    for (var i = 0, l = a.length; i < l; i++) {}
}
function b(opFunction, cmpFunction) {
    var result = opFunction({mode:"compile"});
      var result = opFunction({mode:"par"});
}
function a(arr, op, func, cmpFunc) {
  b(function (m) { return arr[op + "Par"].apply(arr, [func, m]); }, function(m) {});
}
if ("mapPar" in [])
    a(range(0, 1024), "map", function (i) {});
if (typeof oomAfterAllocations === "function")
    oomAfterAllocations(4);
