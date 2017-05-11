add_task(function* testUpdatesBackgroundWindow() {
  SpecialPowers.pushPrefEnv({set: [[PREF_APP_UPDATE_STAGING_ENABLED, false]]});

  let updateParams = "showPrompt=1&promptWaitTime=0";
  let extraWindow = yield BrowserTestUtils.openNewBrowserWindow();
  yield SimpleTest.promiseFocus(extraWindow);

  yield runUpdateTest(updateParams, 1, [
    function*() {
      yield BrowserTestUtils.waitForCondition(() => PanelUI.menuButton.hasAttribute("badge-status"),
                                              "Background window has a badge.");
      is(PanelUI.notificationPanel.state, "closed",
         "The doorhanger is not showing for the background window");
      is(PanelUI.menuButton.getAttribute("badge-status"), "update-available",
         "The badge is showing for the background window");
      let popupShownPromise = BrowserTestUtils.waitForEvent(PanelUI.notificationPanel, "popupshown");
      yield BrowserTestUtils.closeWindow(extraWindow);
      yield SimpleTest.promiseFocus(window);
      yield popupShownPromise;

      checkWhatsNewLink("update-available-whats-new");
      let buttonEl = getNotificationButton(window, "update-available", "button");
      buttonEl.click();
    },
    {
      notificationId: "update-restart",
      button: "secondarybutton",
      cleanup() {
        PanelUI.removeNotification(/.*/);
      }
    },
  ]);
});

