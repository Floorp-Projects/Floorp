Services.prefs.setBoolPref("privacy.resistFingerprinting", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("privacy.resistFingerprinting");
});

add_task(function test_sandbox_rfp() {
  var sb = Cu.Sandbox('http://example.com');
  var date = Cu.evalInSandbox('Date.now()', sb);
  ok(date > 1672553000000, "Date.now() works with resistFingerprinting");
});

add_task(function test_sandbox_system() {
  var sb = Cu.Sandbox(Services.scriptSecurityManager.getSystemPrincipal());
  var date = Cu.evalInSandbox('Date.now()', sb);
  ok(date > 1672553000000, "Date.now() works with systemprincipal");
});
