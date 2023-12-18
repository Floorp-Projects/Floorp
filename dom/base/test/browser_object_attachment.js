ChromeUtils.defineESModuleGetters(this, {
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
});

async function waitForDownload() {
  // Get the downloads list and add a view to listen for a download to be added.
  let downloadList = await Downloads.getList(Downloads.ALL);

  // Wait for a single download
  let downloadView;
  let finishedAllDownloads = new Promise(resolve => {
    downloadView = {
      onDownloadAdded(aDownload) {
        info("download added");
        resolve(aDownload);
      },
    };
  });
  await downloadList.addView(downloadView);
  let download = await finishedAllDownloads;
  await downloadList.removeView(downloadView);

  // Clean up the download from the list.
  await downloadList.remove(download);
  await download.finalize(true);

  // Return the download
  return download;
}

const httpsTestRoot = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function test_pdf_object_attachment() {
  // Set the behaviour to save pdfs to disk and not handle internally, so we
  // don't end up with extra tabs after the test.
  var gMimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  var gHandlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
    Ci.nsIHandlerService
  );
  const mimeInfo = gMimeSvc.getFromTypeAndExtension("application/pdf", "pdf");
  let previousAction = mimeInfo.preferredAction;
  mimeInfo.preferredAction = Ci.nsIHandlerInfo.saveToDisk;
  gHandlerSvc.store(mimeInfo);
  registerCleanupFunction(() => {
    mimeInfo.preferredAction = previousAction;
    gHandlerSvc.store(mimeInfo);
  });

  // Start listening for the download before opening the new tab.
  let downloadPromise = waitForDownload();
  await BrowserTestUtils.withNewTab(
    `${httpsTestRoot}/file_pdf_object_attachment.html`,
    async browser => {
      let download = await downloadPromise;
      is(
        download.source.url,
        `${httpsTestRoot}/file_pdf_attachment.pdf`,
        "download should be the pdf"
      );

      await SpecialPowers.spawn(browser, [], async () => {
        let obj = content.document.querySelector("object");
        is(
          obj.displayedType,
          Ci.nsIObjectLoadingContent.TYPE_NULL,
          "should be displaying TYPE_NULL (fallback)"
        );
      });
    }
  );
});

add_task(async function test_img_object_attachment() {
  // NOTE: This is testing our current behaviour here as of bug 1868001 (which
  // is to download an image with `Content-Disposition: attachment` embedded
  // within an object or embed element).
  //
  // Other browsers ignore the `Content-Disposition: attachment` header when
  // loading images within object or embed element as-of december 2023, as
  // we did prior to the changes in bug 1595491.
  //
  // If this turns out to be a web-compat issue, we may want to introduce
  // special handling to ignore content-disposition when loading images within
  // an object or embed element.

  // Start listening for the download before opening the new tab.
  let downloadPromise = waitForDownload();
  await BrowserTestUtils.withNewTab(
    `${httpsTestRoot}/file_img_object_attachment.html`,
    async browser => {
      let download = await downloadPromise;
      is(
        download.source.url,
        `${httpsTestRoot}/file_img_attachment.jpg`,
        "download should be the jpg"
      );

      await SpecialPowers.spawn(browser, [], async () => {
        let obj = content.document.querySelector("object");
        is(
          obj.displayedType,
          Ci.nsIObjectLoadingContent.TYPE_NULL,
          "should be displaying TYPE_NULL (fallback)"
        );
      });
    }
  );
});
