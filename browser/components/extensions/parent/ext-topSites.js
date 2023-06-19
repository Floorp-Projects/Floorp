/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AboutNewTab: "resource:///modules/AboutNewTab.sys.mjs",
  NewTabUtils: "resource://gre/modules/NewTabUtils.sys.mjs",
  getSearchProvider: "resource://activity-stream/lib/SearchShortcuts.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(this, {
  shortURL: "resource://activity-stream/lib/ShortURL.jsm",
});

const SHORTCUTS_PREF =
  "browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts";
const TOPSITES_FEED_PREF =
  "browser.newtabpage.activity-stream.feeds.system.topsites";

this.topSites = class extends ExtensionAPI {
  getAPI(context) {
    return {
      topSites: {
        get: async function (options) {
          // We fallback to newtab = false behavior if the user disabled their
          // Top Sites feed.
          let getNewtabSites =
            options.newtab &&
            Services.prefs.getBoolPref(TOPSITES_FEED_PREF, false);
          let links = getNewtabSites
            ? AboutNewTab.getTopSites()
            : await NewTabUtils.activityStreamLinks.getTopSites({
                ignoreBlocked: options.includeBlocked,
                onePerDomain: options.onePerDomain,
                numItems: options.limit,
                includeFavicon: options.includeFavicon,
              });

          if (options.includePinned && !getNewtabSites) {
            let pinnedLinks = NewTabUtils.pinnedLinks.links;
            if (options.includeFavicon) {
              pinnedLinks =
                NewTabUtils.activityStreamProvider._faviconBytesToDataURI(
                  await NewTabUtils.activityStreamProvider._addFavicons(
                    pinnedLinks
                  )
                );
            }
            pinnedLinks.forEach((pinnedLink, index) => {
              if (
                pinnedLink &&
                (!pinnedLink.searchTopSite || options.includeSearchShortcuts)
              ) {
                // Remove any dupes from history.
                links = links.filter(
                  link =>
                    link.url != pinnedLink.url &&
                    (!options.onePerDomain ||
                      NewTabUtils.extractSite(link.url) !=
                        pinnedLink.baseDomain)
                );
                links.splice(index, 0, pinnedLink);
              }
            });
          }

          // Convert links to search shortcuts, if necessary.
          if (
            options.includeSearchShortcuts &&
            Services.prefs.getBoolPref(SHORTCUTS_PREF, false) &&
            !getNewtabSites
          ) {
            // Pinned shortcuts are already returned as searchTopSite links,
            // with a proper label and url. But certain non-pinned links may
            // also be promoted to search shortcuts; here we convert them.
            links = links.map(link => {
              let searchProvider = getSearchProvider(shortURL(link));
              if (searchProvider) {
                link.searchTopSite = true;
                link.label = searchProvider.keyword;
                link.url = searchProvider.url;
              }
              return link;
            });
          }

          // Because we may have added links, we must crop again.
          if (typeof options.limit == "number") {
            links = links.slice(0, options.limit);
          }

          const makeDataURI = url => url && ExtensionUtils.makeDataURI(url);

          return Promise.all(
            links.map(async link => ({
              type: link.searchTopSite ? "search" : "url",
              url: link.url,
              // The newtab page allows the user to set custom site titles, which
              // are stored in `label`, so prefer it.  Search top sites currently
              // don't have titles but `hostname` instead.
              title: link.label || link.title || link.hostname || "",
              // Default top sites don't have a favicon property.  Instead they
              // have tippyTopIcon, a 96x96pt image used on the newtab page.
              // We'll use it as the favicon for now, but ideally default top
              // sites would have real favicons.  Non-default top sites (i.e.,
              // those from the user's history) will have favicons.
              favicon: options.includeFavicon
                ? link.favicon || (await makeDataURI(link.tippyTopIcon)) || null
                : null,
            }))
          );
        },
      },
    };
  }
};
