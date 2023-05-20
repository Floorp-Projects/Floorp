/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from allDownloadsView.js */
/* import-globals-from /toolkit/content/globalOverlay.js */

document.addEventListener("DOMContentLoaded", function () {
  let downloadCommands = document.getElementById("downloadCommands");
  downloadCommands.addEventListener("commandupdate", function () {
    goUpdateDownloadCommands();
  });
  downloadCommands.addEventListener("command", function (event) {
    let { id } = event.target;
    goDoCommand(id);
  });
});
