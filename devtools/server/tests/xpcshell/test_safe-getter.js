/* eslint-disable strict */
function run_test() {
  Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
  });
  const { addDebuggerToGlobal } = ChromeUtils.import(
    "resource://gre/modules/jsdebugger.jsm"
  );
  addDebuggerToGlobal(this);
  const g = createTestGlobal("test", {
    wantGlobalProperties: ["ChromeUtils"],
  });
  const dbg = new Debugger();
  const gw = dbg.addDebuggee(g);

  g.eval(`
    // This is not a CCW.
    Object.defineProperty(this, "bar", {
      get: function() { return "bar"; },
      configurable: true,
      enumerable: true
    });

    const { XPCOMUtils } = ChromeUtils.importESModule(
      "resource://gre/modules/XPCOMUtils.sys.mjs"
    );

    // This is a CCW.
    XPCOMUtils.defineLazyGetter(this, "foo", function() { return "foo"; });
  `);

  // Neither scripted getter should be considered safe.
  assert(!DevToolsUtils.hasSafeGetter(gw.getOwnPropertyDescriptor("bar")));
  assert(!DevToolsUtils.hasSafeGetter(gw.getOwnPropertyDescriptor("foo")));

  // Create an object in a less privileged sandbox.
  const obj = gw.makeDebuggeeValue(
    Cu.waiveXrays(
      Cu.Sandbox(null).eval(`
    Object.defineProperty({}, "bar", {
      get: function() { return "bar"; },
      configurable: true,
      enumerable: true
    });
  `)
    )
  );

  // After waiving Xrays, the object has 2 wrappers. Both must be removed
  // in order to detect that the getter is not safe.
  assert(!DevToolsUtils.hasSafeGetter(obj.getOwnPropertyDescriptor("bar")));
}
