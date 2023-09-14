add_task(async function () {
  await SpecialPowers.pushPrefEnv({ set: [["browser.autofocus", false]] });

  const url =
    "data:text/html,<!DOCTYPE html><html><body><input autofocus><button autofocus></button><textarea autofocus></textarea><select autofocus></select></body></html>";

  let loadedPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, url);
  await loadedPromise;

  await new Promise(resolve => executeSoon(resolve));

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    is(content.document.activeElement, content.document.body, "body focused");
  });
});
