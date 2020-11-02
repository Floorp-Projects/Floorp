/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// Checks that the timing for formatting/unformatting the URL.
add_task(async function() {
  const EXPECTED =
    "<https://>example.com</browser/browser/components/urlbar/tests/browser/slow-page.sjs>";
  const URL = EXPECTED.replace(/[<>]/g, "");

  info("Type url in the urlbar");
  gURLBar.focus();
  gURLBar.select();
  EventUtils.sendString(URL);

  Assert.equal(isHighlighted(), false, "Url is not highlighted");
  Assert.equal(
    gURLBar.getAttribute("pageproxystate"),
    "invalid",
    "pageproxystate is invalid when typing url"
  );

  info("Move the focus from the urlbar");
  gBrowser.selectedBrowser.focus();

  Assert.equal(
    isHighlighted(),
    false,
    "Url is not highlighted yet even if the focus is moved"
  );
  Assert.equal(
    gURLBar.getAttribute("pageproxystate"),
    "invalid",
    "pageproxystate is still invalid even if the focus is moved"
  );

  info("Type enter key on the urlbar to load the url");
  const onLoad = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(
    () => "load"
  );
  const onFormat = TestUtils.waitForCondition(() => isHighlighted()).then(
    () => "format"
  );

  gURLBar.focus();
  EventUtils.synthesizeKey("KEY_Enter");

  const raceResult = await Promise.race([onLoad, onFormat]);
  Assert.equal(
    raceResult,
    "format",
    "The url is formatted before finishing loading"
  );

  const selectionController = gURLBar.editor.selectionController;
  const selection = selectionController.getSelection(
    selectionController.SELECTION_URLSECONDARY
  );
  let value = gURLBar.editor.rootElement.textContent;
  let result = "";
  for (let i = 0; i < selection.rangeCount; i++) {
    const range = selection.getRangeAt(i).toString();
    const pos = value.indexOf(range);
    result += value.substring(0, pos) + "<" + range + ">";
    value = value.substring(pos + range.length);
  }
  result += value;

  Assert.equal(result, EXPECTED, "Correct part of the url is de-emphasized");

  await onLoad;
  Assert.equal(
    gURLBar.getAttribute("pageproxystate"),
    "valid",
    "pageproxystate is valid"
  );
  Assert.equal(
    isHighlighted(),
    true,
    "Url is still formatted even after loading"
  );

  info("Move the focus to the urlbar again");
  gURLBar.focus();
  await TestUtils.waitForCondition(
    () => !isHighlighted(),
    "Wait for the urlbar to not be formatted"
  );
  info("Url is unformatted");
});

function isHighlighted() {
  const selectionController = gURLBar.editor.selectionController;
  const selection = selectionController.getSelection(
    selectionController.SELECTION_URLSECONDARY
  );
  return (
    selection.rangeCount === 2 &&
    selection.anchorOffset !== selection.focusOffset
  );
}
