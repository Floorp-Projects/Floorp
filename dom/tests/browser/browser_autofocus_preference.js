add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [["browser.autofocus", false]]});

  const url = "data:text/html,<!DOCTYPE html><html><body><input autofocus><button autofocus></button><textarea autofocus></textarea><select autofocus></select></body></html>";

  let loadedPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.selectedBrowser.loadURI(url);
  await loadedPromise;

  await new Promise(resolve => executeSoon(resolve));

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    is(content.document.activeElement, content.document.body, "body focused");
  });
});

