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

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.always_ask_before_handling_new_types", false],
    ],
  });

  const MimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  const HandlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
    Ci.nsIHandlerService
  );
  let mimeInfo = MimeSvc.getFromTypeAndExtension(
    HandlerSvc.getTypeFromExtension("txt"),
    "txt"
  );
  let existed = HandlerSvc.exists(mimeInfo);
  mimeInfo.alwaysAskBeforeHandling = false;
  mimeInfo.preferredAction = Ci.nsIHandlerInfo.saveToDisk;
  HandlerSvc.store(mimeInfo);

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
      registerCleanupFunction(async () => {
        if (existed) {
          HandlerSvc.store(mimeInfo);
        } else {
          HandlerSvc.remove(mimeInfo);
        }
        await download.finalize(true);
        if (Services.appinfo.OS === "WINNT") {
          // We need to make the file writable to delete it on Windows.
          await IOUtils.setPermissions(download.target.path, 0o600);
        }
        await IOUtils.remove(download.target.path);
        await download.finalize();
        await list.removeFinished();
      });

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
    }
  );
});
