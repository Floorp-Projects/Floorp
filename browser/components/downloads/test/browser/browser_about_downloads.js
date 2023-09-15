/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Ensure about:downloads actually works.
 */
add_task(async function test_about_downloads() {
  await task_resetState();
  registerCleanupFunction(task_resetState);

  await setDownloadDir();

  await task_addDownloads([
    { state: DownloadsCommon.DOWNLOAD_FINISHED },
    { state: DownloadsCommon.DOWNLOAD_PAUSED },
  ]);

  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    let downloadsLoaded = BrowserTestUtils.waitForEvent(
      browser,
      "InitialDownloadsLoaded",
      true
    );
    BrowserTestUtils.startLoadingURIString(browser, "about:downloads");
    await downloadsLoaded;
    await SpecialPowers.spawn(browser, [], async function () {
      let box = content.document.getElementById("downloadsListBox");
      ok(box, "Should have list of downloads");
      is(box.children.length, 2, "Should have 2 downloads.");
      for (let kid of box.children) {
        let desc = kid.querySelector(".downloadTarget");
        // This would just be an `is` check, but stray temp files
        // if this test (or another in this dir) ever fails could throw that off.
        ok(
          /^dm-ui-test(-\d+)?.file$/.test(desc.value),
          `Label '${desc.value}' should match 'dm-ui-test.file'`
        );
      }
      ok(box.firstChild.selected, "First item should be selected.");
    });
  });
});
