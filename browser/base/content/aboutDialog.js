/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from aboutDialog-appUpdater.js */

// Services = object with smart getters for common XPCOM services
var {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
var {AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

async function init(aEvent) {
  if (aEvent.target != document)
    return;

  var distroId = Services.prefs.getCharPref("distribution.id", "");
  if (distroId) {
    var distroString = distroId;

    var distroVersion = Services.prefs.getCharPref("distribution.version", "");
    if (distroVersion) {
      distroString += " - " + distroVersion;
    }

    var distroIdField = document.getElementById("distributionId");
    distroIdField.value = distroString;
    distroIdField.style.display = "block";

    var distroAbout = Services.prefs.getStringPref("distribution.about", "");
    if (distroAbout) {
      var distroField = document.getElementById("distribution");
      distroField.value = distroAbout;
      distroField.style.display = "block";
    }
  }

  // Include the build ID and display warning if this is an "a#" (nightly or aurora) build
  let versionId = "aboutDialog-version";
  let versionAttributes = {
    version: AppConstants.MOZ_APP_VERSION_DISPLAY,
    bits: Services.appinfo.is64Bit ? 64 : 32,
  };

  let version = Services.appinfo.version;
  if (/a\d+$/.test(version)) {
    versionId = "aboutDialog-version-nightly";
    let buildID = Services.appinfo.appBuildID;
    let year = buildID.slice(0, 4);
    let month = buildID.slice(4, 6);
    let day = buildID.slice(6, 8);
    versionAttributes.isodate = `${year}-${month}-${day}`;

    document.getElementById("experimental").hidden = false;
    document.getElementById("communityDesc").hidden = true;
  }

  // Use Fluent arguments for append version and the architecture of the build
  let versionField = document.getElementById("version");

  document.l10n.setAttributes(versionField, versionId, versionAttributes);

  await document.l10n.translateElements([versionField]);

  // Show a release notes link if we have a URL.
  let relNotesLink = document.getElementById("releasenotes");
  let relNotesPrefType = Services.prefs.getPrefType("app.releaseNotesURL");
  if (relNotesPrefType != Services.prefs.PREF_INVALID) {
    let relNotesURL = Services.urlFormatter.formatURLPref("app.releaseNotesURL");
    if (relNotesURL != "about:blank") {
      relNotesLink.href = relNotesURL;
      relNotesLink.hidden = false;
    }
  }

  if (AppConstants.MOZ_UPDATER) {
    gAppUpdater = new appUpdater({ buttonAutoFocus: true });

    let channelLabel = document.getElementById("currentChannel");
    let currentChannelText = document.getElementById("currentChannelText");
    channelLabel.value = UpdateUtils.UpdateChannel;
    if (/^release($|\-)/.test(channelLabel.value))
        currentChannelText.hidden = true;
  }

  if (AppConstants.MOZ_APP_VERSION_DISPLAY.endsWith("esr")) {
    document.getElementById("release").hidden = false;
  }

  window.sizeToContent();

  if (AppConstants.platform == "macosx") {
    window.moveTo((screen.availWidth / 2) - (window.outerWidth / 2), screen.availHeight / 5);
  }
}
