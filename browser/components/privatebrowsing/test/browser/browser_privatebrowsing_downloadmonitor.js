/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Private Browsing Tests.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  let iosvc = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
  let panel = document.getElementById("download-monitor");
  waitForExplicitFinish();

  // Add a new download
  addDownload(dm, {
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
  pb.privateBrowsingEnabled = true;

  setTimeout(function () {
    ok(panel.hidden, "The download panel should be hidden when entering the private browsing mode");

    // Add a new download
    let file = addDownload(dm, {
      resultFileName: "pbtest-2",
      downloadName: "PB Test 2"
    }).targetFile;

    // Update the panel
    DownloadMonitorPanel.updateStatus();

    // Make sure that the panel is visible
    ok(!panel.hidden, "The download panel should show up when a new download is added");

    // Exit the private browsing mode
    pb.privateBrowsingEnabled = false;

    setTimeout(function () {
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

      finish();
    }, 0);
  }, 0);
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
    aParams.sourceURI = "http://localhost:8888/browser/browser/components/privatebrowsing/test/browser/staller.sjs";
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

  return dl;
}

function createURI(aObj)
{
  let ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return (aObj instanceof Ci.nsIFile) ? ios.newFileURI(aObj) :
                                        ios.newURI(aObj, null, null);
}
