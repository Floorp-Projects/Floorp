const { PromptTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromptTestUtils.jsm"
);

function pageScript() {
  window.addEventListener(
    "beforeunload",
    function(event) {
      var str = "Some text that causes the beforeunload dialog to be shown";
      event.returnValue = str;
      return str;
    },
    true
  );
}

SpecialPowers.pushPrefEnv({
  set: [["dom.require_user_interaction_for_beforeunload", true]],
});

const PAGE_URL =
  "data:text/html," +
  encodeURIComponent("<script>(" + pageScript.toSource() + ")();</script>");

add_task(async function doClick() {
  // The onbeforeunload dialog should appear.
  let dialogPromise = PromptTestUtils.waitForPrompt(null, {
    modalType: Services.prompt.MODAL_TYPE_CONTENT,
    promptType: "confirmEx",
  });

  let openPagePromise = openPage(true);
  let dialog = await dialogPromise;
  Assert.ok(true, "Showed the beforeunload dialog.");

  await PromptTestUtils.handlePrompt(dialog, { buttonNumClick: 0 });
  await openPagePromise;
});

add_task(async function noClick() {
  // The onbeforeunload dialog should NOT appear.
  await openPage(false);
  info("If we time out here, then the dialog was shown...");
});

async function openPage(shouldClick) {
  // Open about:blank in a new tab.
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      // Load the page.
      BrowserTestUtils.loadURI(browser, PAGE_URL);
      await BrowserTestUtils.browserLoaded(browser);

      if (shouldClick) {
        await BrowserTestUtils.synthesizeMouse("body", 2, 2, {}, browser);
      }
      let hasInteractedWith = await SpecialPowers.spawn(
        browser,
        [""],
        function() {
          return content.document.userHasInteracted;
        }
      );
      is(
        shouldClick,
        hasInteractedWith,
        "Click should update document interactivity state"
      );
      // And then navigate away.
      BrowserTestUtils.loadURI(browser, "http://example.com/");
      await BrowserTestUtils.browserLoaded(browser);
    }
  );
}
