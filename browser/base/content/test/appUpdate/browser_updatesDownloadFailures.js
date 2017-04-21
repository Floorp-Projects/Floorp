add_task(function* testDownloadFailures() {
  const maxBackgroundErrors = 5;
  SpecialPowers.pushPrefEnv({set: [
    [PREF_APP_UPDATE_BACKGROUNDMAXERRORS, maxBackgroundErrors],
    [PREF_APP_UPDATE_DOWNLOADPROMPTMAXATTEMPTS, 2]
  ]});
  let updateParams = "badURL=1";

  yield runUpdateTest(updateParams, 1, [
    {
      // if we fail maxBackgroundErrors download attempts, then we want to
      // first show the user an update available prompt.
      notificationId: "update-available",
      button: "button"
    },
    {
      notificationId: "update-available",
      button: "button"
    },
    {
      notificationId: "update-manual",
      button: "button",
      *cleanup() {
        yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
        is(gBrowser.selectedBrowser.currentURI.spec,
           URL_MANUAL_UPDATE, "Landed on manual update page.");
        gBrowser.removeTab(gBrowser.selectedTab);
        gMenuButtonUpdateBadge.reset();
      }
    },
  ]);
});
