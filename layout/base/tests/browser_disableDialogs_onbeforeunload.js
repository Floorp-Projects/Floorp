const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);

function pageScript() {
  window.addEventListener(
    "beforeunload",
    function (event) {
      var str = "Some text that causes the beforeunload dialog to be shown";
      event.returnValue = str;
      return str;
    },
    true
  );
}

SpecialPowers.pushPrefEnv({
  set: [["dom.require_user_interaction_for_beforeunload", false]],
});

const PAGE_URL =
  "data:text/html," +
  encodeURIComponent("<script>(" + pageScript.toSource() + ")();</script>");

add_task(async function enableDialogs() {
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

add_task(async function disableDialogs() {
  // The onbeforeunload dialog should NOT appear.
  await openPage(false);
  info("If we time out here, then the dialog was shown...");
});

async function openPage(enableDialogs) {
  // Open about:blank in a new tab.
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      // Load the page.
      BrowserTestUtils.startLoadingURIString(browser, PAGE_URL);
      await BrowserTestUtils.browserLoaded(browser);
      // Load the content script in the frame.
      let methodName = enableDialogs ? "enableDialogs" : "disableDialogs";
      await SpecialPowers.spawn(browser, [methodName], async function (name) {
        content.windowUtils[name]();
      });
      // And then navigate away.
      BrowserTestUtils.startLoadingURIString(browser, "http://example.com/");
      await BrowserTestUtils.browserLoaded(browser);
    }
  );
}
