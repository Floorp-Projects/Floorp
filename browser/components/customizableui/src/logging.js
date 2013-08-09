#if 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

XPCOMUtils.defineLazyModuleGetter(this, "console",
  "resource://gre/modules/devtools/Console.jsm");

let gDebug = false;
try {
  gDebug = Services.prefs.getBoolPref(kPrefCustomizationDebug);
} catch (e) {}

function LOG(...args) {
  if (gDebug) {
    args.unshift(gModuleName);
    console.log.apply(console, args);
  }
}

function ERROR(...args) {
  args.unshift(gModuleName);
  console.error.apply(console, args);
}
