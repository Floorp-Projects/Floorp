/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarContextualTip: "resource:///modules/UrlbarContextualTip.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProviderExtension: "resource:///modules/UrlbarProviderExtension.jsm",
});

var { ExtensionPreferencesManager } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPreferencesManager.jsm"
);
var { getSettingsAPI } = ExtensionPreferencesManager;

ExtensionPreferencesManager.addSetting("openViewOnFocus", {
  prefNames: ["browser.urlbar.openViewOnFocus"],
  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },
});

ExtensionPreferencesManager.addSetting("engagementTelemetry", {
  prefNames: ["browser.urlbar.eventTelemetry.enabled"],
  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },
});
// Keeps track of the id of the extension using the contextual tip.
// Only one extension is allowed to use the contextual tip.
// If another extension attempts to use the contextual tip,
// then an error is thrown.
let idOfExtUsingContextualTip = null;

this.urlbar = class extends ExtensionAPI {
  onShutdown() {
    if (idOfExtUsingContextualTip === this.extension.id) {
      for (let w of windowTracker.browserWindows()) {
        w.gURLBar.view.removeContextualTip();
      }
      idOfExtUsingContextualTip = null;
    }
  }

  /**
   * If no extension is using the contextual tip then the current extension
   * will own the contextual tip. If the contextual tip is already being used
   * by another extension then an error is thrown.
   */
  _registerExtensionToUseContextualTip() {
    if (idOfExtUsingContextualTip == null) {
      idOfExtUsingContextualTip = this.extension.id;
    }

    if (idOfExtUsingContextualTip !== this.extension.id) {
      throw new Error(
        "There was an attempt to use the contextual tip API but " +
          "another extension is already using the contextual tip API."
      );
    }
  }

  /**
   * Given either a "button" or "link" string, returns a function to
   * register click listeners on the contextual tip's button or link element.
   *
   * See the `urlbar.contextualTip.onButtonClicked` and
   * `urlbar.contextualTip.onLinkClicked` events for usage.
   *
   * @param {string} type
   *   Represents the type of element to register a click listener on.
   *   Must be either "button" or "link".
   * @returns {function} Returns a function to register on the EventManager.
   */
  _registerClickListener(type) {
    return fire => {
      this._registerExtensionToUseContextualTip();

      const listener = window => {
        const windowId = this.extension.windowManager.wrapWindow(window).id;
        fire.async(windowId);
      };

      UrlbarContextualTip.addClickListener(type, listener);

      return () => {
        UrlbarContextualTip.removeClickListener(type, listener);
      };
    };
  }

  getAPI(context) {
    return {
      urlbar: {
        onBehaviorRequested: new EventManager({
          context,
          name: "urlbar.onBehaviorRequested",
          register: (fire, providerName) => {
            let provider = UrlbarProviderExtension.getOrCreate(providerName);
            provider.setEventListener(
              "behaviorRequested",
              async queryContext => {
                if (queryContext.isPrivate && !context.privateBrowsingAllowed) {
                  return "inactive";
                }
                return fire.async(queryContext).catch(error => {
                  throw context.normalizeError(error);
                });
              }
            );
            return () => provider.setEventListener("behaviorRequested", null);
          },
        }).api(),

        onQueryCanceled: new EventManager({
          context,
          name: "urlbar.onQueryCanceled",
          register: (fire, providerName) => {
            let provider = UrlbarProviderExtension.getOrCreate(providerName);
            provider.setEventListener("queryCanceled", async queryContext => {
              if (queryContext.isPrivate && !context.privateBrowsingAllowed) {
                return;
              }
              await fire.async(queryContext).catch(error => {
                throw context.normalizeError(error);
              });
            });
            return () => provider.setEventListener("queryCanceled", null);
          },
        }).api(),

        onResultsRequested: new EventManager({
          context,
          name: "urlbar.onResultsRequested",
          register: (fire, providerName) => {
            let provider = UrlbarProviderExtension.getOrCreate(providerName);
            provider.setEventListener(
              "resultsRequested",
              async queryContext => {
                if (queryContext.isPrivate && !context.privateBrowsingAllowed) {
                  return [];
                }
                return fire.async(queryContext).catch(error => {
                  throw context.normalizeError(error);
                });
              }
            );
            return () => provider.setEventListener("resultsRequested", null);
          },
        }).api(),

        openViewOnFocus: getSettingsAPI(
          context.extension.id,
          "openViewOnFocus",
          () => UrlbarPrefs.get("openViewOnFocus")
        ),

        engagementTelemetry: getSettingsAPI(
          context.extension.id,
          "engagementTelemetry",
          () => UrlbarPrefs.get("eventTelemetry.enabled")
        ),

        contextualTip: {
          /**
           * Sets the contextual tip's title, button title, and link title.
           *
           * @param {object} details
           *   If null, then the contextual tip will be hidden.
           * @param {string} details.title
           *   Main title on the contextual tip.
           * @param {string} [details.buttonTitle]
           *   If not given, no button is shown.
           * @param {string} [details.linkTitle]
           *   If not given, no link is shown.
           */
          set: details => {
            this._registerExtensionToUseContextualTip();
            const mostRecentWindow = windowTracker.getTopNormalWindow(context);
            mostRecentWindow.gURLBar.view.setContextualTip(details);
          },

          /**
           * Hides the contextual tip.
           */
          remove: () => {
            this._registerExtensionToUseContextualTip();
            const mostRecentWindow = windowTracker.getTopNormalWindow(context);
            mostRecentWindow.gURLBar.view.hideContextualTip();
          },

          /*
           * Fired when the user clicks the contextual tip's button.
           * The listener is passed the id of the window where
           * the contextual tip was clicked.
           */
          onButtonClicked: new EventManager({
            context,
            name: "urlbar.contextualTip.onButtonClicked",
            register: this._registerClickListener("button"),
          }).api(),

          /**
           * Fired when the user clicks the contextual tip's link.
           * The listener is passed the id of the window where
           * the contextual tip was clicked.
           */
          onLinkClicked: new EventManager({
            context,
            name: "urlbar.contextualTip.onLinkClicked",
            register: this._registerClickListener("link"),
          }).api(),
        },
      },
    };
  }
};
