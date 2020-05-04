/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.org"
);
const TEST_PATH2 = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

registerCleanupFunction(async function() {
  info("Running the cleanup code");
  MockFilePicker.cleanup();
  Services.obs.removeObserver(checkRequest, "http-on-modify-request");
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

function checkRequest(subject) {
  let httpChannel = subject.QueryInterface(Ci.nsIHttpChannel);
  let spec = httpChannel.URI.spec;
  // Ignore initial requests for page that sets cookies and its favicon, which may not have
  // cookies.
  if (
    httpChannel.URI.host == "example.org" &&
    !spec.endsWith("favicon.ico") &&
    !spec.includes("redirect.sjs")
  ) {
    let cookie = httpChannel.getRequestHeader("cookie");
    is(
      cookie.trim(),
      "normalCookie=true",
      "Should have correct cookie in request for " + spec
    );
  }
}

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

add_task(async function() {
  // Use nsICookieService.BEHAVIOR_REJECT_TRACKER to avoid cookie partitioning.
  // In this test case, if the cookie is partitioned, there will be no cookie
  // nsICookieServicebeing sent to compare.
  await SpecialPowers.pushPrefEnv({
    set: [["network.cookie.cookieBehavior", 4]],
  });

  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    Services.obs.addObserver(checkRequest, "http-on-modify-request");
    BrowserTestUtils.loadURI(
      browser,
      TEST_PATH + "set-samesite-cookies-and-redirect.sjs"
    );
    // Test that the original document load doesn't send same-site cookies.
    await BrowserTestUtils.browserLoaded(
      browser,
      true,
      TEST_PATH2 + "set-samesite-cookies-and-redirect.sjs"
    );
    // Now check the saved page.
    // Create the folder the link will be saved into.
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
      MockFilePicker.filterIndex = 0; // kSaveAsType_Complete
      info("done showCallback");
    };
    saveBrowser(browser);
    await new Promise(async (resolve, reject) => {
      let dls = await Downloads.getList(Downloads.PUBLIC);
      dls.addView({
        onDownloadChanged(download) {
          if (download.succeeded) {
            dls.removeView(this);
            dls.removeFinished();
            resolve();
          } else if (download.error) {
            reject("Download failed");
          }
        },
      });
    });
  });
});
