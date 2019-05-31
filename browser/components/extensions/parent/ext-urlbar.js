"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
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
      },
    };
  }
};
