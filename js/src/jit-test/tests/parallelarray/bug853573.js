if (getBuildConfiguration().parallelJS) {
  var p = Proxy.create({
    has : function(id) {}
  });
  Object.prototype.__proto__ = p;
    var pa0 = new ParallelArray(range(0, 256));
    var pa1 = new ParallelArray(256, function (x) {
      return pa0.map(function(y) {});
    });
}

function range(n, m) {
  var result = [];
  for (var i = n; i < m; i++)
    result.push(i);
  return result;
}
