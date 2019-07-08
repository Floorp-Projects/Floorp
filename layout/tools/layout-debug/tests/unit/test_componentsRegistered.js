function run_test() {
  Assert.ok("@mozilla.org/layout-debug/layout-debuggingtools;1" in Cc);
  Assert.ok(
    "@mozilla.org/commandlinehandler/general-startup;1?type=layoutdebug" in Cc
  );
}
