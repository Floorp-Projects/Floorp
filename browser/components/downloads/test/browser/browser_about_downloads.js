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
  await BrowserTestUtils.withNewTab("about:downloads", async function(browser) {
    await SpecialPowers.spawn(browser, [], async function() {
      let box = content.document.getElementById("downloadsRichListBox");
      ok(box, "Should have list of downloads");
      is(box.children.length, 2, "Should have 2 downloads.");
      for (let kid of box.children) {
        let desc = kid.querySelector(".downloadTarget");
        is(
          desc.value,
          "dm-ui-test.file",
          "Should have listed the file name for this download."
        );
      }
      info("Wait for the first item to be selected.");
      await ContentTaskUtils.waitForCondition(() => {
        return box.firstChild.selected;
      }, "Timed out waiting for the first item to be selected.");
    });
  });
});
