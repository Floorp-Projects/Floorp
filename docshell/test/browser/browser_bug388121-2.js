function test() {
  waitForExplicitFinish();

  var w;
  var iteration = 1;
  const uris = ["", "about:blank"];
  var uri;
  var origWgp;

  function testLoad() {
    let wgp = w.gBrowser.selectedBrowser.browsingContext.currentWindowGlobal;
    if (wgp == origWgp) {
      // Go back to polling
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      setTimeout(testLoad, 10);
      return;
    }
    var prin = wgp.documentPrincipal;
    isnot(prin, null, "Loaded principal must not be null when adding " + uri);
    isnot(
      prin,
      undefined,
      "Loaded principal must not be undefined when loading " + uri
    );
    is(
      prin.isSystemPrincipal,
      false,
      "Loaded principal must not be system when loading " + uri
    );
    w.close();

    if (iteration == uris.length) {
      finish();
    } else {
      ++iteration;
      doTest();
    }
  }

  function doTest() {
    uri = uris[iteration - 1];
    window.open(uri, "_blank", "width=10,height=10,noopener");
    w = Services.wm.getMostRecentWindow("navigator:browser");
    origWgp = w.gBrowser.selectedBrowser.browsingContext.currentWindowGlobal;
    var prin = origWgp.documentPrincipal;
    if (!uri) {
      uri = undefined;
    }
    isnot(prin, null, "Forced principal must not be null when loading " + uri);
    isnot(
      prin,
      undefined,
      "Forced principal must not be undefined when loading " + uri
    );
    is(
      prin.isSystemPrincipal,
      false,
      "Forced principal must not be system when loading " + uri
    );
    if (uri == undefined) {
      // No actual load here, so just move along.
      w.close();
      ++iteration;
      doTest();
    } else {
      // Need to poll, because load listeners on the content window won't
      // survive the load.
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      setTimeout(testLoad, 10);
    }
  }

  doTest();
}
