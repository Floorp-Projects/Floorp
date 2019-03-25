function test() {
  waitForExplicitFinish();

  var w;
  var iteration = 1;
  const uris = ["", "about:blank"];
  var uri;
  var origDoc;

  function testLoad() {
    if (w.document == origDoc) {
      // Go back to polling
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      setTimeout(testLoad, 10);
      return;
    }
    var prin = w.document.nodePrincipal;
    isnot(prin, null, "Loaded principal must not be null when adding " + uri);
    isnot(prin, undefined, "Loaded principal must not be undefined when loading " + uri);
    is(prin.isSystemPrincipal, false,
       "Loaded principal must not be system when loading " + uri);
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
    w = window.open(uri, "_blank", "width=10,height=10");
    var prin = w.document.nodePrincipal;
    if (!uri) {
      uri = undefined;
    }
    isnot(prin, null, "Forced principal must not be null when loading " + uri);
    isnot(prin, undefined,
          "Forced principal must not be undefined when loading " + uri);
    is(prin.isSystemPrincipal, false,
       "Forced principal must not be system when loading " + uri);
    if (uri == undefined) {
      // No actual load here, so just move along.
      w.close();
      ++iteration;
      doTest();
    } else {
      origDoc = w.document;
      // Need to poll, because load listeners on the content window won't
      // survive the load.
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      setTimeout(testLoad, 10);
    }
  }

  doTest();
}
