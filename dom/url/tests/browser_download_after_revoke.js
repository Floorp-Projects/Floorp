function test () {
  waitForExplicitFinish();
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  function onLoad() {
    info("Page loaded.");

    var listener = {
      onOpenWindow: function(aXULWindow) {
        info("Download window shown...");
        Services.wm.removeListener(listener);

        function downloadOnLoad() {
          domwindow.removeEventListener("load", downloadOnLoad, true);

	  is(domwindow.document.location.href, "chrome://mozapps/content/downloads/unknownContentType.xul", "Download page appeared");

	  domwindow.close();
          gBrowser.removeTab(gBrowser.selectedTab);
	  finish();
        }

        var domwindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIDOMWindow);
        domwindow.addEventListener("load", downloadOnLoad, true);
      },
      onCloseWindow: function(aXULWindow) {},
    }

    Services.wm.addListener(listener);

    info("Creating BlobURL and clicking on a HTMLAnchorElement...");
    ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
      let blob = new content.Blob(['test'], { type: 'text/plain' });
      let url = content.URL.createObjectURL(blob);

      let link = content.document.createElement('a');
      link.href = url;
      link.download = 'example.txt';
      content.document.body.appendChild(link);
      link.click();

      content.URL.revokeObjectURL(url);
    });
  }

  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(onLoad);

  info("Loading download page...");
  gBrowser.loadURI("http://example.com/browser/dom/url/tests/empty.html");
}
