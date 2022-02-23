add_task(async function test_fullPageScreenshot() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_GREEN_PAGE,
    },

    async browser => {
      const tests = [
        { title: "Green Page", expected: "Green Page.png" },
        { title: "\tA*B\\/+?<>\u200f\x1fC ", expected: "A B__ C.png" },
        {
          title: "簡単".repeat(35),
          expected: " " + "簡単".repeat(35) + ".png",
        },
        {
          title: "簡単".repeat(36),
          expected: " " + "簡単".repeat(26) + "[...].png",
        },
        {
          title: "簡単".repeat(56) + "?",
          expected: " " + "簡単".repeat(26) + "[...].png",
        },
      ];

      for (let test of tests) {
        info("Testing with title " + test.title);

        await SpecialPowers.spawn(browser, [test.title], titleToUse => {
          content.document.title = titleToUse;
        });

        let helper = new ScreenshotsHelper(browser);
        await helper.triggerUIFromToolbar();

        await helper.clickUIElement(
          helper.selector.preselectIframe,
          helper.selector.fullPageButton
        );

        info("Waiting for the preview UI and the download button");
        await helper.waitForUIContent(
          helper.selector.previewIframe,
          helper.selector.downloadButton
        );

        let publicList = await Downloads.getList(Downloads.PUBLIC);
        let downloadPromise = new Promise(resolve => {
          let downloadView = {
            onDownloadAdded(download) {
              publicList.removeView(downloadView);
              resolve(download);
            },
          };

          publicList.addView(downloadView);
        });

        await helper.clickUIElement(
          helper.selector.previewIframe,
          helper.selector.downloadButton
        );

        let download = await downloadPromise;
        let filename = PathUtils.filename(download.target.path);
        ok(
          filename.endsWith(test.expected),
          "Used correct filename '" +
            filename +
            "', expected: '" +
            test.expected +
            "'"
        );

        await task_resetState();
      }
    }
  );
});

// This is from browser/components/downloads/test/browser/head.js
async function task_resetState() {
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let downloads = await publicList.getAll();
  for (let download of downloads) {
    await publicList.remove(download);
    await download.finalize(true);
    if (await IOUtils.exists(download.target.path)) {
      if (Services.appinfo.OS === "WINNT") {
        // We need to make the file writable to delete it on Windows.
        await IOUtils.setPermissions(download.target.path, 0o600);
      }
      await IOUtils.remove(download.target.path);
    }
  }

  DownloadsPanel.hidePanel();
}
