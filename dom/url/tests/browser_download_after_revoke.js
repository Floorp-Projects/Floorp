async function test() {
  waitForExplicitFinish();
  const target = "http://example.com/browser/dom/url/tests/empty.html";
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
        if (
          Services.prefs.getBoolPref(
            "browser.download.always_ask_before_handling_new_types",
            false
          )
        ) {
          // If the download modal is enabled, wait for it to open and declare the
          // download to have begun when we see it.
          let listener = {
            onOpenWindow(aXULWindow) {
              info("Download modal shown...");
              Services.wm.removeListener(listener);

              let domwindow = aXULWindow.docShell.domWindow;
              function onModalLoad() {
                domwindow.removeEventListener("load", onModalLoad, true);

                is(
                  domwindow.document.location.href,
                  "chrome://mozapps/content/downloads/unknownContentType.xhtml",
                  "Download modal loaded..."
                );

                domwindow.close();
                info("Download modal closed.");
                resolve();
              }

              domwindow.addEventListener("load", onModalLoad, true);
            },
            onCloseWindow(aXULWindow) {},
          };

          Services.wm.addListener(listener);
        } else {
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
        }
      });

      let revoked = SpecialPowers.spawn(
        tab.linkedBrowser,
        [],
        () =>
          new Promise(resolve => {
            info("Creating BlobURL...");
            let blob = new content.Blob(["test"], { type: "text/plain" });
            let url = content.URL.createObjectURL(blob);

            let link = content.document.createElement("a");
            link.href = url;
            link.download = "example.txt";
            content.document.body.appendChild(link);
            info("Clicking HTMLAnchorElement...");
            link.click();

            content.URL.revokeObjectURL(url);
            info("BlobURL revoked.");
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
