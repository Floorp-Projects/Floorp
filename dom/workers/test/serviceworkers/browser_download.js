/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import('resource://gre/modules/Services.jsm');
var Downloads = Cu.import("resource://gre/modules/Downloads.jsm", {}).Downloads;
var DownloadsCommon = Cu.import("resource:///modules/DownloadsCommon.jsm", {}).DownloadsCommon;
Cu.import('resource://gre/modules/NetUtil.jsm');

var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/",
                                                    "http://mochi.test:8888/")

function getFile(aFilename) {
  if (aFilename.startsWith('file:')) {
    var url = NetUtil.newURI(aFilename).QueryInterface(Ci.nsIFileURL);
    return url.file.clone();
  }

  var file = Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);
  file.initWithPath(aFilename);
  return file;
}

function windowObserver(win, topic) {
  if (topic !== 'domwindowopened') {
    return;
  }

  win.addEventListener('load', function onLoadWindow() {
    win.removeEventListener('load', onLoadWindow, false);
    if (win.document.documentURI ===
        'chrome://mozapps/content/downloads/unknownContentType.xul') {
      executeSoon(function() {
        var button = win.document.documentElement.getButton('accept');
        button.disabled = false;
        win.document.documentElement.acceptDialog();
      });
    }
  }, false);
}

function test() {
  waitForExplicitFinish();

  Services.ww.registerNotification(windowObserver);

  SpecialPowers.pushPrefEnv({'set': [['dom.serviceWorkers.enabled', true],
                                     ['dom.serviceWorkers.exemptFromPerDomainMax', true],
                                     ['dom.serviceWorkers.testing.enabled', true]]},
                            function() {
    var url = gTestRoot + 'download/window.html';
    var tab = gBrowser.addTab();
    gBrowser.selectedTab = tab;

    Downloads.getList(Downloads.ALL).then(function(downloadList) {
      var downloadListener;

      function downloadVerifier(aDownload) {
        if (aDownload.succeeded) {
          var file = getFile(aDownload.target.path);
          ok(file.exists(), 'download completed');
          is(file.fileSize, 33, 'downloaded file has correct size');
          file.remove(false);
          DownloadsCommon.removeAndFinalizeDownload(aDownload);

          downloadList.removeView(downloadListener);
          gBrowser.removeTab(tab);
          Services.ww.unregisterNotification(windowObserver);

          executeSoon(finish);
        }
      }

      downloadListener = {
        onDownloadAdded: downloadVerifier,
        onDownloadChanged: downloadVerifier
      };

      return downloadList.addView(downloadListener);
    }).then(function() {
      gBrowser.loadURI(url);
    });
  });
}
