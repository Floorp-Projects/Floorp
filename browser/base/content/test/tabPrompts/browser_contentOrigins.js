/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PromptTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromptTestUtils.jsm"
);

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

async function checkAlert(
  pageToLoad,
  expectedTitle,
  expectedIcon = "chrome://global/skin/icons/defaultFavicon.svg"
) {
  function openFn(browser) {
    return SpecialPowers.spawn(browser, [], () => {
      if (content.document.nodePrincipal.isSystemPrincipal) {
        // Can't eval in privileged contexts due to CSP, just call directly:
        content.alert("Test");
      } else {
        // Eval everywhere else so it gets the principal of the loaded page.
        content.eval("alert('Test')");
      }
    });
  }
  return checkDialog(pageToLoad, openFn, expectedTitle, expectedIcon);
}

async function checkBeforeunload(
  pageToLoad,
  expectedTitle,
  expectedIcon = "chrome://global/skin/icons/defaultFavicon.svg"
) {
  async function openFn(browser) {
    let tab = gBrowser.getTabForBrowser(browser);
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "body",
      {},
      browser.browsingContext
    );
    return gBrowser.removeTab(tab); // trigger beforeunload.
  }
  return checkDialog(pageToLoad, openFn, expectedTitle, expectedIcon);
}

async function checkDialog(pageToLoad, openFn, expectedTitle, expectedIcon) {
  return BrowserTestUtils.withNewTab(pageToLoad, async browser => {
    let promptPromise = PromptTestUtils.waitForPrompt(browser, {
      modalType: Ci.nsIPrompt.MODAL_TYPE_CONTENT,
    });
    let spawnPromise = openFn(browser);
    let dialog = await promptPromise;

    let doc = dialog.ui.prompt.document;
    let titleEl = doc.getElementById("titleText");
    if (expectedTitle.value) {
      is(titleEl.textContent, expectedTitle.value, "Title should match.");
    } else {
      is(
        titleEl.dataset.l10nId,
        expectedTitle.l10nId,
        "Title l10n id should match."
      );
    }
    ok(
      !titleEl.parentNode.hasAttribute("overflown"),
      "Title should fit without overflowing."
    );

    ok(BrowserTestUtils.is_visible(titleEl), "New title should be shown.");
    ok(
      BrowserTestUtils.is_hidden(doc.getElementById("infoTitle")),
      "Old title should be hidden."
    );
    let iconCS = doc.ownerGlobal.getComputedStyle(
      doc.querySelector(".titleIcon")
    );
    is(
      iconCS.backgroundImage,
      `url("${expectedIcon}")`,
      "Icon is as expected."
    );

    // This is not particularly neat, but we want to also test overflow
    // Our test systems don't have hosts that long, so just fake it:
    if (browser.currentURI.asciiHost == "example.com") {
      let longerDomain = "extravagantly.long.".repeat(10) + "example.com";
      doc.documentElement.setAttribute(
        "headertitle",
        JSON.stringify({ raw: longerDomain, shouldUseMaskFade: true })
      );
      info("Wait for the prompt title to update.");
      await BrowserTestUtils.waitForMutationCondition(
        titleEl,
        { characterData: true, attributes: true },
        () =>
          titleEl.textContent == longerDomain &&
          titleEl.parentNode.hasAttribute("overflown")
      );
      is(titleEl.textContent, longerDomain, "The longer domain is reflected.");
      ok(
        titleEl.parentNode.hasAttribute("overflown"),
        "The domain should overflow."
      );
    }

    // Work around https://bugzilla.mozilla.org/show_bug.cgi?id=1699844 by
    // waiting before closing this prompt:
    await (async function() {
      let rAFCount = 3;
      while (rAFCount--) {
        await new Promise(requestAnimationFrame);
      }
    })();

    // Close the prompt again.
    await PromptTestUtils.handlePrompt(dialog);
    // The alert in the content process was sync, we need to make sure it gets
    // cleaned up, but couldn't await it above because that'd hang the test!
    await spawnPromise;
  });
}

add_task(async function test_check_prompt_origin_display() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.proton.enabled", true],
      ["browser.proton.modals.enabled", true],
      ["prompts.contentPromptSubDialog", true],
    ],
  });

  await checkAlert("https://example.com/", { value: "example.com" });
  await checkAlert("http://example.com/", { value: "example.com" });
  await checkAlert("data:text/html,<body>", {
    l10nId: "common-dialog-title-null",
  });
  await checkAlert(
    "about:config",
    { l10nId: "common-dialog-title-system" },
    "chrome://branding/content/icon32.png"
  );

  await checkBeforeunload(TEST_ROOT + "file_beforeunload_stop.html", {
    value: "example.com",
  });
});
