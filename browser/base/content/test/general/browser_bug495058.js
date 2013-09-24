function test() {
  waitForExplicitFinish();
  next();
}

var uris = [
  "about:blank",
  "about:sessionrestore",
  "about:privatebrowsing",
];

function next() {
  var tab = gBrowser.addTab();
  var uri = uris.shift();

  if (uri == "about:blank") {
    detach();
  } else {
    let browser = tab.linkedBrowser;
    browser.addEventListener("load", function () {
      browser.removeEventListener("load", arguments.callee, true);
      detach();
    }, true);
    browser.loadURI(uri);
  }

  function detach() {
    var win = gBrowser.replaceTabWithWindow(tab);

    whenDelayedStartupFinished(win, function () {
      is(win.gBrowser.currentURI.spec, uri, uri + ": uri loaded in detached tab");
      is(win.document.activeElement, win.gBrowser.selectedBrowser, uri + ": browser is focused");
      is(win.gURLBar.value, "", uri + ": urlbar is empty");
      ok(win.gURLBar.placeholder, uri + ": placeholder text is present");

      win.close();
      if (uris.length)
        next();
      else
        executeSoon(finish);
    });
  }
}
