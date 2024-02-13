/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_shouldLog_maxLogLevel() {
  let ci = console.createInstance({ maxLogLevel: "Warn" });

  Assert.ok(
    ci.shouldLog("Error"),
    "Should return true for logging a higher level"
  );
  Assert.ok(
    ci.shouldLog("Warn"),
    "Should return true for logging the same level"
  );
  Assert.ok(
    !ci.shouldLog("Debug"),
    "Should return false for logging a lower level;"
  );
});

add_task(async function test_shouldLog_maxLogLevelPref() {
  Services.prefs.setStringPref("test.log", "Warn");
  let ci = console.createInstance({ maxLogLevelPref: "test.log" });

  Assert.ok(
    !ci.shouldLog("Debug"),
    "Should return false for logging a lower level;"
  );

  Services.prefs.setStringPref("test.log", "Debug");
  Assert.ok(
    ci.shouldLog("Debug"),
    "Should return true for logging a lower level after pref update"
  );
});
