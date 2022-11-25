/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// After detaching a tab into a new window, the input value in the new window
// should be formatted.

add_task(async function detach() {
  // Sometimes the value isn't formatted on Mac when running in verify chaos
  // mode. The usual, proper front-end code path is hit, and the path that
  // removes formatting is not hit, so it seems like some kind of race in the
  // editor or selection code in Gecko. Since this has only been observed on Mac
  // in chaos mode and doesn't seem to be a problem in urlbar code, skip the
  // test in that case.
  if (AppConstants.platform == "macosx" && Services.env.get("MOZ_CHAOSMODE")) {
    Assert.ok(true, "Skipping test in chaos mode on Mac");
    return;
  }

  UrlbarPrefs.clear("formatting.enabled");
  Assert.ok(
    UrlbarPrefs.get("formatting.enabled"),
    "Formatting is enabled by default"
  );

  info("Waiting for new tab");
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "https://example.com/detach",
  });

  let winPromise = BrowserTestUtils.waitForNewWindow();
  info("Detaching tab");
  let win = gBrowser.replaceTabWithWindow(tab, {});
  info("Waiting for new window");
  await winPromise;

  // Wait an extra tick for good measure since the code itself also waits for
  // `delayedStartupPromise`.
  info("Waiting for delayed startup in new window");
  await win.delayedStartupPromise;
  info("Waiting for tick");
  await TestUtils.waitForTick();

  assertValue("<https://>example.com</detach>", win);
  await BrowserTestUtils.closeWindow(win);
});

/**
 * Asserts formatting in the input is correct.
 *
 * @param {string} expectedValue
 *   The URL to test. The parts the are expected to be de-emphasized should be
 *   wrapped in "<" and ">" chars.
 * @param {window} win
 *   The input in this window will be tested.
 */
function assertValue(expectedValue, win = window) {
  let selectionController = win.gURLBar.editor.selectionController;
  let selection = selectionController.getSelection(
    selectionController.SELECTION_URLSECONDARY
  );
  let value = win.gURLBar.editor.rootElement.textContent;
  let result = "";
  for (let i = 0; i < selection.rangeCount; i++) {
    let range = selection.getRangeAt(i).toString();
    let pos = value.indexOf(range);
    result += value.substring(0, pos) + "<" + range + ">";
    value = value.substring(pos + range.length);
  }
  result += value;
  Assert.equal(
    result,
    expectedValue,
    "Correct part of the url is de-emphasized"
  );
}
