add_task(function* testPartialPatchApplyFailure() {
  let patches = getLocalPatchString("partial", null, null, null, null, null,
                                    STATE_PENDING);
  let updates = getLocalUpdateString(patches, null, null, null,
                                     Services.appinfo.version, null,
                                     null, null, null, null, "false");

  yield runUpdateProcessingTest(updates, [
    {
      // if we have only an invalid patch, then something's wrong and we don't
      // have an automatic way to fix it, so show the manual update
      // doorhanger.
      notificationId: "update-manual",
      button: "button",
      beforeClick() {
        checkWhatsNewLink("update-manual-whats-new");
      },
      *cleanup() {
        yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
        is(gBrowser.selectedBrowser.currentURI.spec,
           URL_MANUAL_UPDATE, "Landed on manual update page.")
        gBrowser.removeTab(gBrowser.selectedTab);
      }
    },
  ]);
});
