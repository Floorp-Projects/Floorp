/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderExtension:
    "resource:///modules/UrlbarProviderExtension.sys.mjs",
});

var { ExtensionPreferencesManager } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPreferencesManager.jsm"
);
var { getSettingsAPI } = ExtensionPreferencesManager;

ExtensionPreferencesManager.addSetting("engagementTelemetry", {
  prefNames: ["browser.urlbar.eventTelemetry.enabled"],
  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },
});

this.urlbar = class extends ExtensionAPI {
  getAPI(context) {
    return {
      urlbar: {
        closeView() {
          let window = windowTracker.getTopNormalWindow(context);
          window.gURLBar.view.close();
        },

        focus(select = false) {
          let window = windowTracker.getTopNormalWindow(context);
          if (select) {
            window.gURLBar.select();
          } else {
            window.gURLBar.focus();
          }
        },

        search(searchString, options = {}) {
          let window = windowTracker.getTopNormalWindow(context);
          window.gURLBar.search(searchString, options);
        },

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

        onEngagement: new EventManager({
          context,
          name: "urlbar.onEngagement",
          register: (fire, providerName) => {
            let provider = UrlbarProviderExtension.getOrCreate(providerName);
            provider.setEventListener(
              "engagement",
              async (isPrivate, state) => {
                if (isPrivate && !context.privateBrowsingAllowed) {
                  return;
                }
                return fire.async(state).catch(error => {
                  throw context.normalizeError(error);
                });
              }
            );
            return () => provider.setEventListener("engagement", null);
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

        onResultPicked: new EventManager({
          context,
          name: "urlbar.onResultPicked",
          inputHandling: true,
          register: (fire, providerName) => {
            let provider = UrlbarProviderExtension.getOrCreate(providerName);
            provider.setEventListener(
              "resultPicked",
              async (resultPayload, dynamicElementName) => {
                return fire
                  .async(resultPayload, dynamicElementName)
                  .catch(error => {
                    throw context.normalizeError(error);
                  });
              }
            );
            return () => provider.setEventListener("resultPicked", null);
          },
        }).api(),

        engagementTelemetry: getSettingsAPI({
          context,
          name: "engagementTelemetry",
          callback: () => UrlbarPrefs.get("eventTelemetry.enabled"),
        }),
      },
    };
  }
};
