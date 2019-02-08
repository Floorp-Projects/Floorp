/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable no-unused-vars */

ChromeUtils.defineModuleGetter(this, "Preferences",
                               "resource://gre/modules/Preferences.jsm");
ChromeUtils.defineModuleGetter(this, "PluralForm",
                               "resource://gre/modules/PluralForm.jsm");
ChromeUtils.defineModuleGetter(this, "RemoteSettings",
                               "resource://services-settings/remote-settings.js");
const {setTimeout, clearTimeout} = ChromeUtils.import("resource://gre/modules/Timer.jsm", {});
Cu.importGlobalProperties(["fetch", "btoa"]);

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
