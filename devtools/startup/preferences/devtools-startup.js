/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The preferences defined here should be used by the components in devtools-startup.
// devtools-startup is always shipped and those preferences will always be available.

// Enable the JSON View tool (an inspector for application/json documents).
pref("devtools.jsonview.enabled", true);

// Default theme ("dark" or "light")
#ifdef MOZ_DEV_EDITION
pref("devtools.theme", "dark", sticky);
#else
pref("devtools.theme", "light", sticky);
#endif

// Completely disable DevTools entry points, as well as all DevTools command line
// arguments This should be merged with devtools.enabled, see Bug 1440675.
pref("devtools.policy.disabled", false);
