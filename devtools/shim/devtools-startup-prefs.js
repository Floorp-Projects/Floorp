/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This Devtools preferences file will be loaded as a usual Firefox preferences file.
// Most DevTools prefs are included with the addon and loaded dynamically during the addon
// startup. For preferences that are required before the addon is loaded or that we can't
// process in JS, they can be defined in this file.
// Note that this preference file follows Firefox release cycle.

// Enable the JSON View tool (an inspector for application/json documents).
pref("devtools.jsonview.enabled", true);

// Default theme ("dark" or "light")
#ifdef MOZ_DEV_EDITION
pref("devtools.theme", "dark", sticky);
#else
pref("devtools.theme", "light", sticky);
#endif

// Should the devtools toolbar be opened on startup
pref("devtools.toolbar.visible", false);

// Pref to drive the devtools onboarding flow experiment. States:
// - off: forces devtools.enabled to true
// - on: devtools.enabled is not forced to true.
// - force: devtools.enabled is not forced to true and cannot be set to true by checking
//   devtools.selfxss.count. User will have to go through onboarding to use DevTools.
pref("devtools.onboarding.experiment", "off");

// If devtools.onboarding.experiment is set to "on" or "force", we will flip the
// devtools.enabled preference to false once. The flag is used to make sure it is only
// flipped once.
pref("devtools.onboarding.experiment.flipped", false);

// Flag to check if we already logged the devtools onboarding related probe.
pref("devtools.onboarding.telemetry.logged", false);

// Completely disable DevTools entry points, as well as all DevTools command line
// arguments This should be merged with devtools.enabled, see Bug 1440675.
pref("devtools.policy.disabled", false);
