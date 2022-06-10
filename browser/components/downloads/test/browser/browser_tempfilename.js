/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_tempfilename() {
  startServer();
  let downloadURL = httpUrl("interruptible.txt");
  let list = await Downloads.getList(Downloads.PUBLIC);
  let downloadStarted = new Promise(resolve => {
    let view = {
      onDownloadAdded(download) {
        list.removeView(view);
        resolve(download);
      },
    };
    list.addView(view);
  });
  serveInterruptibleAsDownload();
  mustInterruptResponses();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: downloadURL,
      waitForLoad: false,
      waitForStop: true,
    },
    async () => {
      let download = await downloadStarted;
      registerCleanupFunction(() => download.finalize());

      let { partFilePath } = download.target;
      Assert.stringContains(
        partFilePath,
        "interruptible",
        "Should keep bit of original filename."
      );
      isnot(
        PathUtils.filename(partFilePath),
        "interruptible.txt.part",
        "Should not just have original filename."
      );
      ok(
        partFilePath.endsWith(".txt.part"),
        `${PathUtils.filename(partFilePath)} should end with .txt.part`
      );
      let promiseFinished = download.whenSucceeded();
      continueResponses();
      await promiseFinished;
      ok(
        !(await IOUtils.exists(download.target.partFilePath)),
        "Temp file should be gone."
      );
      await IOUtils.remove(download.target.path);
    }
  );
});
