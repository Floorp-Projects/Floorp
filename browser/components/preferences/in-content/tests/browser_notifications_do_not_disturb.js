/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


registerCleanupFunction(function() {
  while (gBrowser.tabs[1])
    gBrowser.removeTab(gBrowser.tabs[1]);
});

add_task(function*() {
  let prefs = yield openPreferencesViaOpenPreferencesAPI("panePrivacy", {leaveOpen: true});
  is(prefs.selectedPane, "panePrivacy", "Privacy pane was selected");

  let doc = gBrowser.contentDocument;
  let notificationsDoNotDisturbBox = doc.getElementById("notificationsDoNotDisturbBox");
  if (notificationsDoNotDisturbBox.hidden) {
    todo(false, "Do not disturb is not available on this platform");
    return;
  }

  let alertService;
  try {
    alertService = Cc["@mozilla.org/alerts-service;1"]
                     .getService(Ci.nsIAlertsService)
                     .QueryInterface(Ci.nsIAlertsDoNotDisturb);
  } catch (ex) {
    ok(true, "Do not disturb is not available on this platform: " + ex.message);
    return;
  }

  let checkbox = doc.getElementById("notificationsDoNotDisturb");
  ok(!checkbox.checked, "Checkbox should not be checked by default");
  ok(!alertService.manualDoNotDisturb, "Do not disturb should be off by default");

  let checkboxChanged = waitForEvent(checkbox, "command")
  checkbox.click();
  yield checkboxChanged;
  ok(alertService.manualDoNotDisturb, "Do not disturb should be enabled when checked");

  checkboxChanged = waitForEvent(checkbox, "command")
  checkbox.click();
  yield checkboxChanged;
  ok(!alertService.manualDoNotDisturb, "Do not disturb should be disabled when unchecked");
});
