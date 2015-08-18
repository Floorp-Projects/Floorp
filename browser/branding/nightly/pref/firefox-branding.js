/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pref("startup.homepage_override_url", "https://www.mozilla.org/projects/firefox/%VERSION%/whatsnew/?oldversion=%OLD_VERSION%");
pref("startup.homepage_welcome_url", "https://www.mozilla.org/projects/firefox/%VERSION%/firstrun/");
// The time interval between checks for a new version (in seconds)
pref("app.update.interval", 7200); // 2 hours
// The time interval between the downloading of mar file chunks in the
// background (in seconds)
// 0 means "download everything at once"
pref("app.update.download.backgroundInterval", 0);
// Give the user x seconds to react before showing the big UI. default=12 hours
pref("app.update.promptWaitTime", 43200);
// URL user can browse to manually if for some reason all update installation
// attempts fail.
pref("app.update.url.manual", "https://nightly.mozilla.org");
// A default value for the "More information about this update" link
// supplied in the "An update is available" page of the update wizard. 
pref("app.update.url.details", "https://nightly.mozilla.org");

// The number of days a binary is permitted to be old
// without checking for an update.  This assumes that
// app.update.checkInstallTime is true.
pref("app.update.checkInstallTime.days", 2);

// code usage depends on contracts, please contact the Firefox module owner if you have questions
pref("browser.search.param.yahoo-fr", "moz35");
pref("browser.search.param.yahoo-fr-ja", "mozff");

// Number of usages of the web console or scratchpad.
// If this is less than 5, then pasting code into the web console or scratchpad is disabled
pref("devtools.selfxss.count", 5);