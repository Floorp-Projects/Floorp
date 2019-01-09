/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var MockFilePicker = SpecialPowers.MockFilePicker;

function createTemporarySaveDirectory() {
  var saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists()) {
    info("create testsavedir!");
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  info("return from createTempSaveDir: " + saveDir.path);
  return saveDir;
}

add_task(async function checkCertExportWorks() {
  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  let certButton = gBrowser.selectedBrowser.contentDocument.getElementById("viewCertificatesButton");
  certButton.scrollIntoView();
  let certDialogLoaded = promiseLoadSubDialog("chrome://pippki/content/certManager.xul");
  certButton.click();
  let dialogWin = await certDialogLoaded;
  let doc = dialogWin.document;
  doc.getElementById("certmanagertabs").selectedTab = doc.getElementById("ca_tab");
  let expectedCert;
  let treeView = doc.getElementById("ca-tree").treeBoxObject.view;
  // Select any which cert. Ignore parent rows (ie rows without certs):
  for (let i = 0; i < treeView.rowCount; i++) {
    treeView.selection.select(i);
    dialogWin.getSelectedCerts();
    let certs = dialogWin.selected_certs; // yuck... but this is how the dialog works.
    if (certs && certs.length == 1 && certs[0]) {
      expectedCert = certs[0];
      // OK, we managed to select a cert!
      break;
    }
  }

  let exportButton = doc.getElementById("ca_exportButton");
  is(exportButton.disabled, false, "Should enable export button");
  // Create the folder the link will be saved into.
  var destDir = createTemporarySaveDirectory();
  var destFile = destDir.clone();
  MockFilePicker.init(window);
  registerCleanupFunction(function() {
    MockFilePicker.cleanup();
    destDir.remove(true);
  });
  MockFilePicker.displayDirectory = destDir;
  MockFilePicker.showCallback = function(fp) {
    info("showCallback");
    let fileName = fp.defaultString;
    info("fileName: " + fileName);
    destFile.append(fileName);
    MockFilePicker.setFiles([destFile]);
    MockFilePicker.filterIndex = 0; // Save an x509 PEM copy of the cert.
    info("done showCallback");
  };
  let finishedExporting = TestUtils.topicObserved("cert-export-finished");
  exportButton.click();
  await finishedExporting;
  if (destFile && destFile.exists()) {
    let contents = await OS.File.read(destFile.path, {encoding: "utf-8"});
    is(contents, dialogWin.getPEMString(expectedCert), "Should have written correct contents");
  } else {
    ok(false, "No cert saved!");
  }
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

