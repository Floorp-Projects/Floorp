const Cu = Components.utils;
function run_test() {
  var sb = Cu.Sandbox('http://www.example.com');
  var f = Cu.evalInSandbox('var f = function() {}; f;', sb);
  do_check_eq(f.name, "");
}
