/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  AboutNewTab: "resource:///modules/AboutNewTab.jsm",
  NewTabUtils: "resource://gre/modules/NewTabUtils.jsm",
  shortURL: "resource://activity-stream/lib/ShortURL.jsm",
  getSearchProvider: "resource://activity-stream/lib/SearchShortcuts.jsm",
});

const SHORTCUTS_PREF =
  "browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts";

this.topSites = class extends ExtensionAPI {
  getAPI(context) {
    return {
      topSites: {
        get: async function(options) {
          let links = options.newtab
            ? AboutNewTab.getTopSites()
            : await NewTabUtils.activityStreamLinks.getTopSites({
                ignoreBlocked: options.includeBlocked,
                onePerDomain: options.onePerDomain,
                numItems: options.limit,
                includeFavicon: options.includeFavicon,
              });

          if (options.includePinned && !options.newtab) {
            let pinnedLinks = NewTabUtils.pinnedLinks.links;
            if (options.includeFavicon) {
              pinnedLinks = NewTabUtils.activityStreamProvider._faviconBytesToDataURI(
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
            !options.newtab
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

          return links.map(link => {
            let newLink;
            if (link.searchTopSite) {
              newLink = {
                type: "search",
                url: link.url,
                title: link.label,
              };
            } else {
              newLink = {
                type: "url",
                url: link.url,
                title: link.title || link.hostname,
              };
            }
            // Default top sites don't have a favicon property.  Instead they
            // have tippyTopIcon, a 96x96pt image used on the newtab page.
            // We'll use it as the favicon for now, but ideally default top
            // sites would have real favicons.  Non-default top sites (i.e.,
            // those from the user's history) will have favicons.
            newLink.favicon = options.includeFavicon
              ? link.favicon || link.tippyTopIcon || null
              : null;
            return newLink;
          });
        },
      },
    };
  }
};
