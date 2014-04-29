# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#filter substitution

pref("general.useragent.locale", "@AB_CD@");

// Enable sparse localization by setting a few package locale overrides
pref("chrome.override_package.global", "b2g-l10n");
pref("chrome.override_package.mozapps", "b2g-l10n");
pref("chrome.override_package.passwordmgr", "b2g-l10n");
