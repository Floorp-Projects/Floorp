"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
});

var {ExtensionPreferencesManager} = ChromeUtils.import("resource://gre/modules/ExtensionPreferencesManager.jsm");
var {getSettingsAPI} = ExtensionPreferencesManager;

ExtensionPreferencesManager.addSetting("openViewOnFocus", {
  prefNames: [
    "browser.urlbar.openViewOnFocus",
  ],
  setCallback(value) {
    return {[this.prefNames[0]]: value};
  },
});

this.urlbar = class extends ExtensionAPI {
  getAPI(context) {
    return {
      urlbar: {
        /**
         * Event fired before a search starts, to get the provider behavior.
         */
        onQueryReady: new EventManager(context, "urlbar.onQueryReady", (fire, name) => {
          UrlbarProvidersManager.addExtensionListener(
            name, "queryready", queryContext => {
              if (queryContext.isPrivate && !context.privateBrowsingAllowed) {
                return Promise.resolve("inactive");
              }
              return fire.async(queryContext);
            });
          return () => {
            UrlbarProvidersManager.removeExtensionListener(name, "queryready");
          };
        }).api(),

        openViewOnFocus: getSettingsAPI(
          context.extension.id, "openViewOnFocus",
          () => {
            return UrlbarPrefs.get("openViewOnFocus");
          }),
      },
    };
  }
};
