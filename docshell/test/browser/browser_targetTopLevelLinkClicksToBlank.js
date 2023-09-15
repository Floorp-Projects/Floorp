/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test exercises the behaviour where user-initiated link clicks on
 * the top-level document result in pageloads in a _blank target in a new
 * browser window.
 */

const TEST_PAGE = "https://example.com/browser/";
const TEST_PAGE_2 = "https://example.com/browser/components/";
const TEST_IFRAME_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "dummy_iframe_page.html";

// There is an <a> element with this href=".." in the TEST_PAGE
// that we will click, which should take us up a level.
const LINK_URL = "https://example.com/";

/**
 * Test that a user-initiated link click results in targeting to a new
 * <browser> element, and that this properly sets the referrer on the newly
 * loaded document.
 */
add_task(async function target_to_new_blank_browser() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let originalTab = win.gBrowser.selectedTab;
  let originalBrowser = originalTab.linkedBrowser;
  BrowserTestUtils.startLoadingURIString(originalBrowser, TEST_PAGE);
  await BrowserTestUtils.browserLoaded(originalBrowser, false, TEST_PAGE);

  // Now set the targetTopLevelLinkClicksToBlank property to true, since it
  // defaults to false.
  originalBrowser.browsingContext.targetTopLevelLinkClicksToBlank = true;

  let newTabPromise = BrowserTestUtils.waitForNewTab(win.gBrowser, LINK_URL);
  await SpecialPowers.spawn(originalBrowser, [], async () => {
    let anchor = content.document.querySelector(`a[href=".."]`);
    let userInput = content.windowUtils.setHandlingUserInput(true);
    try {
      anchor.click();
    } finally {
      userInput.destruct();
    }
  });
  let newTab = await newTabPromise;
  let newBrowser = newTab.linkedBrowser;

  Assert.ok(
    originalBrowser !== newBrowser,
    "A new browser should have been created."
  );
  await SpecialPowers.spawn(newBrowser, [TEST_PAGE], async referrer => {
    Assert.equal(
      content.document.referrer,
      referrer,
      "Should have gotten the right referrer set"
    );
  });
  await BrowserTestUtils.switchTab(win.gBrowser, originalTab);
  BrowserTestUtils.removeTab(newTab);

  // Now do the same thing with a subframe targeting "_top". This should also
  // get redirected to "_blank".
  BrowserTestUtils.startLoadingURIString(originalBrowser, TEST_IFRAME_PAGE);
  await BrowserTestUtils.browserLoaded(
    originalBrowser,
    false,
    TEST_IFRAME_PAGE
  );

  newTabPromise = BrowserTestUtils.waitForNewTab(win.gBrowser, LINK_URL);
  let frameBC1 = originalBrowser.browsingContext.children[0];
  Assert.ok(frameBC1, "Should have found a subframe BrowsingContext");

  await SpecialPowers.spawn(frameBC1, [LINK_URL], async linkUrl => {
    let anchor = content.document.createElement("a");
    anchor.setAttribute("href", linkUrl);
    anchor.setAttribute("target", "_top");
    content.document.body.appendChild(anchor);
    let userInput = content.windowUtils.setHandlingUserInput(true);
    try {
      anchor.click();
    } finally {
      userInput.destruct();
    }
  });
  newTab = await newTabPromise;
  newBrowser = newTab.linkedBrowser;

  Assert.ok(
    originalBrowser !== newBrowser,
    "A new browser should have been created."
  );
  await SpecialPowers.spawn(
    newBrowser,
    [frameBC1.currentURI.spec],
    async referrer => {
      Assert.equal(
        content.document.referrer,
        referrer,
        "Should have gotten the right referrer set"
      );
    }
  );
  await BrowserTestUtils.switchTab(win.gBrowser, originalTab);
  BrowserTestUtils.removeTab(newTab);

  await BrowserTestUtils.closeWindow(win);
});

/**
 * Test that we don't target to _blank loads caused by:
 * 1. POST requests
 * 2. Any load that isn't "normal" (in the nsIDocShell.LOAD_CMD_NORMAL sense)
 * 3. Any loads that are caused by location.replace
 * 4. Any loads that were caused by setting location.href
 * 5. Link clicks fired without user interaction.
 */
add_task(async function skip_blank_target_for_some_loads() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let currentBrowser = win.gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(currentBrowser, TEST_PAGE);
  await BrowserTestUtils.browserLoaded(currentBrowser, false, TEST_PAGE);

  // Now set the targetTopLevelLinkClicksToBlank property to true, since it
  // defaults to false.
  currentBrowser.browsingContext.targetTopLevelLinkClicksToBlank = true;

  let ensureSingleBrowser = () => {
    Assert.equal(
      win.gBrowser.browsers.length,
      1,
      "There should only be 1 browser."
    );

    Assert.ok(
      currentBrowser.browsingContext.targetTopLevelLinkClicksToBlank,
      "Should still be targeting top-level clicks to _blank"
    );
  };

  // First we'll test a POST request
  let sameBrowserLoad = BrowserTestUtils.browserLoaded(
    currentBrowser,
    false,
    TEST_PAGE
  );
  await SpecialPowers.spawn(currentBrowser, [], async () => {
    let doc = content.document;
    let form = doc.createElement("form");
    form.setAttribute("method", "post");
    doc.body.appendChild(form);
    let userInput = content.windowUtils.setHandlingUserInput(true);
    try {
      form.submit();
    } finally {
      userInput.destruct();
    }
  });
  await sameBrowserLoad;
  ensureSingleBrowser();

  // Next, we'll try a non-normal load - specifically, we'll try a reload.
  // Since we've got a page loaded via a POST request, an attempt to reload
  // will cause the "repost" dialog to appear, so we temporarily allow the
  // repost to go through with the always_accept testing pref.
  await SpecialPowers.pushPrefEnv({
    set: [["dom.confirm_repost.testing.always_accept", true]],
  });
  sameBrowserLoad = BrowserTestUtils.browserLoaded(
    currentBrowser,
    false,
    TEST_PAGE
  );
  await SpecialPowers.spawn(currentBrowser, [], async () => {
    let userInput = content.windowUtils.setHandlingUserInput(true);
    try {
      content.location.reload();
    } finally {
      userInput.destruct();
    }
  });
  await sameBrowserLoad;
  ensureSingleBrowser();
  await SpecialPowers.popPrefEnv();

  // Next, we'll try a location.replace
  sameBrowserLoad = BrowserTestUtils.browserLoaded(
    currentBrowser,
    false,
    TEST_PAGE_2
  );
  await SpecialPowers.spawn(currentBrowser, [TEST_PAGE_2], async page2 => {
    let userInput = content.windowUtils.setHandlingUserInput(true);
    try {
      content.location.replace(page2);
    } finally {
      userInput.destruct();
    }
  });
  await sameBrowserLoad;
  ensureSingleBrowser();

  // Finally we'll try setting location.href
  sameBrowserLoad = BrowserTestUtils.browserLoaded(
    currentBrowser,
    false,
    TEST_PAGE
  );
  await SpecialPowers.spawn(currentBrowser, [TEST_PAGE], async page1 => {
    let userInput = content.windowUtils.setHandlingUserInput(true);
    try {
      content.location.href = page1;
    } finally {
      userInput.destruct();
    }
  });
  await sameBrowserLoad;
  ensureSingleBrowser();

  // Now that we're back at TEST_PAGE, let's try a scripted link click. This
  // shouldn't target to _blank.
  sameBrowserLoad = BrowserTestUtils.browserLoaded(
    currentBrowser,
    false,
    LINK_URL
  );
  await SpecialPowers.spawn(currentBrowser, [], async () => {
    let anchor = content.document.querySelector(`a[href=".."]`);
    anchor.click();
  });
  await sameBrowserLoad;
  ensureSingleBrowser();

  // A javascript:void(0); link should also not target to _blank.
  sameBrowserLoad = BrowserTestUtils.browserLoaded(
    currentBrowser,
    false,
    TEST_PAGE
  );
  await SpecialPowers.spawn(currentBrowser, [TEST_PAGE], async newPageURL => {
    let anchor = content.document.querySelector(`a[href=".."]`);
    anchor.href = "javascript:void(0);";
    anchor.addEventListener("click", e => {
      content.location.href = newPageURL;
    });
    let userInput = content.windowUtils.setHandlingUserInput(true);
    try {
      anchor.click();
    } finally {
      userInput.destruct();
    }
  });
  await sameBrowserLoad;
  ensureSingleBrowser();

  // Let's also try a non-void javascript: location.
  sameBrowserLoad = BrowserTestUtils.browserLoaded(
    currentBrowser,
    false,
    TEST_PAGE
  );
  await SpecialPowers.spawn(currentBrowser, [TEST_PAGE], async newPageURL => {
    let anchor = content.document.querySelector(`a[href=".."]`);
    anchor.href = `javascript:"string-to-navigate-to"`;
    anchor.addEventListener("click", e => {
      content.location.href = newPageURL;
    });
    let userInput = content.windowUtils.setHandlingUserInput(true);
    try {
      anchor.click();
    } finally {
      userInput.destruct();
    }
  });
  await sameBrowserLoad;
  ensureSingleBrowser();

  await BrowserTestUtils.closeWindow(win);
});
