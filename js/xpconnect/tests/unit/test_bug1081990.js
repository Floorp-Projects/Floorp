const Cu = Components.utils;
function run_test() {
  var sb = new Cu.Sandbox('http://www.example.com');
  sb.obj = {};
  sb.arr = [];
  sb.fun = function() {};
  do_check_true(sb.eval('Object.getPrototypeOf(obj) == Object.prototype'));
  do_check_true(sb.eval('Object.getPrototypeOf(arr) == null'));
  do_check_true(sb.eval('Object.getPrototypeOf(fun) == null'));
}
