/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

this.EXPORTED_SYMBOLS = [ "ContentLinkHandler" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Feeds",
  "resource:///modules/Feeds.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm");

const SIZES_TELEMETRY_ENUM = {
  NO_SIZES: 0,
  ANY: 1,
  DIMENSION: 2,
  INVALID: 3,
};

const FAVICON_PARSING_TIMEOUT = 100;
const FAVICON_RICH_ICON_MIN_WIDTH = 96;

/*
 * Create a nsITimer.
 *
 * @param {function} aCallback A timeout callback function.
 * @param {Number} aDelay A timeout interval in millisecond.
 * @return {nsITimer} A nsITimer object.
 */
function setTimeout(aCallback, aDelay) {
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(aCallback, aDelay, Ci.nsITimer.TYPE_ONE_SHOT);
  return timer;
}

/*
 * Extract the icon width from the size attribute. It also sends the telemetry
 * about the size type and size dimension info.
 *
 * @param {Array} aSizes An array of strings about size.
 * @return {Number} A width of the icon in pixel.
 */
function extractIconSize(aSizes) {
  let width = -1;
  let sizesType;
  const re = /^([1-9][0-9]*)x[1-9][0-9]*$/i;

  if (aSizes.length) {
    for (let size of aSizes) {
      if (size.toLowerCase() == "any") {
        sizesType = SIZES_TELEMETRY_ENUM.ANY;
        break;
      } else {
        let values = re.exec(size);
        if (values && values.length > 1) {
          sizesType = SIZES_TELEMETRY_ENUM.DIMENSION;
          width = parseInt(values[1]);
          break;
        } else {
          sizesType = SIZES_TELEMETRY_ENUM.INVALID;
          break;
        }
      }
    }
  } else {
    sizesType = SIZES_TELEMETRY_ENUM.NO_SIZES;
  }

  // Telemetry probes for measuring the sizes attribute
  // usage and available dimensions.
  Services.telemetry.getHistogramById("LINK_ICON_SIZES_ATTR_USAGE").add(sizesType);
  if (width > 0)
    Services.telemetry.getHistogramById("LINK_ICON_SIZES_ATTR_DIMENSION").add(width);

  return width;
}

/*
 * Get link icon URI from a link dom node.
 *
 * @param {DOMNode} aLink A link dom node.
 * @return {nsIURI} A uri of the icon.
 */
function getLinkIconURI(aLink) {
  let targetDoc = aLink.ownerDocument;
  let uri = Services.io.newURI(aLink.href, targetDoc.characterSet);
  try {
    uri.userPass = "";
  } catch (e) {
    // some URIs are immutable
  }
  return uri;
}

/*
 * Set the icon via sending the "Link:Seticon" message.
 *
 * @param {Object} aIconInfo The IconInfo object looks like {
 *   iconUri: icon URI,
 *   loadingPrincipal: icon loading principal
 * }.
 * @param {Object} aChromeGlobal A global chrome object.
 */
function setIconForLink(aIconInfo, aChromeGlobal) {
  aChromeGlobal.sendAsyncMessage(
    "Link:SetIcon",
    { url: aIconInfo.iconUri.spec,
      loadingPrincipal: aIconInfo.loadingPrincipal,
      requestContextID: aIconInfo.requestContextID,
      canUseForTab: !aIconInfo.isRichIcon,
    });
}

/*
 * Timeout callback function for loading favicon.
 *
 * @param {Map} aFaviconLoads A map of page URL and FaviconLoad object pairs,
 *   where the FaviconLoad object looks like {
 *     timer: a nsITimer object,
 *     iconInfos: an array of IconInfo objects
 *   }
 * @param {String} aPageUrl A page URL string for this callback.
 * @param {Object} aChromeGlobal A global chrome object.
 */
function faviconTimeoutCallback(aFaviconLoads, aPageUrl, aChromeGlobal) {
  let load = aFaviconLoads.get(aPageUrl);
  if (!load)
    return;

  // SVG and ico are the preferred icons
  let preferredIcon;
  // Other links with the "icon" tag are the default icons
  let defaultIcon;
  // Rich icons are either apple-touch or fluid icons, or the ones of the
  // dimension 96x96 or greater
  let largestRichIcon;

  for (let icon of load.iconInfos) {
    if (icon.type === "image/svg+xml" ||
      icon.type === "image/x-icon" ||
      icon.type === "image/vnd.microsoft.icon") {
      preferredIcon = icon;
      continue;
    }

    // Note that some sites use hi-res icons without specifying them as
    // apple-touch or fluid icons.
    if (icon.isRichIcon || icon.width >= FAVICON_RICH_ICON_MIN_WIDTH) {
      if (!largestRichIcon || largestRichIcon.width < icon.width) {
        largestRichIcon = icon;
      }
    } else {
      defaultIcon = icon;
    }
  }

  // Now set the favicons for the page in the following order:
  // 1. Set the best rich icon if any.
  // 2. Set the preferred one if any, otherwise use the default one.
  // This order allows smaller icon frames to eventually override rich icon
  // frames.
  if (largestRichIcon) {
    setIconForLink(largestRichIcon, aChromeGlobal);
  }
  if (preferredIcon) {
    setIconForLink(preferredIcon, aChromeGlobal);
  } else if (defaultIcon) {
    setIconForLink(defaultIcon, aChromeGlobal);
  }

  load.timer = null;
  aFaviconLoads.delete(aPageUrl);
}

/*
 * Get request context ID of the link dom node's document.
 *
 * @param {DOMNode} aLink A link dom node.
 * @return {Number} The request context ID.
 *                  Return null when document's load group is not available.
 */
function getLinkRequestContextID(aLink) {
  try {
    return aLink.ownerDocument.documentLoadGroup.requestContextID;
  } catch (e) {
    return null;
  }
}

/*
 * Favicon link handler.
 *
 * @param {DOMNode} aLink A link dom node.
 * @param {bool} aIsRichIcon A bool to indicate if the link is rich icon.
 * @param {Object} aChromeGlobal A global chrome object.
 * @param {Map} aFaviconLoads A map of page URL and FaviconLoad object pairs.
 * @return {bool} Returns true if the link is successfully handled.
 */
function handleFaviconLink(aLink, aIsRichIcon, aChromeGlobal, aFaviconLoads) {
  let pageUrl = aLink.ownerDocument.documentURI;
  let iconUri = getLinkIconURI(aLink);
  if (!iconUri)
    return false;

  // Extract the size type and width.
  let width = extractIconSize(aLink.sizes);
  let iconInfo = {
    iconUri,
    width,
    isRichIcon: aIsRichIcon,
    type: aLink.type,
    loadingPrincipal: aLink.ownerDocument.nodePrincipal,
    requestContextID: getLinkRequestContextID(aLink)
  };

  if (aFaviconLoads.has(pageUrl)) {
    let load = aFaviconLoads.get(pageUrl);
    load.iconInfos.push(iconInfo);
    // Re-initialize the timer
    load.timer.delay = FAVICON_PARSING_TIMEOUT;
  } else {
    let timer = setTimeout(() => faviconTimeoutCallback(aFaviconLoads, pageUrl, aChromeGlobal),
                                                        FAVICON_PARSING_TIMEOUT);
    let load = { timer, iconInfos: [iconInfo] };
    aFaviconLoads.set(pageUrl, load);
  }
  return true;
}

this.ContentLinkHandler = {
  init(chromeGlobal) {
    const faviconLoads = new Map();
    chromeGlobal.addEventListener("DOMLinkAdded", event => {
      this.onLinkEvent(event, chromeGlobal, faviconLoads);
    });
    chromeGlobal.addEventListener("DOMLinkChanged", event => {
      this.onLinkEvent(event, chromeGlobal, faviconLoads);
    });
    chromeGlobal.addEventListener("unload", event => {
      for (const [pageUrl, load] of faviconLoads) {
        load.timer.cancel();
        load.timer = null;
        faviconLoads.delete(pageUrl);
      }
    });
  },

  onLinkEvent(event, chromeGlobal, faviconLoads) {
    var link = event.originalTarget;
    var rel = link.rel && link.rel.toLowerCase();
    if (!link || !link.ownerDocument || !rel || !link.href)
      return;

    // Ignore sub-frames (bugs 305472, 479408).
    let window = link.ownerGlobal;
    if (window != window.top)
      return;

    // Note: following booleans only work for the current link, not for the
    // whole content
    var feedAdded = false;
    var iconAdded = false;
    var searchAdded = false;
    var rels = {};
    for (let relString of rel.split(/\s+/))
      rels[relString] = true;

    for (let relVal in rels) {
      let isRichIcon = true;

      switch (relVal) {
        case "feed":
        case "alternate":
          if (!feedAdded && event.type == "DOMLinkAdded") {
            if (!rels.feed && rels.alternate && rels.stylesheet)
              break;

            if (Feeds.isValidFeed(link, link.ownerDocument.nodePrincipal, "feed" in rels)) {
              chromeGlobal.sendAsyncMessage("Link:AddFeed",
                                            {type: link.type,
                                             href: link.href,
                                             title: link.title});
              feedAdded = true;
            }
          }
          break;
        case "icon":
          isRichIcon = false;
          // Fall through to rich icon handling
        case "apple-touch-icon":
        case "apple-touch-icon-precomposed":
        case "fluid-icon":
          if (link.hasAttribute("mask") || // Masked icons are not supported yet.
              iconAdded ||
              !Services.prefs.getBoolPref("browser.chrome.site_icons")) {
            break;
          }

          iconAdded = handleFaviconLink(link, isRichIcon, chromeGlobal, faviconLoads);
          break;
        case "search":
          if (!searchAdded && event.type == "DOMLinkAdded") {
            var type = link.type && link.type.toLowerCase();
            type = type.replace(/^\s+|\s*(?:;.*)?$/g, "");

            let re = /^(?:https?|ftp):/i;
            if (type == "application/opensearchdescription+xml" && link.title &&
                re.test(link.href)) {
              let engine = { title: link.title, href: link.href };
              chromeGlobal.sendAsyncMessage("Link:AddSearch",
                                            {engine,
                                             url: link.ownerDocument.documentURI});
              searchAdded = true;
            }
          }
          break;
      }
    }
  },
};
