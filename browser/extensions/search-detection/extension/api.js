/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionCommon, ExtensionAPI, Services, XPCOMUtils, ExtensionUtils */

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);
const { WebRequest } = ChromeUtils.import(
  "resource://gre/modules/WebRequest.jsm"
);

// eslint-disable-next-line mozilla/reject-importGlobalProperties
XPCOMUtils.defineLazyGlobalGetters(this, ["ChannelWrapper"]);

XPCOMUtils.defineLazyGetter(this, "searchInitialized", () => {
  if (Services.search.isInitialized) {
    return Promise.resolve();
  }

  return ExtensionUtils.promiseObserved(
    "browser-search-service",
    (_, data) => data === "init-complete"
  );
});

const SEARCH_TOPIC_ENGINE_MODIFIED = "browser-search-engine-modified";

this.addonsSearchDetection = class extends ExtensionAPI {
  getAPI(context) {
    const { extension } = context;

    // We want to temporarily store the first monitored URLs that have been
    // redirected, indexed by request IDs, so that the background script can
    // find the add-on IDs to report.
    this.firstMatchedUrls = {};

    return {
      addonsSearchDetection: {
        // `getMatchPatterns()` returns a map where each key is an URL pattern
        // to monitor and its corresponding value is a list of add-on IDs
        // (search engines).
        //
        // Note: We don't return a simple list of URL patterns because the
        // background script might want to lookup add-on IDs for a given URL in
        // the case of server-side redirects.
        async getMatchPatterns() {
          const patterns = {};

          try {
            await searchInitialized;
            const visibleEngines = await Services.search.getEngines();

            visibleEngines.forEach(engine => {
              const { _extensionID, _urls } = engine;

              if (!_extensionID) {
                // OpenSearch engines don't have an extension ID.
                return;
              }

              _urls
                // We only want to collect "search URLs" (and not "suggestion"
                // ones for instance). See `URL_TYPE` in `SearchUtils.jsm`.
                .filter(({ type }) => type === "text/html")
                .forEach(({ template }) => {
                  // If this is changed, double check the code in the background
                  // script because `webRequestCancelledHandler` splits patterns
                  // on `*` to retrieve URL prefixes.
                  const pattern = template.split("?")[0] + "*";

                  // Multiple search engines could register URL templates that
                  // would become the same URL pattern as defined above so we
                  // store a list of extension IDs per URL pattern.
                  if (!patterns[pattern]) {
                    patterns[pattern] = [];
                  }

                  // We exclude built-in search engines because we don't need
                  // to report them.
                  if (
                    !patterns[pattern].includes(_extensionID) &&
                    !_extensionID.endsWith("@search.mozilla.org")
                  ) {
                    patterns[pattern].push(_extensionID);
                  }
                });
            });
          } catch (err) {
            Cu.reportError(err);
          }

          return patterns;
        },

        // `getAddonVersion()` returns the add-on version if it exists.
        async getAddonVersion(addonId) {
          const addon = await AddonManager.getAddonByID(addonId);

          return addon && addon.version;
        },

        // `getPublicSuffix()` returns the public suffix/Effective TLD Service
        // of the given URL.
        // See: https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Reference/Interface/nsIEffectiveTLDService
        async getPublicSuffix(url) {
          try {
            return Services.eTLD.getBaseDomain(Services.io.newURI(url));
          } catch (err) {
            Cu.reportError(err);
            return null;
          }
        },

        // `onSearchEngineModified` is an event that occurs when the list of
        // search engines has changed, e.g., a new engine has been added or an
        // engine has been removed.
        //
        // See: https://searchfox.org/mozilla-central/source/toolkit/components/search/SearchUtils.jsm#145-152
        onSearchEngineModified: new ExtensionCommon.EventManager({
          context,
          name: "addonsSearchDetection.onSearchEngineModified",
          register: fire => {
            const onSearchEngineModifiedObserver = (
              aSubject,
              aTopic,
              aData
            ) => {
              if (
                aTopic !== SEARCH_TOPIC_ENGINE_MODIFIED ||
                // We are only interested in these modified types.
                !["engine-added", "engine-removed", "engine-changed"].includes(
                  aData
                )
              ) {
                return;
              }

              fire.async();
            };

            Services.obs.addObserver(
              onSearchEngineModifiedObserver,
              SEARCH_TOPIC_ENGINE_MODIFIED
            );

            return () => {
              Services.obs.removeObserver(
                onSearchEngineModifiedObserver,
                SEARCH_TOPIC_ENGINE_MODIFIED
              );
            };
          },
        }).api(),

        // `onRedirected` is an event fired after a request has stopped and
        // this request has been redirected once or more. The registered
        // listeners will received the following properties:
        //
        //   - `addonId`: the add-on ID that redirected the request, if any.
        //   - `firstUrl`: the first monitored URL of the request that has
        //      been redirected.
        //   - `lastUrl`: the last URL loaded for the request, after one or
        //      more redirects.
        onRedirected: new ExtensionCommon.EventManager({
          context,
          name: "addonsSearchDetection.onRedirected",
          register: (fire, filter) => {
            const stopListener = event => {
              if (event.type != "stop") {
                return;
              }

              const wrapper = event.currentTarget;
              const { channel, id: requestId } = wrapper;

              // When we detected a redirect, we read the request property,
              // hoping to find an add-on ID corresponding to the add-on that
              // initiated the redirect. It might not return anything when the
              // redirect is a search server-side redirect but it can also be
              // caused by an error.
              let addonId;
              try {
                addonId = channel
                  ?.QueryInterface(Ci.nsIPropertyBag)
                  ?.getProperty("redirectedByExtension");
              } catch (err) {
                Cu.reportError(err);
              }

              const firstUrl = this.firstMatchedUrls[requestId];
              // We don't need this entry anymore.
              delete this.firstMatchedUrls[requestId];

              const lastUrl = wrapper.finalURL;

              if (!firstUrl || !lastUrl) {
                // Something went wrong but there is nothing we can do at this
                // point.
                return;
              }

              fire.sync({ addonId, firstUrl, lastUrl });
            };

            const listener = ({ requestId, url, originUrl }) => {
              // We exclude requests not originating from the location bar,
              // bookmarks and other "system-ish" requests.
              if (originUrl !== undefined) {
                return;
              }

              // Keep the first monitored URL that was redirected for the
              // request until the request has stopped.
              if (!this.firstMatchedUrls[requestId]) {
                this.firstMatchedUrls[requestId] = url;

                const wrapper = ChannelWrapper.getRegisteredChannel(
                  requestId,
                  context.extension.policy,
                  context.xulBrowser.frameLoader.remoteTab
                );

                wrapper.addEventListener("stop", stopListener);
              }
            };

            WebRequest.onBeforeRedirect.addListener(
              listener,
              // filter
              {
                types: ["main_frame"],
                urls: ExtensionUtils.parseMatchPatterns(filter.urls),
              },
              // info
              [],
              // listener details
              {
                addonId: extension.id,
                policy: extension.policy,
                blockingAllowed: false,
              }
            );

            return () => {
              WebRequest.onBeforeRedirect.removeListener(listener);
            };
          },
        }).api(),
      },
    };
  }
};
