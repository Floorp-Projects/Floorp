/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var Downloads = ChromeUtils.import("resource://gre/modules/Downloads.jsm", {})
  .Downloads;

var gTestRoot = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://mochi.test:8888/"
);

function getFile(aFilename) {
  if (aFilename.startsWith("file:")) {
    var url = NetUtil.newURI(aFilename).QueryInterface(Ci.nsIFileURL);
    return url.file.clone();
  }

  var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(aFilename);
  return file;
}

function windowObserver(win, topic) {
  if (topic !== "domwindowopened") {
    return;
  }

  win.addEventListener(
    "load",
    function() {
      if (
        win.document.documentURI ===
        "chrome://mozapps/content/downloads/unknownContentType.xhtml"
      ) {
        executeSoon(function() {
          let dialog = win.document.getElementById("unknownContentType");
          let button = dialog.getButton("accept");
          button.disabled = false;
          dialog.acceptDialog();
        });
      }
    },
    { once: true }
  );
}

function test() {
  waitForExplicitFinish();

  Services.ww.registerNotification(windowObserver);

  SpecialPowers.pushPrefEnv(
    {
      set: [
        ["dom.serviceWorkers.enabled", true],
        ["dom.serviceWorkers.exemptFromPerDomainMax", true],
        ["dom.serviceWorkers.testing.enabled", true],
      ],
    },
    function() {
      var url = gTestRoot + "download/window.html";
      var tab = BrowserTestUtils.addTab(gBrowser);
      gBrowser.selectedTab = tab;

      Downloads.getList(Downloads.ALL)
        .then(function(downloadList) {
          var downloadListener;

          function downloadVerifier(aDownload) {
            if (aDownload.succeeded) {
              var file = getFile(aDownload.target.path);
              ok(file.exists(), "download completed");
              is(file.fileSize, 33, "downloaded file has correct size");
              file.remove(false);
              downloadList.remove(aDownload).catch(Cu.reportError);
              downloadList.removeView(downloadListener).catch(Cu.reportError);
              gBrowser.removeTab(tab);
              Services.ww.unregisterNotification(windowObserver);

              executeSoon(finish);
            }
          }

          downloadListener = {
            onDownloadAdded: downloadVerifier,
            onDownloadChanged: downloadVerifier,
          };

          return downloadList.addView(downloadListener);
        })
        .then(function() {
          BrowserTestUtils.loadURI(gBrowser, url);
        });
    }
  );
}
