// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

window.attachEvent("onload", function() {
  // Set text direction.
  var direction = external.getTextDirection();
  var profileRefreshForm = document.getElementById("profileRefreshForm");
  profileRefreshForm.style.direction = direction;
  var checkboxLabel = document.getElementById("checkboxLabel");
  checkboxLabel.className += " checkboxLabel-" + direction;

  // Get this page's static strings.
  document.getElementById("header").innerText = external.getUIString(
    "cleanup_header"
  );
  document.getElementById("refreshButton").innerText = external.getUIString(
    "cleanup_button"
  );
  checkboxLabel.innerText = external.getUIString("cleanup_checkbox");

  // Set up the confirmation button.
  profileRefreshForm.attachEvent("onsubmit", function() {
    var doProfileCleanup = document.getElementById("refreshCheckbox").checked;
    external.gotoInstallPage(doProfileCleanup);
    return false;
  });

  document.getElementById("refreshButton").focus();
});
