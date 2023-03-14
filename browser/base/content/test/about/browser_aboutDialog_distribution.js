/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/mozapps/update/tests/browser/head.js",
  this
);

add_task(async function verify_distribution_info_hides() {
  let defaultBranch = Services.prefs.getDefaultBranch(null);

  defaultBranch.setCharPref("distribution.id", "mozilla-test-distribution-id");
  defaultBranch.setCharPref("distribution.version", "1.0");

  let aboutDialog = await waitForAboutDialog();

  let distroIdField = aboutDialog.document.getElementById("distributionId");
  let distroField = aboutDialog.document.getElementById("distribution");

  if (
    AppConstants.platform === "win" &&
    Services.sysinfo.getProperty("hasWinPackageId")
  ) {
    is(distroIdField.value, "mozilla-test-distribution-id - 1.0");
    is(distroIdField.style.display, "block");
    is(distroField.style.display, "block");
  } else {
    is(distroIdField.value, "");
    isnot(distroIdField.style.display, "block");
    isnot(distroField.style.display, "block");
  }

  aboutDialog.close();
});

add_task(async function verify_distribution_info_displays() {
  let defaultBranch = Services.prefs.getDefaultBranch(null);

  defaultBranch.setCharPref("distribution.id", "test-distribution-id");
  defaultBranch.setCharPref("distribution.version", "1.0");
  defaultBranch.setCharPref("distribution.about", "About Text");

  let aboutDialog = await waitForAboutDialog();

  let distroIdField = aboutDialog.document.getElementById("distributionId");

  is(distroIdField.value, "test-distribution-id - 1.0");
  is(distroIdField.style.display, "block");

  let distroField = aboutDialog.document.getElementById("distribution");
  is(distroField.value, "About Text");
  is(distroField.style.display, "block");

  aboutDialog.close();
});

add_task(async function cleanup() {
  let defaultBranch = Services.prefs.getDefaultBranch(null);

  // This is the best we can do since we can't remove default prefs
  defaultBranch.setCharPref("distribution.id", "");
  defaultBranch.setCharPref("distribution.version", "");
  defaultBranch.setCharPref("distribution.about", "");
});
