/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.org"
);

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

registerCleanupFunction(async function() {
  info("Running the cleanup code");
  MockFilePicker.cleanup();
  if (gTestDir && gTestDir.exists()) {
    // On Windows, sometimes nsIFile.remove() throws, probably because we're
    // still writing to the directory we're trying to remove, despite
    // waiting for the download to complete. Just retry a bit later...
    let succeeded = false;
    while (!succeeded) {
      try {
        gTestDir.remove(true);
        succeeded = true;
      } catch (ex) {
        await new Promise(requestAnimationFrame);
      }
    }
  }
});

let gTestDir = null;

function createTemporarySaveDirectory() {
  var saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists()) {
    info("create testsavedir!");
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  info("return from createTempSaveDir: " + saveDir.path);
  return saveDir;
}

add_task(async function test_image_download() {
  await BrowserTestUtils.withNewTab(
    TEST_PATH + "test_mixed_content_image.html",
    async browser => {
      // Add the image, and wait for it to load.
      await ContentTask.spawn(browser, null, function() {
        let loc = content.document.location.href;
        let httpRoot = loc.replace("https", "http");
        let imgloc = new content.URL("dummy.png", httpRoot);
        let img = content.document.createElement("img");
        img.src = imgloc;
        return new Promise(resolve => {
          img.onload = resolve;
          content.document.body.appendChild(img);
        });
      });
      gTestDir = createTemporarySaveDirectory();

      let destFile = gTestDir.clone();

      MockFilePicker.displayDirectory = gTestDir;
      let fileName;
      MockFilePicker.showCallback = function(fp) {
        info("showCallback");
        fileName = fp.defaultString;
        info("fileName: " + fileName);
        destFile.append(fileName);
        info("path: " + destFile.path);
        MockFilePicker.setFiles([destFile]);
        MockFilePicker.filterIndex = 0; // just save the file
        info("done showCallback");
      };
      let downloadFinishedPromise = new Promise(async resolve => {
        let dls = await Downloads.getList(Downloads.PUBLIC);
        dls.addView({
          onDownloadChanged(download) {
            info("Download changed!");
            if (download.succeeded || download.error) {
              info("Download succeeded or errored");
              dls.removeView(this);
              dls.removeFinished();
              resolve(download);
            }
          },
        });
      });
      // open the context menu.
      let popup = document.getElementById("contentAreaContextMenu");
      let popupShown = BrowserTestUtils.waitForEvent(popup, "popupshown");
      BrowserTestUtils.synthesizeMouseAtCenter(
        "img",
        { type: "contextmenu", button: 2 },
        browser
      );
      await popupShown;
      let popupHidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
      popup.querySelector("#context-saveimage").click();
      popup.hidePopup();
      await popupHidden;
      info("Context menu hidden, waiting for download to finish");
      let imageDownload = await downloadFinishedPromise;
      ok(imageDownload.succeeded, "Image should have downloaded successfully");
    }
  );
});
