/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test exists because we use a <panelmultiview> element and it handles
 * some of the height changes for us. We need to verify that the height is
 * updated correctly if downloads are removed while the panel is hidden.
 */
add_task(async function test_height_reduced_after_removal() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.autohideButton", false]],
  });
  await promiseButtonShown("downloads-button");
  // downloading two items since the download panel only shows up when at least one item is in it
  await task_addDownloads([{ state: DownloadsCommon.DOWNLOAD_FINISHED }]);
  await task_addDownloads([{ state: DownloadsCommon.DOWNLOAD_FINISHED }]);

  await task_openPanel();
  let panel = document.getElementById("downloadsPanel");
  let heightBeforeRemoval = panel.getBoundingClientRect().height;

  // We want to close the panel before we remove the download from the list.
  DownloadsPanel.hidePanel();
  await task_resetState();
  // keep at least one item in the download list since the panel disabled when it is empty
  await task_addDownloads([{ state: DownloadsCommon.DOWNLOAD_FINISHED }]);

  await task_openPanel();
  let heightAfterRemoval = panel.getBoundingClientRect().height;
  Assert.greater(heightBeforeRemoval, heightAfterRemoval);

  await task_resetState();
});
