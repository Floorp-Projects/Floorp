const Cu = Components.utils;
function run_test() {
  var sb = new Cu.Sandbox('http://www.example.com');
  sb.obj = {};
  sb.arr = [];
  sb.fun = function() {};
  Assert.ok(sb.eval('Object.getPrototypeOf(obj) == null'));
  Assert.ok(sb.eval('Object.getPrototypeOf(arr) == null'));
  Assert.ok(sb.eval('Object.getPrototypeOf(fun) == null'));
}
