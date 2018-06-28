/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ContentLinkHandler"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["Blob", "FileReader"]);

ChromeUtils.defineModuleGetter(this, "Feeds",
  "resource:///modules/Feeds.jsm");
ChromeUtils.defineModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");
ChromeUtils.defineModuleGetter(this, "DeferredTask",
  "resource://gre/modules/DeferredTask.jsm");
ChromeUtils.defineModuleGetter(this, "PromiseUtils",
  "resource://gre/modules/PromiseUtils.jsm");

const SIZES_TELEMETRY_ENUM = {
  NO_SIZES: 0,
  ANY: 1,
  DIMENSION: 2,
  INVALID: 3,
};

const FAVICON_PARSING_TIMEOUT = 100;
const FAVICON_RICH_ICON_MIN_WIDTH = 96;
const PREFERRED_WIDTH = 16;

// URL schemes that we don't want to load and convert to data URLs.
const LOCAL_FAVICON_SCHEMES = [
  "chrome",
  "about",
  "resource",
  "data",
];

const MAX_FAVICON_EXPIRATION = 7 * 24 * 60 * 60 * 1000;

const TYPE_ICO = "image/x-icon";
const TYPE_SVG = "image/svg+xml";

function promiseBlobAsDataURL(blob) {
  return new Promise((resolve, reject) => {
    let reader = new FileReader();
    reader.addEventListener("load", () => resolve(reader.result));
    reader.addEventListener("error", reject);
    reader.readAsDataURL(blob);
  });
}

function promiseBlobAsOctets(blob) {
  return new Promise((resolve, reject) => {
    let reader = new FileReader();
    reader.addEventListener("load", () => {
      resolve(Array.from(reader.result).map(c => c.charCodeAt(0)));
    });
    reader.addEventListener("error", reject);
    reader.readAsBinaryString(blob);
  });
}

class FaviconLoad {
  constructor(iconInfo) {
    this.buffers = [];
    this.icon = iconInfo;

    this.channel = NetUtil.newChannel({
      uri: iconInfo.iconUri,
      loadingNode: iconInfo.node,
      loadingPrincipal: iconInfo.node.nodePrincipal,
      triggeringPrincipal: iconInfo.node.nodePrincipal,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE_FAVICON,
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS |
                     Ci.nsILoadInfo.SEC_ALLOW_CHROME |
                     Ci.nsILoadInfo.SEC_DISALLOW_SCRIPT,
    });

    this.channel.loadFlags |= Ci.nsIRequest.LOAD_BACKGROUND;
    // Sometimes node is a document and sometimes it is an element. This is
    // the easiest single way to get to the load group in both those cases.
    this.channel.loadGroup = iconInfo.node.ownerGlobal.document.documentLoadGroup;

    if (Services.prefs.getBoolPref("network.http.tailing.enabled", true) &&
        this.channel instanceof Ci.nsIClassOfService) {
      this.channel.addClassFlags(Ci.nsIClassOfService.Tail | Ci.nsIClassOfService.Throttleable);
    }
  }

  load() {
    this._deferred = PromiseUtils.defer();

    try {
      this.channel.asyncOpen2(this);
    } catch (e) {
      this._deferred.reject(e);
    }

    return this._deferred.promise;
  }

  cancel() {
    this._deferred.reject(Components.Exception(`Favicon load from ${this.icon.iconUri.spec} was cancelled.`, Cr.NS_BINDING_ABORTED));
    this.channel.cancel(Cr.NS_BINDING_ABORTED);
  }

  onStartRequest(request, context) {
  }

  onDataAvailable(request, context, inputStream, offset, count) {
    let data = NetUtil.readInputStreamToString(inputStream, count);
    this.buffers.push(Uint8Array.from(data, c => c.charCodeAt(0)));
  }

  async onStopRequest(request, context, statusCode) {
    if (!Components.isSuccessCode(statusCode)) {
      // If the load was cancelled the promise will have been rejected then.
      if (statusCode != Cr.NS_BINDING_ABORTED) {
        this._deferred.reject(Components.Exception(`Favicon at "${this.icon.iconUri.spec}" failed to load.`, statusCode));
      }
      return;
    }

    if (this.channel instanceof Ci.nsIHttpChannel) {
      if (!this.channel.requestSucceeded) {
        this._deferred.reject(Components.Exception(`Favicon at "${this.icon.iconUri.spec}" failed to load: ${this.channel.responseStatusText}.`, Cr.NS_ERROR_FAILURE));
        return;
      }
    }

    // Attempt to get an expiration time from the cache.  If this fails, we'll
    // use this default.
    let expiration = Date.now() + MAX_FAVICON_EXPIRATION;

    // This stuff isn't available after onStopRequest returns (so don't start
    // any async operations before this!).
    if (this.channel instanceof Ci.nsICacheInfoChannel) {
      expiration = Math.min(this.channel.cacheTokenExpirationTime * 1000, expiration);
    }

    let type = this.channel.contentType;
    let blob = new Blob(this.buffers, { type });

    if (type != "image/svg+xml") {
      let octets = await promiseBlobAsOctets(blob);
      let sniffer = Cc["@mozilla.org/image/loader;1"].
                    createInstance(Ci.nsIContentSniffer);
      try {
        type = sniffer.getMIMETypeFromContent(this.channel, octets, octets.length);
      } catch (e) {
        this._deferred.reject(e);
        return;
      }

      if (!type) {
        this._deferred.reject(Components.Exception(`Favicon at "${this.icon.iconUri.spec}" did not match a known mimetype.`, Cr.NS_ERROR_FAILURE));
        return;
      }

      blob = blob.slice(0, blob.size, type);
    }

    let dataURL = await promiseBlobAsDataURL(blob);

    this._deferred.resolve({
      expiration,
      dataURL,
    });
  }
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
    uri = uri.mutate().setUserPass("").finalize();
  } catch (e) {
    // some URIs are immutable
  }
  return uri;
}

/**
 * Guess a type for an icon based on its declared type or file extension.
 */
function guessType(icon) {
  // No type with no icon
  if (!icon) {
    return "";
  }

  // Use the file extension to guess at a type we're interested in
  if (!icon.type) {
    let extension = icon.iconUri.filePath.split(".").pop();
    switch (extension) {
      case "ico":
        return TYPE_ICO;
      case "svg":
        return TYPE_SVG;
    }
  }

  // Fuzzily prefer the type or fall back to the declared type
  return icon.type == "image/vnd.microsoft.icon" ? TYPE_ICO : icon.type || "";
}

/*
 * Selects the best rich icon and tab icon from a list of IconInfo objects.
 *
 * @param {Document} document The document to select icons for.
 * @param {Array} iconInfos A list of IconInfo objects.
 * @param {integer} preferredWidth The preferred width for tab icons.
 */
function selectIcons(document, iconInfos, preferredWidth) {
  if (iconInfos.length == 0) {
    return {
      richIcon: null,
      tabIcon: null,
    };
  }

  let preferredIcon;
  let bestSizedIcon;
  // Other links with the "icon" tag are the default icons
  let defaultIcon;
  // Rich icons are either apple-touch or fluid icons, or the ones of the
  // dimension 96x96 or greater
  let largestRichIcon;

  for (let icon of iconInfos) {
    if (!icon.isRichIcon) {
      // First check for svg. If it's not available check for an icon with a
      // size adapt to the current resolution. If both are not available, prefer
      // ico files. When multiple icons are in the same set, the latest wins.
      if (guessType(icon) == TYPE_SVG) {
        preferredIcon = icon;
      } else if (icon.width == preferredWidth && guessType(preferredIcon) != TYPE_SVG) {
        preferredIcon = icon;
      } else if (guessType(icon) == TYPE_ICO && (!preferredIcon || guessType(preferredIcon) == TYPE_ICO)) {
        preferredIcon = icon;
      }

      // Check for an icon larger yet closest to preferredWidth, that can be
      // downscaled efficiently.
      if (icon.width >= preferredWidth &&
          (!bestSizedIcon || bestSizedIcon.width >= icon.width)) {
        bestSizedIcon = icon;
      }
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
  // 2. Set the preferred one if any, otherwise check if there's a better
  //    sized fit.
  // This order allows smaller icon frames to eventually override rich icon
  // frames.

  let tabIcon = null;
  if (preferredIcon) {
    tabIcon = preferredIcon;
  } else if (bestSizedIcon) {
    tabIcon = bestSizedIcon;
  } else if (defaultIcon) {
    tabIcon = defaultIcon;
  }

  return {
    richIcon: largestRichIcon,
    tabIcon
  };
}

function makeFaviconFromLink(aLink, aIsRichIcon) {
  let iconUri = getLinkIconURI(aLink);
  if (!iconUri)
    return null;

  // Extract the size type and width.
  let width = extractIconSize(aLink.sizes);

  return {
    iconUri,
    width,
    isRichIcon: aIsRichIcon,
    type: aLink.type,
    node: aLink,
  };
}

class IconLoader {
  constructor(chromeGlobal) {
    this.chromeGlobal = chromeGlobal;
  }

  async load(iconInfo) {
    if (this._loader) {
      this._loader.cancel();
    }

    if (LOCAL_FAVICON_SCHEMES.includes(iconInfo.iconUri.scheme)) {
      this.chromeGlobal.sendAsyncMessage("Link:SetIcon", {
        originalURL: iconInfo.iconUri.spec,
        canUseForTab: !iconInfo.isRichIcon,
        expiration: undefined,
        iconURL: iconInfo.iconUri.spec,
      });
      return;
    }

    try {
      this._loader = new FaviconLoad(iconInfo);
      let { dataURL, expiration } = await this._loader.load();
      this._loader = null;

      this.chromeGlobal.sendAsyncMessage("Link:SetIcon", {
        originalURL: iconInfo.iconUri.spec,
        canUseForTab: !iconInfo.isRichIcon,
        expiration,
        iconURL: dataURL,
      });
    } catch (e) {
      if (e.resultCode != Cr.NS_BINDING_ABORTED) {
        Cu.reportError(e);

        // Used mainly for tests currently.
        this.chromeGlobal.sendAsyncMessage("Link:SetFailedIcon", {
          originalURL: iconInfo.iconUri.spec,
          canUseForTab: !iconInfo.isRichIcon,
        });
      }
    }
  }

  cancel() {
    if (!this._loader) {
      return;
    }

    this._loader.cancel();
    this._loader = null;
  }
}

class ContentLinkHandler {
  constructor(chromeGlobal) {
    this.chromeGlobal = chromeGlobal;
    this.iconInfos = [];
    this.seenTabIcon = false;

    chromeGlobal.addEventListener("DOMLinkAdded", this);
    chromeGlobal.addEventListener("DOMLinkChanged", this);
    chromeGlobal.addEventListener("pageshow", this);
    chromeGlobal.addEventListener("pagehide", this);

    // For every page we attempt to find a rich icon and a tab icon. These
    // objects take care of the load process for each.
    this.richIconLoader = new IconLoader(chromeGlobal);
    this.tabIconLoader = new IconLoader(chromeGlobal);

    this.iconTask = new DeferredTask(() => this.loadIcons(), FAVICON_PARSING_TIMEOUT);
  }

  loadIcons() {
    let preferredWidth = PREFERRED_WIDTH * Math.ceil(this.chromeGlobal.content.devicePixelRatio);
    let { richIcon, tabIcon } = selectIcons(this.chromeGlobal.content.document,
                                            this.iconInfos, preferredWidth);
    this.iconInfos = [];

    if (richIcon) {
      this.richIconLoader.load(richIcon);
    }

    if (tabIcon) {
      this.tabIconLoader.load(tabIcon);
    }
  }

  addIcon(iconInfo) {
    if (!Services.prefs.getBoolPref("browser.chrome.site_icons", true)) {
      return;
    }

    if (!iconInfo.isRichIcon) {
      this.seenTabIcon = true;
    }
    this.iconInfos.push(iconInfo);
    this.iconTask.arm();
  }

  onPageShow(event) {
    if (event.target != this.chromeGlobal.content.document) {
      return;
    }

    if (!this.seenTabIcon && Services.prefs.getBoolPref("browser.chrome.guess_favicon", true)) {
      // Currently ImageDocuments will just load the default favicon, see bug
      // 403651 for discussion.

      // Inject the default icon. Use documentURIObject so that we do the right
      // thing with about:-style error pages. See bug 453442
      let baseURI = this.chromeGlobal.content.document.documentURIObject;
      if (baseURI.schemeIs("http") || baseURI.schemeIs("https")) {
        let iconUri = baseURI.mutate().setPathQueryRef("/favicon.ico").finalize();
        this.addIcon({
          iconUri,
          width: -1,
          isRichIcon: false,
          type: TYPE_ICO,
          node: this.chromeGlobal.content.document,
        });
      }
    }

    // We're likely done with icon parsing so load the pending icons now.
    if (this.iconTask.isArmed) {
      this.iconTask.disarm();
      this.loadIcons();
    }
  }

  onPageHide(event) {
    if (event.target != this.chromeGlobal.content.document) {
      return;
    }

    this.richIconLoader.cancel();
    this.tabIconLoader.cancel();

    this.iconTask.disarm();
    this.iconInfos = [];
    this.seenTabIcon = false;
  }

  onLinkEvent(event) {
    let link = event.target;
    // Ignore sub-frames (bugs 305472, 479408).
    if (link.ownerGlobal != this.chromeGlobal.content) {
      return;
    }

    let rel = link.rel && link.rel.toLowerCase();
    if (!rel || !link.href)
      return;

    // Note: following booleans only work for the current link, not for the
    // whole content
    let feedAdded = false;
    let iconAdded = false;
    let searchAdded = false;
    let rels = {};
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
              this.chromeGlobal.sendAsyncMessage("Link:AddFeed", {
                type: link.type,
                href: link.href,
                title: link.title,
              });
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
          if (iconAdded || link.hasAttribute("mask")) { // Masked icons are not supported yet.
            break;
          }

          let iconInfo = makeFaviconFromLink(link, isRichIcon);
          if (iconInfo) {
            iconAdded = this.addIcon(iconInfo);
          }
          break;
        case "search":
          if (Services.policies && !Services.policies.isAllowed("installSearchEngine")) {
            break;
          }

          if (!searchAdded && event.type == "DOMLinkAdded") {
            let type = link.type && link.type.toLowerCase();
            type = type.replace(/^\s+|\s*(?:;.*)?$/g, "");

            let re = /^(?:https?|ftp):/i;
            if (type == "application/opensearchdescription+xml" && link.title &&
                re.test(link.href)) {
              let engine = { title: link.title, href: link.href };
              this.chromeGlobal.sendAsyncMessage("Link:AddSearch", {
                engine,
                url: link.ownerDocument.documentURI,
              });
              searchAdded = true;
            }
          }
          break;
      }
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "pageshow":
        this.onPageShow(event);
        break;
      case "pagehide":
        this.onPageHide(event);
        break;
      default:
        this.onLinkEvent(event);
    }
  }
}
