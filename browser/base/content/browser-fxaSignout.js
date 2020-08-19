// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

ChromeUtils.defineModuleGetter(
  this,
  "SyncDisconnect",
  "resource://services-sync/SyncDisconnect.jsm"
);

function onLoad() {
  const hideDeleteLocalDataSections = window.arguments[0].hideDeleteDataOption;
  document.getElementById(
    "fxaDeleteLocalDataContent"
  ).hidden = hideDeleteLocalDataSections;
  document.getElementById(
    "fxaSignoutBody"
  ).hidden = hideDeleteLocalDataSections;

  document.addEventListener("dialogaccept", () => {
    window.arguments[1].deleteLocalData = document.getElementById(
      "shouldDeleteLocalData"
    ).checked;
    window.arguments[1].userConfirmedDisconnect = true;
  });
}
