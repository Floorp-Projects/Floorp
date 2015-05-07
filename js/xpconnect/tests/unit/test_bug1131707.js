const Cu = Components.utils;

function testStrict(sb) {
  "use strict";
  do_check_eq(sb.eval("typeof wrappedCtor()"), "string");
  do_check_eq(sb.eval("typeof new wrappedCtor()"), "object");
}

function run_test() {
  var sb = new Cu.Sandbox(null);
  var dateCtor = sb.Date;
  sb.wrappedCtor = Cu.exportFunction(function wrapper(val) {
    "use strict";
    var constructing = this.constructor == wrapper;
    return constructing ? new dateCtor(val) : dateCtor(val);
  }, sb);
  do_check_eq(typeof Date(), "string");
  do_check_eq(typeof new Date(), "object");
  do_check_eq(sb.eval("typeof wrappedCtor()"), "string");
  do_check_eq(sb.eval("typeof new wrappedCtor()"), "object");
  testStrict(sb);
}
