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
const { ExperimentAPI } = ChromeUtils.import(
  "resource://messaging-system/experiments/ExperimentAPI.jsm"
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "ACTIVITY_STREAM_DEBUG",
  "browser.newtabpage.activity-stream.debug",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "isAboutWelcomePrefEnabled",
  "browser.aboutwelcome.enabled",
  false
);

class AboutNewTabChild extends JSWindowActorChild {
  handleEvent(event) {
    if (event.type == "DOMContentLoaded") {
      // If the separate about:welcome page is enabled, we can skip all of this,
      // since that mode doesn't load any of the Activity Stream bits.
      if (
        isAboutWelcomePrefEnabled &&
        // about:welcome should be enabled by default if no experiment exists.
        ExperimentAPI.isFeatureEnabled("aboutwelcome", true) &&
        this.contentWindow.location.pathname.includes("welcome")
      ) {
        return;
      }

      // In the event that the document that was loaded here was the cached
      // about:home document, then there's nothing further to do - the page
      // will load its scripts itself.
      //
      // Note that it's okay to waive the xray wrappers here since this actor
      // is registered to only run in the privileged about content process from
      // about:home, about:newtab and about:welcome.
      if (ChromeUtils.waiveXrays(this.contentWindow).__FROM_STARTUP_CACHE__) {
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
      if (this.document.visibilityState == "visible") {
        this.sendAsyncMessage("DefaultBrowserNotification");
      }
    }
  }
}
