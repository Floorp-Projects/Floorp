/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file contains branding-specific prefs.

pref("startup.homepage_override_url", "https://blog.ablaze.one/category/ablaze/ablaze-project/floorp/");
pref("startup.homepage_welcome_url", "about:setup | https://blog.ablaze.one/category/ablaze/ablaze-project/floorp/");
pref("startup.homepage_welcome_url.additional", "https://docs.ablaze.one/floorp_privacy_policy/");
// Interval: Time between checks for a new version (in seconds)
pref("app.update.interval", 43200); // 12 hours
// Give the user x seconds to react before showing the big UI. default=192 hours
pref("app.update.promptWaitTime", 691200);
// app.update.url.manual: URL user can browse to manually if for some reason
// all update installation attempts fail.
// app.update.url.details: a default value for the "More information about this
// update" link supplied in the "An update is available" page of the update
// wizard.


// wizard.
#if MOZ_UPDATE_CHANNEL == beta
  pref("app.update.url.manual", "https://floorp.app");
  pref("app.update.url.details", "https://blog.ablaze.one/category/ablaze/ablaze-project/floorp/");
  pref("app.releaseNotesURL", "https://blog.ablaze.one/category/ablaze/ablaze-project/floorp/");
  pref("app.releaseNotesURL.aboutDialog", "https://blog.ablaze.one/category/ablaze/ablaze-project/floorp/");
#else
  pref("app.update.url.manual", "https://floorp.app");
  pref("app.update.url.details", "https://blog.ablaze.one/category/ablaze/ablaze-project/floorp/");
  pref("app.releaseNotesURL", "https://blog.ablaze.one/category/ablaze/ablaze-project/floorp/");
  pref("app.releaseNotesURL.aboutDialog", "https://blog.ablaze.one/category/ablaze/ablaze-project/floorp/");
#endif
// The number of days a binary is permitted to be old
// without checking for an update.  This assumes that
// app.update.checkInstallTime is true.
pref("app.update.checkInstallTime.days", 63);

// Give the user x seconds to reboot before showing a badge on the hamburger
// button. default=4 days
pref("app.update.badgeWaitTime", 345600);

// Number of usages of the web console.
// If this is less than 5, then pasting code into the web console is disabled
pref("devtools.selfxss.count", 0);
