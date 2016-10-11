/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test exists because we use a <panelmultiview> element and it handles
 * some of the height changes for us. We need to verify that the height is
 * updated correctly if downloads are removed while the panel is hidden.
 */
add_task(function* test_height_reduced_after_removal() {
  yield task_addDownloads([
    { state: nsIDM.DOWNLOAD_FINISHED },
  ]);

  yield task_openPanel();
  let panel = document.getElementById("downloadsPanel");
  let heightBeforeRemoval = panel.getBoundingClientRect().height;

  // We want to close the panel before we remove the download from the list.
  DownloadsPanel.hidePanel();
  yield task_resetState();

  yield task_openPanel();
  let heightAfterRemoval = panel.getBoundingClientRect().height;
  Assert.greater(heightBeforeRemoval, heightAfterRemoval);

  yield task_resetState();
});
