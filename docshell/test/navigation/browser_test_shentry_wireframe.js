/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BUILDER = "http://mochi.test:8888/document-builder.sjs?html=";
const PAGE_1 = BUILDER + encodeURIComponent(`<html><body>Page 1</body></html>`);
const PAGE_2 = BUILDER + encodeURIComponent(`<html><body>Page 2</body></html>`);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.history.collectWireframes", true]],
  });
});

/**
 * Test that capturing wireframes on nsISHEntriy's in the parent process
 * happens at the right times.
 */
add_task(async function () {
  await BrowserTestUtils.withNewTab(PAGE_1, async browser => {
    let sh = browser.browsingContext.sessionHistory;
    Assert.equal(
      sh.count,
      1,
      "Got the right SessionHistory entry count after initial tab load."
    );
    Assert.ok(
      !sh.getEntryAtIndex(0).wireframe,
      "No wireframe for the loaded entry after initial tab load."
    );

    let loaded = BrowserTestUtils.browserLoaded(browser, false, PAGE_2);
    BrowserTestUtils.startLoadingURIString(browser, PAGE_2);
    await loaded;

    Assert.equal(
      sh.count,
      2,
      "Got the right SessionHistory entry count after loading page 2."
    );
    Assert.ok(
      sh.getEntryAtIndex(0).wireframe,
      "A wireframe was captured for the first entry after loading page 2."
    );
    Assert.ok(
      !sh.getEntryAtIndex(1).wireframe,
      "No wireframe for the loaded entry after loading page 2."
    );

    // Now go back
    loaded = BrowserTestUtils.waitForContentEvent(browser, "pageshow");
    browser.goBack();
    await loaded;
    Assert.ok(
      sh.getEntryAtIndex(1).wireframe,
      "A wireframe was captured for the second entry after going back."
    );
    Assert.ok(
      !sh.getEntryAtIndex(0).wireframe,
      "No wireframe for the loaded entry after going back."
    );

    // Now forward again
    loaded = BrowserTestUtils.waitForContentEvent(browser, "pageshow");
    browser.goForward();
    await loaded;

    Assert.equal(
      sh.count,
      2,
      "Got the right SessionHistory entry count after going forward again."
    );
    Assert.ok(
      sh.getEntryAtIndex(0).wireframe,
      "A wireframe was captured for the first entry after going forward again."
    );
    Assert.ok(
      !sh.getEntryAtIndex(1).wireframe,
      "No wireframe for the loaded entry after going forward again."
    );

    // And using pushState
    await SpecialPowers.spawn(browser, [], async () => {
      content.history.pushState({}, "", "nothing-1.html");
      content.history.pushState({}, "", "nothing-2.html");
    });

    Assert.equal(
      sh.count,
      4,
      "Got the right SessionHistory entry count after using pushState."
    );
    Assert.ok(
      sh.getEntryAtIndex(0).wireframe,
      "A wireframe was captured for the first entry after using pushState."
    );
    Assert.ok(
      sh.getEntryAtIndex(1).wireframe,
      "A wireframe was captured for the second entry after using pushState."
    );
    Assert.ok(
      sh.getEntryAtIndex(2).wireframe,
      "A wireframe was captured for the third entry after using pushState."
    );
    Assert.ok(
      !sh.getEntryAtIndex(3).wireframe,
      "No wireframe for the loaded entry after using pushState."
    );

    // Now check that wireframes can be written to in case we're restoring
    // an nsISHEntry from serialization.
    let wireframe = sh.getEntryAtIndex(2).wireframe;
    sh.getEntryAtIndex(2).wireframe = null;
    Assert.equal(
      sh.getEntryAtIndex(2).wireframe,
      null,
      "Successfully cleared wireframe."
    );

    sh.getEntryAtIndex(3).wireframe = wireframe;
    Assert.deepEqual(
      sh.getEntryAtIndex(3).wireframe,
      wireframe,
      "Successfully wrote a wireframe to an nsISHEntry."
    );
  });
});
