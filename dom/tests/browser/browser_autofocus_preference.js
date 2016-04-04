add_task(function* () {
  yield new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [["browser.autofocus", false]]}, resolve);
  });

  const url = "data:text/html,<!DOCTYPE html><html><body><input autofocus><button autofocus></button><textarea autofocus></textarea><select autofocus></select></body></html>";

  let loadedPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.selectedBrowser.loadURI(url);
  yield loadedPromise;

  yield new Promise(resolve => executeSoon(resolve));

  yield ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
    is(content.document.activeElement, content.document.body, "body focused");
  });
});

