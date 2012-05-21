/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test added with bug 460086 to test the behavior of the new API that was added
 * to remove all traces of visiting a site.
 */

////////////////////////////////////////////////////////////////////////////////
//// Constants

let pb = Cc[PRIVATEBROWSING_CONTRACT_ID].
         getService(Ci.nsIPrivateBrowsingService);

////////////////////////////////////////////////////////////////////////////////
//// Utility Functions

/**
 * Creates an nsIURI object for the given file.
 *
 * @param aFile
 *        The nsIFile of the URI to create.
 * @returns an nsIURI representing aFile.
 */
function uri(aFile)
{
  return Cc["@mozilla.org/network/io-service;1"].
         getService(Ci.nsIIOService).
         newFileURI(aFile);
}

/**
 * Checks to ensure a URI string is in download history or not.
 *
 * @param aURIString
 *        The string of the URI to check.
 * @param aIsActive
 *        True if the URI should be actively downloaded, false otherwise.
 */
function check_active_download(aURIString, aIsActive)
{
  let dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
  let enumerator = dm.activeDownloads;
  let found = false;
  while (enumerator.hasMoreElements()) {
    let dl = enumerator.getNext().QueryInterface(Ci.nsIDownload);
    if (dl.source.spec == aURIString)
      found = true;
  }
  let checker = aIsActive ? do_check_true : do_check_false;
  checker(found);
}

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

let destFile = dirSvc.get("TmpD", Ci.nsIFile);
destFile.append("dm-test-file");
destFile = uri(destFile);
let data = [
  { source: "http://mozilla.org/direct_match",
    target: destFile.spec,
    removed: true
  },
  { source: "http://www.mozilla.org/subdomain",
    target: destFile.spec,
    removed: true
  },
  { source: "http://ilovemozilla.org/contains_domain",
    target: destFile.spec,
    removed: false
  },
];

function do_test()
{
  // We add this data to the database first, but we cannot instantiate the
  // download manager service, otherwise these downloads will not be placed in
  // the active downloads array.

  // Copy the empty downloads database to our profile directory
  let downloads = do_get_file("downloads.empty.sqlite");
  downloads.copyTo(dirSvc.get("ProfD", Ci.nsIFile), "downloads.sqlite");

  // Open the database
  let ss = Cc["@mozilla.org/storage/service;1"].
           getService(Ci.mozIStorageService);
  let file = dirSvc.get("ProfD", Ci.nsIFile);
  file.append("downloads.sqlite");
  let db = ss.openDatabase(file);

  // Insert the data
  let stmt = db.createStatement(
    "INSERT INTO moz_downloads (source, target, state, autoResume, entityID) " +
    "VALUES (:source, :target, :state, :autoResume, :entityID)"
  );
  for (let i = 0; i < data.length; i++) {
    stmt.params.source = data[i].source;
    stmt.params.target = data[i].target;
    stmt.params.state = Ci.nsIDownloadManager.DOWNLOAD_PAUSED;
    stmt.params.autoResume = 0; // DONT_RESUME is 0
    stmt.params.entityID = "foo" // just has to be non-null for our test
    stmt.execute();
    stmt.reset();
  }
  stmt.finalize();
  stmt = null;
  db.close();
  db = null;

  // Check to make sure it's all there
  for (let i = 0; i < data.length; i++)
    check_active_download(data[i].source, true);

  // Dispatch the remove call
  pb.removeDataFromDomain("mozilla.org");

  // And check our data
  for (let i = 0; i < data.length; i++)
    check_active_download(data[i].source, !data[i].removed);
}
