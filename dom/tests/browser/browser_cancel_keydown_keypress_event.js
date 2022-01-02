const URL =
  "https://example.com/browser/dom/tests/browser/prevent_return_key.html";

const { PromptTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromptTestUtils.jsm"
);

// Wait for alert dialog and dismiss it immediately.
function awaitAndCloseAlertDialog(browser) {
  return PromptTestUtils.handleNextPrompt(
    browser,
    { modalType: Services.prompt.MODAL_TYPE_CONTENT, promptType: "alert" },
    { buttonNumClick: 0 }
  );
}

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  let browser = tab.linkedBrowser;

  // Focus and enter random text on input.
  await SpecialPowers.spawn(browser, [], async function() {
    let input = content.document.getElementById("input");
    input.focus();
    input.value = "abcd";
  });

  // Send return key (cross process) to submit the form implicitly.
  let dialogShown = awaitAndCloseAlertDialog(browser);
  EventUtils.synthesizeKey("KEY_Enter");
  await dialogShown;

  // Check that the form should not have been submitted.
  await SpecialPowers.spawn(browser, [], async function() {
    let result = content.document.getElementById("result").innerHTML;
    info("submit result: " + result);
    is(result, "not submitted", "form should not have submitted");
  });

  gBrowser.removeCurrentTab();
});
