add_task(function* testBasicPrompt() {
  SpecialPowers.pushPrefEnv({set: [
    [PREF_APP_UPDATE_SERVICE_ENABLED, false],
  ]});
  let updateParams = "promptWaitTime=0";

  let file = getWriteTestFile();
  file.create(file.NORMAL_FILE_TYPE, 0o444);
  file.fileAttributesWin |= file.WFA_READONLY;
  file.fileAttributesWin &= ~file.WFA_READWRITE;

  yield runUpdateTest(updateParams, 1, [
    {
      notificationId: "update-manual",
      button: "button",
      *cleanup() {
        yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
        is(gBrowser.selectedBrowser.currentURI.spec,
           URL_MANUAL_UPDATE, "Landed on manual update page.")
        gBrowser.removeTab(gBrowser.selectedTab);
        getWriteTestFile();
      }
    },
  ]);
});

function getWriteTestFile() {
  let file = getAppBaseDir();
  file.append(FILE_UPDATE_TEST);
  file.QueryInterface(Ci.nsILocalFileWin);
  if (file.exists()) {
    file.fileAttributesWin |= file.WFA_READWRITE;
    file.fileAttributesWin &= ~file.WFA_READONLY;
    file.remove(true);
  }
  return file;
}
