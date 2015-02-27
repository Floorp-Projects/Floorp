/**
 * Test the adoptDownload API.  Specifically, we expect that when we call
 * adoptDownload with a valid payload that:
 * - The method will be resolved with a valid, fully populated DOMDownload
 *   instance, including an id.
 * - An ondownloadstart notification will be generated and the DOMDownload
 *   instance it receives will be logically equivalent.
 *
 * We also explicitly verify that invalid adoptDownload payloads result in a
 * rejection and that no download is added.
 *
 * This test explicitly does not test that the download is correctly persisted
 * to the database.  This is done because Downloads.jsm does not provide a means
 * of safely restarting itself, so Firefox would need to be restarted.  Because
 * the adoptDownload code is using the Downloads API in a straightforward
 * manner, it's not considered likely this would regress, and certainly not
 * considered worth the automated testing overhead of a restart.
 */

function checkInvalidResult(dict, expectedErr, explanation) {
  navigator.mozDownloadManager.ondownloadstart = function() {
    ok(false, "No download should have been added!");
  };
  navigator.mozDownloadManager.adoptDownload(dict).then(
    function() {
      ok(false, "Invalid adoptDownload did not reject!");
      runTests();
    },
    function(rejectedWith) {
      is(rejectedWith, expectedErr, explanation + " rejection value");
      runTests();
    });
}

// Pick a date that Date.now() could not possibly return by picking a date in
// the past.  (We want to make sure the date we provide works.)
var arbitraryDate = new Date(Date.now() - 60000);

var blobContents = new Uint8Array(256);
var memBlob = new Blob([blobContents], { type: 'application/octet-stream' });
var blobStorageName;
var blobStoragePath = 'blobby.blob';

function checkAdoptedDownload(download, validPayload) {
  is(download.totalBytes, memBlob.size, 'size');
  is(download.url, validPayload.url, 'url');
  // The filesystem path is not practical to check since we can't hard-code it
  // and the only way to check is to effectively duplicate the logic in
  // DownloadsAPI.js.  The good news, however, is that the value is
  // round-tripped from storageName/storagePath to path and back again, and we
  // also verify the file exists on disk, so we can be reasonably confident this
  // is correct.  We output it to aid in debugging if things should break,
  // of course.
  info('path (not checked): ' + download.path);
  is(download.storageName, validPayload.storageName, 'storageName');
  is(download.storagePath, validPayload.storagePath, 'storagePath');
  is(download.state, 'succeeded', 'state');
  is(download.contentType, validPayload.contentType, 'contentType');
  is(download.startTime.valueOf(), arbitraryDate.valueOf(), 'startTime');
  is(download.sourceAppManifestURL,
     'http://mochi.test:8888/' +
       'tests/dom/downloads/tests/testapp_downloads_adopt_download.manifest',
    'app manifest');
};

var tests = [
  function saveBlobToDeviceStorage() {
    // Only sdcard can handle arbitrary MIME types and is guaranteed to be a
    // thing.
    var storage = navigator.getDeviceStorage('sdcard');
    // We used the non-array helper, so the name we get may be different than
    // what we asked for.
    blobStorageName = storage.storageName;
    ok(!!storage, 'have storage');
    var req = storage.addNamed(memBlob, blobStoragePath);
    req.onerror = function() {
      ok(false, 'problem saving blob to storage: ' + req.error.name);
    };
    req.onsuccess = function(evt) {
      ok(true, 'saved blob: ' + evt.target.result);
      runTests();
    };
  },
  function addValid() {
      var validPayload = {
        // All currently expected consumers are unable to provide a valid URL, and
        // as a result need to provide an empty string.
        url: "",
        storageName: blobStorageName,
        storagePath: blobStoragePath,
        contentType: memBlob.type,
        startTime: arbitraryDate
      };
    // Wrap the notification in a check so we can force our logic to be
    // consistently ordered in the test even if it's not in reality.
    var notifiedPromise = new Promise(function(resolve, reject) {
      navigator.mozDownloadManager.ondownloadstart = function(evt) {
        resolve(evt.download);
      };
    });

    // Start the download
    navigator.mozDownloadManager.adoptDownload(validPayload).then(
      function(apiDownload) {
        checkAdoptedDownload(apiDownload, validPayload);
        ok(!!apiDownload.id, "Need a download id!");
        notifiedPromise.then(function(notifiedDownload) {
          checkAdoptedDownload(notifiedDownload, validPayload);
          is(apiDownload.id, notifiedDownload.id,
             "Notification should be for the download we adopted");
          runTests();
        });
      },
      function() {
        ok(false, "adoptDownload should not have rejected");
        runTests();
      });
  },

  function dictionaryNotProvided() {
    checkInvalidResult(undefined, "InvalidDownload");
  },
  // Missing fields immediately result in rejection with InvalidDownload
  function missingStorageName() {
    checkInvalidResult({
      url: "",
      // no storageName
      storagePath: "relpath/filename.txt",
      contentType: "text/plain",
      startTime: arbitraryDate
    }, "InvalidDownload", "missing storage name");
  },
  function nullStorageName() {
    checkInvalidResult({
      url: "",
      storageName: null,
      storagePath: "relpath/filename.txt",
      contentType: "text/plain",
      startTime: arbitraryDate
    }, "InvalidDownload", "null storage name");
  },
  function missingStoragePath() {
    checkInvalidResult({
      url: "",
      storageName: blobStorageName,
      // no storagePath
      contentType: "text/plain",
      startTime: arbitraryDate
    }, "InvalidDownload", "missing storage path");
  },
  function nullStoragePath() {
    checkInvalidResult({
      url: "",
      storageName: blobStorageName,
      storagePath: null,
      contentType: "text/plain",
      startTime: arbitraryDate
    }, "InvalidDownload", "null storage path");
  },
  function missingContentType() {
    checkInvalidResult({
      url: "",
      storageName: "sdcard",
      storagePath: "relpath/filename.txt",
      // no contentType
      startTime: arbitraryDate
    }, "InvalidDownload", "missing content type");
  },
  function nullContentType() {
    checkInvalidResult({
      url: "",
      storageName: "sdcard",
      storagePath: "relpath/filename.txt",
      contentType: null,
      startTime: arbitraryDate
    }, "InvalidDownload", "null content type");
  },
  // Incorrect storage names are likewise immediately invalidated
  function invalidStorageName() {
    checkInvalidResult({
      url: "",
      storageName: "ALMOST CERTAINLY DOES NOT EXIST",
      storagePath: "relpath/filename.txt",
      contentType: "text/plain",
      startTime: arbitraryDate
    }, "InvalidDownload", "invalid storage name");
  },
  // The existence of the file is validated in the parent process
  function legitStorageInvalidPath() {
    checkInvalidResult({
      url: "",
      storageName: blobStorageName,
      storagePath: "ALMOST CERTAINLY DOES NOT EXIST",
      contentType: "text/plain",
      startTime: arbitraryDate
    }, "AdoptNoSuchFile", "invalid path");
  },
  function allDone() {
    // Just in case, make sure no other mochitest could mess with us after we've
    // finished.
    navigator.mozDownloadManager.ondownloadstart = null;
    runTests();
  }
];

function runTests() {
  if (!tests.length) {
    finish();
    return;
  }

  var test = tests.shift();
  if (test.name) {
    info('starting test: ' + test.name);
  }
  test();
}
runTests();
