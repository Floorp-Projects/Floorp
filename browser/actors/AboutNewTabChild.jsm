/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["AboutNewTabChild"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "ACTIVITY_STREAM_DEBUG",
  "browser.newtabpage.activity-stream.debug",
  false
);

class AboutNewTabChild extends JSWindowActorChild {
  handleEvent(event) {
    if (event.type == "DOMContentLoaded") {
      // If the separate about:welcome page is enabled, we can skip all of this,
      // since that mode doesn't load any of the Activity Stream bits.
      if (
        NimbusFeatures.aboutwelcome.isEnabled({ defaultValue: true }) &&
        this.contentWindow.location.pathname.includes("welcome")
      ) {
        return;
      }

      const debug = !AppConstants.RELEASE_OR_BETA && ACTIVITY_STREAM_DEBUG;
      const debugString = debug ? "-dev" : "";

      // This list must match any similar ones in render-activity-stream-html.js.
      const scripts = [
        "chrome://browser/content/contentSearchUI.js",
        "chrome://browser/content/contentSearchHandoffUI.js",
        "chrome://browser/content/contentTheme.js",
        `resource://activity-stream/vendor/react${debugString}.js`,
        `resource://activity-stream/vendor/react-dom${debugString}.js`,
        "resource://activity-stream/vendor/prop-types.js",
        "resource://activity-stream/vendor/react-transition-group.js",
        "resource://activity-stream/vendor/redux.js",
        "resource://activity-stream/vendor/react-redux.js",
        "resource://activity-stream/data/content/activity-stream.bundle.js",
        "resource://activity-stream/data/content/newtab-render.js",
      ];

      for (let script of scripts) {
        Services.scriptloader.loadSubScript(script, this.contentWindow);
      }
    } else if (
      (event.type == "pageshow" || event.type == "visibilitychange") &&
      // The default browser notification shouldn't be shown on about:welcome
      // since we don't want to distract from the onboarding wizard.
      !this.contentWindow.location.pathname.includes("welcome")
    ) {
      // Don't show the notification in non-permanent private windows
      // since it is expected to have very little opt-in here.
      let contentWindowPrivate = PrivateBrowsingUtils.isContentWindowPrivate(
        this.contentWindow
      );
      if (
        this.document.visibilityState == "visible" &&
        (!contentWindowPrivate ||
          (contentWindowPrivate &&
            PrivateBrowsingUtils.permanentPrivateBrowsing))
      ) {
        this.sendAsyncMessage("DefaultBrowserNotification");

        // Note: newtab feature info is currently being loaded in PrefsFeed.jsm,
        // But we're recording exposure events here.
        NimbusFeatures.newtab.recordExposureEvent();
      }
    }
  }
}
