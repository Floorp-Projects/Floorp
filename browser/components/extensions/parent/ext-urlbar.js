"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
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

this.urlbar = class extends ExtensionAPI {
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
      },
    };
  }
};
