function test() {
  waitForExplicitFinish();
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  function onLoad() {
    info("Page loaded.");

    var listener = {
      onOpenWindow(aXULWindow) {
        info("Download window shown...");
        Services.wm.removeListener(listener);

        function downloadOnLoad() {
          domwindow.removeEventListener("load", downloadOnLoad, true);

          is(
            domwindow.document.location.href,
            "chrome://mozapps/content/downloads/unknownContentType.xhtml",
            "Download page appeared"
          );

          domwindow.close();
          gBrowser.removeTab(gBrowser.selectedTab);
          finish();
        }

        var domwindow = aXULWindow.docShell.domWindow;
        domwindow.addEventListener("load", downloadOnLoad, true);
      },
      onCloseWindow(aXULWindow) {},
    };

    Services.wm.addListener(listener);

    info("Creating BlobURL and clicking on a HTMLAnchorElement...");
    SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
      let blob = new content.Blob(["test"], { type: "text/plain" });
      let url = content.URL.createObjectURL(blob);

      let link = content.document.createElement("a");
      link.href = url;
      link.download = "example.txt";
      content.document.body.appendChild(link);
      link.click();

      content.URL.revokeObjectURL(url);
    });
  }

  const target = "http://example.com/browser/dom/url/tests/empty.html";
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, target).then(
    onLoad
  );

  info("Loading download page...");
  BrowserTestUtils.loadURI(gBrowser, target);
}
