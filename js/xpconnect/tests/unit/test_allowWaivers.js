const Cu = Components.utils;
function checkWaivers(from, allowed) {
  var sb = new Cu.Sandbox('http://example.com');
  from.test = sb.eval('var o = {prop: 2, f: function() {return 42;}}; o');

  // Make sure that |from| has Xrays to sb.
  Assert.equal(from.eval('test.prop'), 2);
  Assert.equal(from.eval('test.f'), undefined);

  // Make sure that waivability works as expected.
  Assert.equal(from.eval('!!test.wrappedJSObject'), allowed);
  Assert.equal(from.eval('XPCNativeWrapper.unwrap(test) !== test'), allowed);

  // Make a sandbox with the same principal as |from|, but without any waiver
  // restrictions, and make sure that the waiver does not transfer.
  var friend = new Cu.Sandbox(Cu.getObjectPrincipal(from));
  friend.test = from.test;
  friend.eval('var waived = test.wrappedJSObject;');
  Assert.ok(friend.eval('waived.f()'), 42);
  friend.from = from;
  friend.eval('from.waived = waived');
  Assert.equal(from.eval('!!waived.f'), allowed);
}

function run_test() {
  checkWaivers(new Cu.Sandbox('http://example.com'), true);
  checkWaivers(new Cu.Sandbox('http://example.com', {allowWaivers: false}), false);
  checkWaivers(new Cu.Sandbox(['http://example.com']), true);
  checkWaivers(new Cu.Sandbox(['http://example.com'], {allowWaivers: false}), false);
}
