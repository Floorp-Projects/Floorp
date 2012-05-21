/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the download monitor status bar panel is correctly
// cleared when switching the private browsing mode on or off.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  let dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
  if (!gDownloadMgr)
    gDownloadMgr = dm;
  let panel = document.getElementById("download-monitor");
  waitForExplicitFinish();

  let acceptDialog = 0;
  let confirmCalls = 0;
  function promptObserver(aSubject, aTopic, aData) {
    let dialogWin = aSubject.QueryInterface(Ci.nsIDOMWindow);
    confirmCalls++;
    if (acceptDialog-- > 0)
      dialogWin.document.documentElement.getButton("accept").click();
  }

  Services.obs.addObserver(promptObserver, "common-dialog-loaded", false);

  // Add a new download
  let [file, persist] = addDownload(dm, {
    resultFileName: "pbtest-1",
    downloadName: "PB Test 1"
  });

  // Make sure that the download is being displayed in the monitor panel
  if (!DownloadMonitorPanel.inited())
    DownloadMonitorPanel.init();
  else
    DownloadMonitorPanel.updateStatus();
  ok(!panel.hidden, "The download panel should be successfully added initially");

  // Enter the private browsing mode
  acceptDialog = 1;
  pb.privateBrowsingEnabled = true;
  is(confirmCalls, 1, "One prompt was accepted");
  ok(pb.privateBrowsingEnabled, "The private browsing transition was successful");

  executeSoon(function () {
    ok(panel.hidden, "The download panel should be hidden when entering the private browsing mode");

    // Add a new download
    let [file2, persist2] = addDownload(dm, {
      resultFileName: "pbtest-2",
      downloadName: "PB Test 2"
    });

    // Update the panel
    DownloadMonitorPanel.updateStatus();

    // Make sure that the panel is visible
    ok(!panel.hidden, "The download panel should show up when a new download is added");

    // Exit the private browsing mode
    acceptDialog = 1;
    pb.privateBrowsingEnabled = false;
    is(confirmCalls, 2, "One prompt was accepted");
    ok(!pb.privateBrowsingEnabled, "The private browsing transition was successful");

    executeSoon(function () {
      ok(panel.hidden, "The download panel should be hidden when leaving the private browsing mode");

      // cleanup
      let dls = dm.activeDownloads;
      while (dls.hasMoreElements()) {
        let dl = dls.getNext().QueryInterface(Ci.nsIDownload);
        dm.removeDownload(dl.id);
        let file = dl.targetFile;
        if (file.exists())
          file.remove(false);
      }
      if (file.exists())
        file.remove(false);

      Services.obs.removeObserver(promptObserver, "common-dialog-loaded", false);
      finish();
    });
  });
}

/**
 * Adds a download to the DM, and starts it.
 * (Copied from toolkit/componentns/downloads/test/unit/head_download_manager.js)
 * @param aParams (optional): an optional object which contains the function
 *                            parameters:
 *                              resultFileName: leaf node for the target file
 *                              targetFile: nsIFile for the target (overrides resultFileName)
 *                              sourceURI: the download source URI
 *                              downloadName: the display name of the download
 *                              runBeforeStart: a function to run before starting the download
 */
function addDownload(dm, aParams)
{
  if (!aParams)
    aParams = {};
  if (!("resultFileName" in aParams))
    aParams.resultFileName = "download.result";
  if (!("targetFile" in aParams)) {
    let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                 getService(Ci.nsIProperties);
    aParams.targetFile = dirSvc.get("ProfD", Ci.nsIFile);
    aParams.targetFile.append(aParams.resultFileName);
  }
  if (!("sourceURI" in aParams))
    aParams.sourceURI = "http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/staller.sjs";
  if (!("downloadName" in aParams))
    aParams.downloadName = null;
  if (!("runBeforeStart" in aParams))
    aParams.runBeforeStart = function () {};

  const nsIWBP = Ci.nsIWebBrowserPersist;
  let persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                .createInstance(Ci.nsIWebBrowserPersist);
  persist.persistFlags = nsIWBP.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                         nsIWBP.PERSIST_FLAGS_BYPASS_CACHE |
                         nsIWBP.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

  let dl = dm.addDownload(Ci.nsIDownloadManager.DOWNLOAD_TYPE_DOWNLOAD,
                          createURI(aParams.sourceURI),
                          createURI(aParams.targetFile), aParams.downloadName, null,
                          Math.round(Date.now() * 1000), null, persist);

  // This will throw if it isn't found, and that would mean test failure, so no
  // try catch block
  let test = dm.getDownload(dl.id);

  aParams.runBeforeStart.call(undefined, dl);

  persist.progressListener = dl.QueryInterface(Ci.nsIWebProgressListener);
  persist.saveURI(dl.source, null, null, null, null, dl.targetFile);

  return [dl.targetFile, persist];
}

function createURI(aObj) {
  let ios = Services.io;
  return (aObj instanceof Ci.nsIFile) ? ios.newFileURI(aObj) :
                                        ios.newURI(aObj, null, null);
}
