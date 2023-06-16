/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Disable `xpc::IsInAutomation()` so that missing variables don't throw
// errors.
Services.prefs.setBoolPref(
  "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer",
  false
);

/**
 * The following test demonstrates crashing behavior.
 */
add_task(function test_missing_variables() {
  const l10nReg = new L10nRegistry();

  const fs = [
    { path: "/localization/en-US/browser/test.ftl", source: "welcome-message = Welcome { $user }\n" }
  ]
  const locales = ["en-US"];
  const source = L10nFileSource.createMock("test", "app", locales, "/localization/{locale}", fs);
  l10nReg.registerSources([source]);
  const l10n = new Localization(["/browser/test.ftl"], true, l10nReg, locales);

  {
    const [message] = l10n.formatValuesSync([{ id: "welcome-message", args: { user: "Greg" } }]);
    equal(message, "Welcome Greg");
  }

  {
    // This will crash in debug builds.
    const [message] = l10n.formatValuesSync([{ id: "welcome-message" }]);
    equal(message, "Welcome {$user}");
  }
});
