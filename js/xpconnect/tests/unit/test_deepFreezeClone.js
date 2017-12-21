const Cu = Components.utils;

function checkThrows(f, rgxp) { try { f(); do_check_false(); } catch (e) { Assert.ok(rgxp.test(e)); } }

var o = { foo: 42, bar : { tick: 'tock' } };
function checkClone(clone, frozen) {
  var waived = Cu.waiveXrays(clone);
  function touchFoo() { "use strict"; waived.foo = 12; Assert.equal(waived.foo, 12); }
  function touchBar() { "use strict"; waived.bar.tick = 'tack'; Assert.equal(waived.bar.tick, 'tack'); }
  function addProp() { "use strict"; waived.newProp = 100; Assert.equal(waived.newProp, 100); }
  if (!frozen) {
    touchFoo();
    touchBar();
    addProp();
  } else {
    checkThrows(touchFoo, /read-only/);
    checkThrows(touchBar, /read-only/);
    checkThrows(addProp, /extensible/);
  }

  var desc = Object.getOwnPropertyDescriptor(waived, 'foo');
  Assert.equal(desc.writable, !frozen);
  Assert.equal(desc.configurable, !frozen);
  desc = Object.getOwnPropertyDescriptor(waived.bar, 'tick');
  Assert.equal(desc.writable, !frozen);
  Assert.equal(desc.configurable, !frozen);
}

function run_test() {
  var sb = new Cu.Sandbox(null);
  checkClone(Cu.waiveXrays(Cu.cloneInto(o, sb)), false);
  checkClone(Cu.cloneInto(o, sb, { deepFreeze: true }), true);
}
