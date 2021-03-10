/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals browser, AppConstants, Services, ExtensionAPI */

"use strict";

ChromeUtils.defineModuleGetter(this, "AppConstants",
                               "resource://gre/modules/AppConstants.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "ExtensionCommon",
  "resource://gre/modules/ExtensionCommon.jsm"
);

const TOPIC = "contextmenu-screenshot";

this.screenshots = class extends ExtensionAPI {
  getAPI(context) {
    const { extension } = context;
    const { tabManager } = extension;
    let EventManager = ExtensionCommon.EventManager;

    return {
      experiments: {
        screenshots: {
          // If you are checking for 'nightly', also check for 'nightly-try'.
          //
          // Otherwise, just use the standard builds, but be aware of the many
          // non-standard options that also exist (as of August 2018).
          //
          // Standard builds:
          //   'esr' - ESR channel
          //   'release' - release channel
          //   'beta' - beta channel
          //   'nightly' - nightly channel
          // Non-standard / deprecated builds:
          //   'aurora' - deprecated aurora channel (still observed in dxr)
          //   'default' - local builds from source
          //   'nightly-try' - nightly Try builds (QA may occasionally need to test with these)
          getUpdateChannel() {
            return AppConstants.MOZ_UPDATE_CHANNEL;
          },
          isHistoryEnabled() {
            return Services.prefs.getBoolPref("places.history.enabled", true);
          },
          isUploadDisabled() {
            return Services.prefs.getBoolPref("extensions.screenshots.upload-disabled", false);
          },
          onScreenshotCommand: new EventManager({
            context,
            name: "experiments.screenshots.onScreenshotCommand",
            register: fire => {
              let observer = () => {
                fire.sync();
              };
              Services.obs.addObserver(observer, TOPIC);

              return () => {
                Services.obs.removeObserver(observer, TOPIC);
              };
            },
          }).api(),
        },
      },
    };
  }
};
