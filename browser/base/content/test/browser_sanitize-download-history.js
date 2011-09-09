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
 * The Original Code is mozilla.org Test Code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


function test()
{
  //////////////////////////////////////////////////////////////////////////////
  //// Tests (defined locally for scope's sake)

  function test_checkedAndDisabledAtStart(aWin)
  {
    let doc = aWin.document;
    let downloads = doc.getElementById("downloads-checkbox");
    let history = doc.getElementById("history-checkbox");

    ok(history.checked, "history checkbox is checked");
    ok(downloads.disabled, "downloads checkbox is disabled");
    ok(downloads.checked, "downloads checkbox is checked");
  }

  function test_checkedAndDisabledOnHistoryToggle(aWin)
  {
    let doc = aWin.document;
    let downloads = doc.getElementById("downloads-checkbox");
    let history = doc.getElementById("history-checkbox");

    EventUtils.synthesizeMouse(history, 0, 0, {}, aWin);
    ok(!history.checked, "history checkbox is not checked");
    ok(downloads.disabled, "downloads checkbox is disabled");
    ok(downloads.checked, "downloads checkbox is checked");
  }

  function test_checkedAfterAddingDownload(aWin)
  {
    let doc = aWin.document;
    let downloads = doc.getElementById("downloads-checkbox");
    let history = doc.getElementById("history-checkbox");

    // Add download to DB
    let file = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties).get("TmpD", Ci.nsIFile);
    file.append("sanitize-dm-test.file");
    file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);
    let testPath = Services.io.newFileURI(file).spec;
    let data = {
      name: "381603.patch",
      source: "https://bugzilla.mozilla.org/attachment.cgi?id=266520",
      target: testPath,
      startTime: 1180493839859230,
      endTime: 1180493839859239,
      state: Ci.nsIDownloadManager.DOWNLOAD_FINISHED,
      currBytes: 0, maxBytes: -1, preferredAction: 0, autoResume: 0
    };
    let db = Cc["@mozilla.org/download-manager;1"].
             getService(Ci.nsIDownloadManager).DBConnection;
    let stmt = db.createStatement(
      "INSERT INTO moz_downloads (name, source, target, startTime, endTime, " +
        "state, currBytes, maxBytes, preferredAction, autoResume) " +
      "VALUES (:name, :source, :target, :startTime, :endTime, :state, " +
        ":currBytes, :maxBytes, :preferredAction, :autoResume)");
    try {
      for (let prop in data)
        stmt.params[prop] = data[prop];
      stmt.execute();
    }
    finally {
      stmt.finalize();
    }

    // Toggle history to get everything to update
    EventUtils.synthesizeMouse(history, 0, 0, {}, aWin);
    EventUtils.synthesizeMouse(history, 0, 0, {}, aWin);

    ok(!history.checked, "history checkbox is not checked");
    ok(!downloads.disabled, "downloads checkbox is not disabled");
    ok(downloads.checked, "downloads checkbox is checked");
  }

  function test_checkedAndDisabledWithHistoryChecked(aWin)
  {
    let doc = aWin.document;
    let downloads = doc.getElementById("downloads-checkbox");
    let history = doc.getElementById("history-checkbox");

    EventUtils.synthesizeMouse(history, 0, 0, {}, aWin);
    ok(history.checked, "history checkbox is checked");
    ok(downloads.disabled, "downloads checkbox is disabled");
    ok(downloads.checked, "downloads checkbox is checked");
  }

  let tests = [
    test_checkedAndDisabledAtStart,
    test_checkedAndDisabledOnHistoryToggle,
    test_checkedAfterAddingDownload,
    test_checkedAndDisabledWithHistoryChecked,
  ];

  //////////////////////////////////////////////////////////////////////////////
  //// Run the tests

  let dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
  let db = dm.DBConnection;

  // Empty any old downloads
  db.executeSimpleSQL("DELETE FROM moz_downloads");

  // Close the UI if necessary
  let win = Services.ww.getWindowByName("Sanatize", null);
  if (win && (win instanceof Ci.nsIDOMWindow))
    win.close();

  // Start the test when the sanitize window loads
  Services.ww.registerNotification(function (aSubject, aTopic, aData) {
    Services.ww.unregisterNotification(arguments.callee);
    aSubject.QueryInterface(Ci.nsIDOMEventTarget)
            .addEventListener("DOMContentLoaded", doTest, false);
  });

  // Let the methods that run onload finish before we test
  let doTest = function() setTimeout(function() {
    let win = Services.ww.getWindowByName("Sanitize", null)
                .QueryInterface(Ci.nsIDOMWindow);

    for (let i = 0; i < tests.length; i++)
      tests[i](win);

    win.close();
    finish();
  }, 0);
 
  // Show the UI
  Services.ww.openWindow(window,
                         "chrome://browser/content/sanitize.xul",
                         "Sanitize",
                         "chrome,titlebar,centerscreen",
                         null);

  waitForExplicitFinish();
}
