add_task(function* testBasicPrompt() {
  SpecialPowers.pushPrefEnv({set: [[PREF_APP_UPDATE_STAGING_ENABLED, true]]});
  let updateParams = "showPrompt=1&promptWaitTime=0";
  gUseTestUpdater = true;

  // Open a new window to make sure that it doesn't get in the way
  // of the notification management.
  let extraWindow = yield BrowserTestUtils.openNewBrowserWindow();

  yield runUpdateTest(updateParams, 1, [
    {
      notificationId: "update-available",
      button: "button",
      beforeClick() {
        checkWhatsNewLink("update-available-whats-new");
      }
    },
    {
      notificationId: "update-restart",
      button: "secondarybutton",
      *cleanup() {
        PanelUI.removeNotification(/.*/);
      }
    },
  ]);

  yield BrowserTestUtils.closeWindow(extraWindow);
});
