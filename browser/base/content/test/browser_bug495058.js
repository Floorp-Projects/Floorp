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
    win.addEventListener("load", function () {
      win.removeEventListener("load", arguments.callee, false);

      win.gBrowser.addEventListener("pageshow", function() {
        win.gBrowser.removeEventListener("pageshow", arguments.callee, false);

        // wait for delayedStartup
        win.setTimeout(function () {
          is(win.gBrowser.currentURI.spec, uri, uri + ": uri loaded in detached tab");
          is(win.document.activeElement, win.gBrowser.selectedBrowser, uri + ": browser is focused");
          is(win.gURLBar.value, "", uri + ": urlbar is empty");
          ok(win.gURLBar.emptyText, uri + ": emptytext is present");
          ok(win.gURLBar.hasAttribute("isempty"), uri + ": emptytext is displayed");

          win.close();
          if (uris.length)
            next();
          else
            executeSoon(finish);
        }, 100);
      }, false);
    }, false);
  }
}
