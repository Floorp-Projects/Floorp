add_task(async function () {
  let url =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content/",
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      "http://example.com/"
    ) + "file_bug1206879.html";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url, true);

  let numLocationChanges = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async function () {
      let webprogress = content.docShell.QueryInterface(Ci.nsIWebProgress);
      let locationChangeCount = 0;
      let listener = {
        onLocationChange(aWebProgress, aRequest, aLocation) {
          info("onLocationChange: " + aLocation.spec);
          locationChangeCount++;
          this.resolve();
        },
        QueryInterface: ChromeUtils.generateQI([
          "nsIWebProgressListener",
          "nsISupportsWeakReference",
        ]),
      };
      let locationPromise = new Promise(resolve => {
        listener.resolve = resolve;
      });
      webprogress.addProgressListener(
        listener,
        Ci.nsIWebProgress.NOTIFY_LOCATION
      );

      content.frames[0].history.pushState(null, null, "foo");

      await locationPromise;
      webprogress.removeProgressListener(listener);

      return locationChangeCount;
    }
  );

  gBrowser.removeTab(tab);
  is(
    numLocationChanges,
    1,
    "pushState with a different URI should cause a LocationChange event."
  );
});
