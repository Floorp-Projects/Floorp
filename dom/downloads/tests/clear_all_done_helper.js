/**
 * A helper to clear out the existing downloads known to the mozDownloadManager
 * / downloads.js.
 *
 * It exists because previously mozDownloadManager.clearAllDone() thought that
 * when it returned that all the completed downloads would be cleared out.  It
 * was wrong and this led to various intermittent test failurse.  In discussion
 * on https://bugzil.la/979446#c13 and onwards, it was decided that
 * clearAllDone() was in the wrong and that the jsdownloads API it depends on
 * was not going to change to make it be in the right.
 *
 * The existing uses of clearAllDone() in tests seemed to be about:
 * - Exploding if there was somehow still a download in progress
 * - Clearing out the download list at the start of a test so that calls to
 *   getDownloads() wouldn't have to worry about existing downloads, etc.
 *
 * From discussion, the right way to handle clearing is to wait for the expected
 * removal events to occur for the existing downloads.  So that's what we do.
 * We still generate a test failure if there are any in-progress downloads.
 *
 * @param {Boolean} [getDownloads=false]
 *   If true, invoke getDownloads after clearing the download list and return
 *   its value.
 */
function clearAllDoneHelper(getDownloads) {
  var clearedPromise = new Promise(function(resolve, reject) {
    function gotDownloads(downloads) {
      // If there are no downloads, we're already done.
      if (downloads.length === 0) {
        resolve();
        return;
      }

      // Track the set of expected downloads that will be finalized.
      var expectedIds = new Set();
      function changeHandler(evt) {
        var download = evt.download;
        if (download.state === "finalized") {
          expectedIds.delete(download.id);
          if (expectedIds.size === 0) {
            resolve();
          }
        }
      }
      downloads.forEach(function(download) {
        if (download.state === "downloading") {
          ok(false, "A download is still active: " + download.path);
          reject("Active download");
        }
        download.onstatechange = changeHandler;
        expectedIds.add(download.id);
      });
      navigator.mozDownloadManager.clearAllDone();
    }
    function gotBadNews(err) {
      ok(false, "Problem clearing all downloads: " + err);
      reject(err);
    }
    navigator.mozDownloadManager.getDownloads().then(gotDownloads, gotBadNews);
 });
 if (!getDownloads) {
   return clearedPromise;
 }
 return clearedPromise.then(function() {
   return navigator.mozDownloadManager.getDownloads();
 });
}
