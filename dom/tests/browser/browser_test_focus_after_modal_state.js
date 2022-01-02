const TEST_URL =
  "https://example.com/browser/dom/tests/browser/focus_after_prompt.html";

const { PromptTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromptTestUtils.jsm"
);

function awaitAndClosePrompt(browser) {
  return PromptTestUtils.handleNextPrompt(
    browser,
    { modalType: Services.prompt.MODAL_TYPE_CONTENT, promptType: "prompt" },
    { buttonNumClick: 0 }
  );
}

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let browser = tab.linkedBrowser;

  // Focus on editable iframe.
  await BrowserTestUtils.synthesizeMouseAtCenter("#edit", {}, browser);
  await SpecialPowers.spawn(browser, [], async function() {
    is(
      content.document.activeElement,
      content.document.getElementById("edit"),
      "Focus should be on iframe element"
    );
  });

  let focusBlurPromise = SpecialPowers.spawn(browser, [], async function() {
    let focusOccurred = false;
    let blurOccurred = false;

    return new Promise(resolve => {
      let doc = content.document.getElementById("edit").contentDocument;
      doc.addEventListener("focus", function(event) {
        focusOccurred = true;
        if (blurOccurred) {
          resolve(true);
        }
      });

      doc.addEventListener("blur", function(event) {
        blurOccurred = true;
        if (focusOccurred) {
          resolve(false);
        }
      });
    });
  });

  // Click on div that triggers a prompt, and then check that focus is back on
  // the editable iframe.
  let dialogShown = awaitAndClosePrompt(browser);
  await SpecialPowers.spawn(browser, [], async function() {
    let div = content.document.getElementById("clickMeDiv");
    div.click();
  });
  await dialogShown;
  let blurCameFirst = await focusBlurPromise;
  await SpecialPowers.spawn(browser, [], async function() {
    is(
      content.document.activeElement,
      content.document.getElementById("edit"),
      "Focus should be back on iframe element"
    );
  });
  ok(blurCameFirst, "Should receive blur and then focus event");

  BrowserTestUtils.removeTab(tab);
});
