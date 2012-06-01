// |jit-test| allow-oom

gcparam("maxBytes", gcparam("gcBytes") + 1024);
test();
function test() {
  test();
  eval('');
}
