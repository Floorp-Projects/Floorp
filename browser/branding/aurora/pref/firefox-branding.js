

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We don't have pages ready for this, so leave these prefs empty for now (bug 648330)
pref("startup.homepage_override_url","");
pref("startup.homepage_welcome_url","");
// The time interval between checks for a new version (in seconds)
pref("app.update.interval", 28800); // 8 hours
// The time interval between the downloading of mar file chunks in the
// background (in seconds)
pref("app.update.download.backgroundInterval", 60);
// Give the user x seconds to react before showing the big UI. default=24 hours
pref("app.update.promptWaitTime", 86400);
// URL user can browse to manually if for some reason all update installation
// attempts fail.
pref("app.update.url.manual", "http://www.mozilla.com/firefox/channel/");
// A default value for the "More information about this update" link
// supplied in the "An update is available" page of the update wizard. 
pref("app.update.url.details", "http://www.mozilla.org/projects/firefox/");

// Release notes and vendor URLs
pref("app.releaseNotesURL", "http://www.mozilla.org/projects/firefox/%VERSION%/releasenotes/");
pref("app.vendorURL", "http://www.mozilla.org/projects/firefox/");

// Search codes belong only in builds with official branding
pref("browser.search.param.yahoo-fr", "");
pref("browser.search.param.yahoo-fr-cjkt", ""); // now unused
pref("browser.search.param.yahoo-fr-ja", "");
pref("browser.search.param.yahoo-f-CN", "");
