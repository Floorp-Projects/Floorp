/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_download_shortcut() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.useDownloadDir", true]],
  });

  let publicDownloads = await Downloads.getList(Downloads.PUBLIC);
  // First ensure we catch the download finishing.
  let downloadFinishedPromise = new Promise(resolve => {
    publicDownloads.addView({
      onDownloadChanged(download) {
        info("Download changed!");
        if (download.succeeded || download.error) {
          info("Download succeeded or errored");
          publicDownloads.removeView(this);
          resolve(download);
        }
      },
    });
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();
      await helper.dragOverlay(10, 10, 500, 500);

      let screenshotExit = TestUtils.topicObserved("screenshots-exit");

      await SpecialPowers.spawn(browser, [], async () => {
        EventUtils.synthesizeKey("s", { accelKey: true }, content);
      });

      info("wait for download to finish");
      let download = await downloadFinishedPromise;

      ok(download.succeeded, "Download should succeed");

      await publicDownloads.removeFinished();
      await screenshotExit;

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      let screenshotReady = TestUtils.topicObserved(
        "screenshots-preview-ready"
      );

      let visibleButton = await helper.getPanelButton("#visible-page");
      visibleButton.click();

      await screenshotReady;

      screenshotExit = TestUtils.topicObserved("screenshots-exit");

      EventUtils.synthesizeKey("s", { accelKey: true });

      info("wait for download to finish");
      download = await downloadFinishedPromise;

      ok(download.succeeded, "Download should succeed");

      await publicDownloads.removeFinished();
      await screenshotExit;
    }
  );
});

add_task(async function test_copy_shortcut() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();
      await helper.dragOverlay(10, 10, 500, 500);

      let screenshotExit = TestUtils.topicObserved("screenshots-exit");
      let clipboardChanged = helper.waitForRawClipboardChange(490, 490);

      await SpecialPowers.spawn(browser, [], async () => {
        EventUtils.synthesizeKey("c", { accelKey: true }, content);
      });

      await clipboardChanged;
      await screenshotExit;

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      let screenshotReady = TestUtils.topicObserved(
        "screenshots-preview-ready"
      );

      let visibleButton = await helper.getPanelButton("#visible-page");
      visibleButton.click();

      await screenshotReady;

      clipboardChanged = helper.waitForRawClipboardChange(
        contentInfo.clientWidth,
        contentInfo.clientHeight
      );
      screenshotExit = TestUtils.topicObserved("screenshots-exit");

      EventUtils.synthesizeKey("c", { accelKey: true });

      await clipboardChanged;
      await screenshotExit;
    }
  );
});
