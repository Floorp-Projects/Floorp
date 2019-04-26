/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  Services.prefs.lockPref("browser.download.useDownloadDir");

  await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;

  var downloadFolder = doc.getElementById("downloadFolder");
  var chooseFolder = doc.getElementById("chooseFolder");
  is(downloadFolder.disabled, false, "Download folder field should not be disabled.");
  is(chooseFolder.disabled, false, "Choose folder should not be disabled.");

  gBrowser.removeCurrentTab();

  Services.prefs.unlockPref("browser.download.useDownloadDir");
});
