/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.org"
);

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

registerCleanupFunction(async function () {
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

function expectedImageAcceptHeader() {
  if (Services.prefs.prefHasUserValue("image.http.accept")) {
    return Services.prefs.getCharPref("image.http.accept");
  }

  return (
    (Services.prefs.getBoolPref("image.avif.enabled") ? "image/avif," : "") +
    (Services.prefs.getBoolPref("image.jxl.enabled") ? "image/jxl," : "") +
    "image/webp,*/*"
  );
}

add_task(async function test_image_download() {
  await BrowserTestUtils.withNewTab(TEST_PATH + "dummy.html", async browser => {
    // Add the image, and wait for it to load.
    await SpecialPowers.spawn(browser, [], async function () {
      let loc = content.document.location.href;
      let imgloc = new content.URL("dummy.png", loc);
      let img = content.document.createElement("img");
      img.src = imgloc;
      await new Promise(resolve => {
        img.onload = resolve;
        content.document.body.appendChild(img);
      });
    });
    gTestDir = createTemporarySaveDirectory();

    let destFile = gTestDir.clone();

    MockFilePicker.displayDirectory = gTestDir;
    let fileName;
    MockFilePicker.showCallback = function (fp) {
      info("showCallback");
      fileName = fp.defaultString;
      info("fileName: " + fileName);
      destFile.append(fileName);
      info("path: " + destFile.path);
      MockFilePicker.setFiles([destFile]);
      MockFilePicker.filterIndex = 0; // just save the file
      info("done showCallback");
    };
    let publicDownloads = await Downloads.getList(Downloads.PUBLIC);
    let downloadFinishedPromise = new Promise(resolve => {
      publicDownloads.addView({
        onDownloadChanged(download) {
          info("Download changed!");
          if (download.succeeded || download.error) {
            info("Download succeeded or errored");
            publicDownloads.removeView(this);
            publicDownloads.removeFinished();
            resolve(download);
          }
        },
      });
    });
    let httpOnModifyPromise = TestUtils.topicObserved(
      "http-on-modify-request",
      (s, t, d) => {
        let channel = s.QueryInterface(Ci.nsIChannel);
        let uri = channel.URI && channel.URI.spec;
        if (!uri.endsWith("dummy.png")) {
          info("Ignoring request for " + uri);
          return false;
        }
        ok(channel instanceof Ci.nsIHttpChannel, "Should be HTTP channel");
        channel.QueryInterface(Ci.nsIHttpChannel);
        is(
          channel.getRequestHeader("Accept"),
          expectedImageAcceptHeader(),
          "Header should be image header"
        );
        return true;
      }
    );
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
    popup.activateItem(popup.querySelector("#context-saveimage"));
    await popupHidden;
    info("Context menu hidden, waiting for download to finish");
    let imageDownload = await downloadFinishedPromise;
    ok(imageDownload.succeeded, "Image should have downloaded successfully");
    info("Waiting for http request to complete.");
    // Ensure we got the http request:
    await httpOnModifyPromise;
  });
});
