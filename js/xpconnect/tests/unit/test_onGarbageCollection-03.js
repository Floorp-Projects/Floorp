// Test that the onGarbageCollection hook is not reentrant.

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

function run_test() {
  do_test_pending();

  const root = newGlobal();
  const dbg = new Debugger();
  const wrappedRoot = dbg.addDebuggee(root)

  let fired = true;
  let depth = 0;

  dbg.memory.onGarbageCollection = _ => {
    fired = true;

    equal(depth, 0);
    depth++;
    try {
      root.eval(`gc()`);
    } finally {
      equal(depth, 1);
      depth--;
    }
  }

  root.eval(`gc()`);

  executeSoon(() => {
    ok(fired);
    equal(depth, 0);
    dbg.memory.onGarbageCollection = undefined;
    do_test_finished();
  });
}
