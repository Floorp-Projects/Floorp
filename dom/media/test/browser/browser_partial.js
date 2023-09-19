async function test() {
  waitForExplicitFinish();
  const target =
    "https://example.com/browser/dom/media/test/browser/file_empty_page.html";

  info("Loading download page...");

  let tab = BrowserTestUtils.addTab(gBrowser, target);

  registerCleanupFunction(function () {
    gBrowser.removeTab(tab);
    window.restore();
  });

  gBrowser.selectedTab = tab;
  BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, target).then(
    async () => {
      info("Page loaded.");
      let allDownloads = await Downloads.getList(Downloads.ALL);
      let started = new Promise(resolve => {
        // With no download modal, the download will begin on its own, so we need
        // to wait to be notified by the downloads list when that happens.
        let downloadView = {
          onDownloadAdded(download) {
            ok(true, "Download was started.");
            download.cancel();
            allDownloads.removeView(this);
            allDownloads.removeFinished();
            resolve();
          },
        };
        allDownloads.addView(downloadView);
      });

      let revoked = SpecialPowers.spawn(
        tab.linkedBrowser,
        [],
        () =>
          new Promise(resolve => {
            let link = content.document.createElement("a");
            link.href = "force_octet_stream.mp4";
            content.document.body.appendChild(link);
            info("Clicking HTMLAnchorElement...");
            link.click();
            info("Clicked HTMLAnchorElement.");
            resolve();
          })
      );

      info("Waiting for async activities...");
      await Promise.all([revoked, started]);
      ok(true, "Exiting test.");
      finish();
    }
  );
}
