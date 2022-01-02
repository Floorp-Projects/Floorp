"use strict";

add_task(async function doCheckPasteEventAtMiddleClickOnAnchorElement() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.opentabfor.middleclick", true],
      ["middlemouse.paste", true],
      ["middlemouse.contentLoadURL", false],
      ["general.autoScroll", false],
    ],
  });

  await new Promise((resolve, reject) => {
    SimpleTest.waitForClipboard(
      "Text in the clipboard",
      () => {
        Cc["@mozilla.org/widget/clipboardhelper;1"]
          .getService(Ci.nsIClipboardHelper)
          .copyString("Text in the clipboard");
      },
      resolve,
      () => {
        ok(false, "Clipboard copy failed");
        reject();
      }
    );
  });

  is(
    gBrowser.tabs.length,
    1,
    "Number of tabs should be 1 at starting this test #1"
  );

  let pageURL = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  );
  pageURL = `${pageURL}file_anchor_elements.html`;
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageURL);

  let pasteEventCount = 0;
  BrowserTestUtils.addContentEventListener(
    gBrowser.selectedBrowser,
    "paste",
    () => {
      ++pasteEventCount;
    }
  );

  // Click the usual link.
  ok(true, "Clicking on usual link...");
  let newTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "https://example.com/#a_with_href",
    true
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#a_with_href",
    { button: 1 },
    gBrowser.selectedBrowser
  );
  let openTabForUsualLink = await newTabPromise;
  is(
    openTabForUsualLink.linkedBrowser.currentURI.spec,
    "https://example.com/#a_with_href",
    "Middle click should open site to correct url at clicking on usual link"
  );
  is(
    pasteEventCount,
    0,
    "paste event should be suppressed when clicking on usual link"
  );

  // Click the link in editing host.
  is(
    gBrowser.tabs.length,
    3,
    "Number of tabs should be 3 at starting this test #2"
  );
  ok(true, "Clicking on editable link...");
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#editable_a_with_href",
    { button: 1 },
    gBrowser.selectedBrowser
  );
  await TestUtils.waitForCondition(
    () => pasteEventCount >= 1,
    "Waiting for paste event caused by clicking on editable link"
  );
  is(
    pasteEventCount,
    1,
    "paste event should be suppressed when clicking on editable link"
  );
  is(
    gBrowser.tabs.length,
    3,
    "Clicking on editable link shouldn't open new tab"
  );

  // Click the link in non-editable area in editing host.
  ok(true, "Clicking on non-editable link in an editing host...");
  newTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "https://example.com/#non-editable_a_with_href",
    true
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#non-editable_a_with_href",
    { button: 1 },
    gBrowser.selectedBrowser
  );
  let openTabForNonEditableLink = await newTabPromise;
  is(
    openTabForNonEditableLink.linkedBrowser.currentURI.spec,
    "https://example.com/#non-editable_a_with_href",
    "Middle click should open site to correct url at clicking on non-editable link in an editing host."
  );
  is(
    pasteEventCount,
    1,
    "paste event should be suppressed when clicking on non-editable link in an editing host"
  );

  // Click the <a> element without href attribute.
  is(
    gBrowser.tabs.length,
    4,
    "Number of tabs should be 4 at starting this test #3"
  );
  ok(true, "Clicking on anchor element without href...");
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#a_with_name",
    { button: 1 },
    gBrowser.selectedBrowser
  );
  await TestUtils.waitForCondition(
    () => pasteEventCount >= 2,
    "Waiting for paste event caused by clicking on anchor element without href"
  );
  is(
    pasteEventCount,
    2,
    "paste event should be suppressed when clicking on anchor element without href"
  );
  is(
    gBrowser.tabs.length,
    4,
    "Clicking on anchor element without href shouldn't open new tab"
  );

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(openTabForUsualLink);
  BrowserTestUtils.removeTab(openTabForNonEditableLink);
});
