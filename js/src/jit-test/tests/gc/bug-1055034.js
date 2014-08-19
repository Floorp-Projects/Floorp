function range(n, m) {
  var result = [];
  for (var i = n; i < m; i++)
    result.push(i);
  return result;
}
function run(arr, func) {
  var expected = arr["map"].apply(arr, [func]);
  function f(m) { return arr["mapPar"].apply(arr, [func, m]); }
    f({mode:"compile"});
    f({mode:"seq"});
 }
run(range(0, 1024), function (i) { var a = []; a.length = i; });
