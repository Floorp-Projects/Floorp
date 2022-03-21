/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BUILDER = "http://mochi.test:8888/document-builder.sjs?html=";
const PAGE_1 = BUILDER + encodeURIComponent(`<html><body>Page 1</body></html>`);
const PAGE_2 = BUILDER + encodeURIComponent(`<html><body>Page 2</body></html>`);

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.history.collectWireframes", true]],
  });
});

/**
 * Test that capturing wireframes on nsISHEntriy's in the parent process
 * happens at the right times.
 */
add_task(async function() {
  await BrowserTestUtils.withNewTab(PAGE_1, async browser => {
    let sh = browser.browsingContext.sessionHistory;
    Assert.equal(sh.count, 1, "Got the right SessionHistory entry count.");
    Assert.ok(
      !sh.getEntryAtIndex(0).wireframe,
      "No wireframe for the loaded entry."
    );

    let loaded = BrowserTestUtils.browserLoaded(browser, false, PAGE_2);
    BrowserTestUtils.loadURI(browser, PAGE_2);
    await loaded;

    Assert.equal(sh.count, 2, "Got the right SessionHistory entry count.");
    Assert.ok(
      sh.getEntryAtIndex(0).wireframe,
      "A wireframe was captured for the first entry."
    );
    Assert.ok(
      !sh.getEntryAtIndex(1).wireframe,
      "No wireframe for the loaded entry."
    );

    // Now go back
    loaded = BrowserTestUtils.waitForContentEvent(browser, "pageshow");
    browser.goBack();
    await loaded;
    // These are TODO due to bug 1759528.
    todo(
      sh.getEntryAtIndex(1).wireframe,
      "A wireframe was captured for the second entry."
    );
    todo(
      !sh.getEntryAtIndex(0).wireframe,
      "No wireframe for the loaded entry."
    );

    // Now forward again
    loaded = BrowserTestUtils.waitForContentEvent(browser, "pageshow");
    browser.goForward();
    await loaded;

    Assert.equal(sh.count, 2, "Got the right SessionHistory entry count.");
    Assert.ok(
      sh.getEntryAtIndex(0).wireframe,
      "A wireframe was captured for the first entry."
    );
    Assert.ok(
      !sh.getEntryAtIndex(1).wireframe,
      "No wireframe for the loaded entry."
    );

    // And using pushState
    await SpecialPowers.spawn(browser, [], async () => {
      content.history.pushState({}, "", "nothing-1.html");
      content.history.pushState({}, "", "nothing-2.html");
    });

    Assert.equal(sh.count, 4, "Got the right SessionHistory entry count.");
    Assert.ok(
      sh.getEntryAtIndex(0).wireframe,
      "A wireframe was captured for the first entry."
    );
    Assert.ok(
      sh.getEntryAtIndex(1).wireframe,
      "A wireframe was captured for the second entry."
    );
    Assert.ok(
      sh.getEntryAtIndex(2).wireframe,
      "A wireframe was captured for the third entry."
    );
    Assert.ok(
      !sh.getEntryAtIndex(3).wireframe,
      "No wireframe for the loaded entry."
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
