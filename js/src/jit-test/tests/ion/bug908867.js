if (typeof ParallelArray === "undefined")
  quit();

x = ParallelArray()
y = x.shape
Object.defineProperty(this, "z", {
    get: function() {
        return x.flatten()
    }
})
Object.defineProperty(y, 5, {
    value: this
});
y[8] = z
valueOf = (function() {
    function f() {
        (.9 % 1) > f
    }
    return f
})(this, {})
x.shape.join()


assertArraySeqParResultsEq(range(0, 1024), "filter", function(e, i) { return (i % (1.1)) != 0; });
function range(n, m) {
  var result = [];
  for (var i = n; i < m; i++)
    result.push(i);
  return result;
}
function assertArraySeqParResultsEq(arr, op, func) {
  arr[op].apply(arr, [func]);
}


function foo(v) {
  if (v < -200) return 0;
  if (v > 200) return 0;
  return v % 1;
}
assertEq(foo(0.9), 0.9);
assertEq(foo(0.9), 0.9);
