const Cu = Components.utils;

function testStrict(sb) {
  "use strict";
  Assert.equal(sb.eval("typeof wrappedCtor()"), "string");
  Assert.equal(sb.eval("typeof new wrappedCtor()"), "object");
}

function run_test() {
  var sb = new Cu.Sandbox(null);
  var dateCtor = sb.Date;
  sb.wrappedCtor = Cu.exportFunction(function wrapper(val) {
    "use strict";
    var constructing = this.constructor == wrapper;
    return constructing ? new dateCtor(val) : dateCtor(val);
  }, sb);
  Assert.equal(typeof Date(), "string");
  Assert.equal(typeof new Date(), "object");
  Assert.equal(sb.eval("typeof wrappedCtor()"), "string");
  Assert.equal(sb.eval("typeof new wrappedCtor()"), "object");
  testStrict(sb);
}
