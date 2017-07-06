// |jit-test| allow-oom; allow-unhandlable-oom; allow-overrecursed

gcparam("maxBytes", gcparam("gcBytes") + 1024);
test();
function test() {
  var upvar = "";
  function f() { upvar += ""; }
  test();
  eval('');
}
