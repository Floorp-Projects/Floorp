const Cu = Components.utils;
function run_test() {
  var sb = Cu.Sandbox('http://www.example.com');
  var o = new sb.Object();
  o.__proto__ = null;
  do_check_eq(Object.getPrototypeOf(o), null);
}
