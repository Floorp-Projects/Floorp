const Cu = Components.utils;

function checkThrows(f, rgxp) { try { f(); do_check_false(); } catch (e) { do_check_true(rgxp.test(e)); } }

var o = { foo: 42, bar : { tick: 'tock' } };
function checkClone(clone, frozen) {
  var waived = Cu.waiveXrays(clone);
  function touchFoo() { "use strict"; waived.foo = 12; do_check_eq(waived.foo, 12); }
  function touchBar() { "use strict"; waived.bar.tick = 'tack'; do_check_eq(waived.bar.tick, 'tack'); }
  function addProp() { "use strict"; waived.newProp = 100; do_check_eq(waived.newProp, 100); }
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
  do_check_eq(desc.writable, !frozen);
  do_check_eq(desc.configurable, !frozen);
  desc = Object.getOwnPropertyDescriptor(waived.bar, 'tick');
  do_check_eq(desc.writable, !frozen);
  do_check_eq(desc.configurable, !frozen);
}

function run_test() {
  var sb = new Cu.Sandbox(null);
  checkClone(Cu.waiveXrays(Cu.cloneInto(o, sb)), false);
  checkClone(Cu.cloneInto(o, sb, { deepFreeze: true }), true);
}
