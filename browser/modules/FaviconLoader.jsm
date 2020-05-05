/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["FaviconLoader"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["Blob", "FileReader"]);

ChromeUtils.defineModuleGetter(
  this,
  "DeferredTask",
  "resource://gre/modules/DeferredTask.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PromiseUtils",
  "resource://gre/modules/PromiseUtils.jsm"
);

const STREAM_SEGMENT_SIZE = 4096;
const PR_UINT32_MAX = 0xffffffff;

const BinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);
const StorageStream = Components.Constructor(
  "@mozilla.org/storagestream;1",
  "nsIStorageStream",
  "init"
);
const BufferedOutputStream = Components.Constructor(
  "@mozilla.org/network/buffered-output-stream;1",
  "nsIBufferedOutputStream",
  "init"
);

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
const LOCAL_FAVICON_SCHEMES = ["chrome", "about", "resource", "data"];

const MAX_FAVICON_EXPIRATION = 7 * 24 * 60 * 60 * 1000;
const MAX_ICON_SIZE = 2048;

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

function promiseImage(stream, type) {
  return new Promise((resolve, reject) => {
    let imgTools = Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools);

    imgTools.decodeImageAsync(
      stream,
      type,
      (image, result) => {
        if (!Components.isSuccessCode(result)) {
          reject();
          return;
        }

        resolve(image);
      },
      Services.tm.currentThread
    );
  });
}

class FaviconLoad {
  constructor(iconInfo) {
    this.icon = iconInfo;

    this.channel = Services.io.newChannelFromURI(
      iconInfo.iconUri,
      iconInfo.node,
      iconInfo.node.nodePrincipal,
      iconInfo.node.nodePrincipal,
      Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS |
        Ci.nsILoadInfo.SEC_ALLOW_CHROME |
        Ci.nsILoadInfo.SEC_DISALLOW_SCRIPT,
      Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE_FAVICON
    );

    this.channel.loadFlags |=
      Ci.nsIRequest.LOAD_BACKGROUND |
      Ci.nsIRequest.VALIDATE_NEVER |
      Ci.nsIRequest.LOAD_FROM_CACHE;
    // Sometimes node is a document and sometimes it is an element. This is
    // the easiest single way to get to the load group in both those cases.
    this.channel.loadGroup =
      iconInfo.node.ownerGlobal.document.documentLoadGroup;
    this.channel.notificationCallbacks = this;

    if (this.channel instanceof Ci.nsIHttpChannelInternal) {
      this.channel.blockAuthPrompt = true;
    }

    if (
      Services.prefs.getBoolPref("network.http.tailing.enabled", true) &&
      this.channel instanceof Ci.nsIClassOfService
    ) {
      this.channel.addClassFlags(
        Ci.nsIClassOfService.Tail | Ci.nsIClassOfService.Throttleable
      );
    }
  }

  load() {
    this._deferred = PromiseUtils.defer();

    // Clear the references when we succeed or fail.
    let cleanup = () => {
      this.channel = null;
      this.dataBuffer = null;
      this.stream = null;
    };
    this._deferred.promise.then(cleanup, cleanup);

    this.dataBuffer = new StorageStream(STREAM_SEGMENT_SIZE, PR_UINT32_MAX);

    // storage streams do not implement writeFrom so wrap it with a buffered stream.
    this.stream = new BufferedOutputStream(
      this.dataBuffer.getOutputStream(0),
      STREAM_SEGMENT_SIZE * 2
    );

    try {
      this.channel.asyncOpen(this);
    } catch (e) {
      this._deferred.reject(e);
    }

    return this._deferred.promise;
  }

  cancel() {
    if (!this.channel) {
      return;
    }

    this.channel.cancel(Cr.NS_BINDING_ABORTED);
  }

  onStartRequest(request) {}

  onDataAvailable(request, inputStream, offset, count) {
    this.stream.writeFrom(inputStream, count);
  }

  asyncOnChannelRedirect(oldChannel, newChannel, flags, callback) {
    if (oldChannel == this.channel) {
      this.channel = newChannel;
    }

    callback.onRedirectVerifyCallback(Cr.NS_OK);
  }

  async onStopRequest(request, statusCode) {
    if (request != this.channel) {
      // Indicates that a redirect has occurred. We don't care about the result
      // of the original channel.
      return;
    }

    this.stream.close();
    this.stream = null;

    if (!Components.isSuccessCode(statusCode)) {
      if (statusCode == Cr.NS_BINDING_ABORTED) {
        this._deferred.reject(
          Components.Exception(
            `Favicon load from ${this.icon.iconUri.spec} was cancelled.`,
            statusCode
          )
        );
      } else {
        this._deferred.reject(
          Components.Exception(
            `Favicon at "${this.icon.iconUri.spec}" failed to load.`,
            statusCode
          )
        );
      }
      return;
    }

    if (this.channel instanceof Ci.nsIHttpChannel) {
      if (!this.channel.requestSucceeded) {
        this._deferred.reject(
          Components.Exception(
            `Favicon at "${this.icon.iconUri.spec}" failed to load: ${this.channel.responseStatusText}.`,
            Cr.NS_ERROR_FAILURE
          )
        );
        return;
      }
    }

    // By default don't store icons added after "pageshow".
    let canStoreIcon = this.icon.beforePageShow;
    if (canStoreIcon) {
      // Don't store icons responding with Cache-Control: no-store.
      try {
        if (
          this.channel instanceof Ci.nsIHttpChannel &&
          this.channel.isNoStoreResponse()
        ) {
          canStoreIcon = false;
        }
      } catch (ex) {
        if (ex.result != Cr.NS_ERROR_NOT_AVAILABLE) {
          throw ex;
        }
      }
    }

    // Attempt to get an expiration time from the cache.  If this fails, we'll
    // use this default.
    let expiration = Date.now() + MAX_FAVICON_EXPIRATION;

    // This stuff isn't available after onStopRequest returns (so don't start
    // any async operations before this!).
    if (this.channel instanceof Ci.nsICacheInfoChannel) {
      try {
        expiration = Math.min(
          this.channel.cacheTokenExpirationTime * 1000,
          expiration
        );
      } catch (e) {
        // Ignore failures to get the expiration time.
      }
    }

    try {
      let stream = new BinaryInputStream(this.dataBuffer.newInputStream(0));
      let buffer = new ArrayBuffer(this.dataBuffer.length);
      stream.readArrayBuffer(buffer.byteLength, buffer);

      let type = this.channel.contentType;
      let blob = new Blob([buffer], { type });

      if (type != "image/svg+xml") {
        let octets = await promiseBlobAsOctets(blob);
        let sniffer = Cc["@mozilla.org/image/loader;1"].createInstance(
          Ci.nsIContentSniffer
        );
        type = sniffer.getMIMETypeFromContent(
          this.channel,
          octets,
          octets.length
        );

        if (!type) {
          throw Components.Exception(
            `Favicon at "${this.icon.iconUri.spec}" did not match a known mimetype.`,
            Cr.NS_ERROR_FAILURE
          );
        }

        blob = blob.slice(0, blob.size, type);

        let image;
        try {
          image = await promiseImage(this.dataBuffer.newInputStream(0), type);
        } catch (e) {
          throw Components.Exception(
            `Favicon at "${this.icon.iconUri.spec}" could not be decoded.`,
            Cr.NS_ERROR_FAILURE
          );
        }

        if (image.width > MAX_ICON_SIZE || image.height > MAX_ICON_SIZE) {
          throw Components.Exception(
            `Favicon at "${this.icon.iconUri.spec}" is too large.`,
            Cr.NS_ERROR_FAILURE
          );
        }
      }

      let dataURL = await promiseBlobAsDataURL(blob);

      this._deferred.resolve({
        expiration,
        dataURL,
        canStoreIcon,
      });
    } catch (e) {
      this._deferred.reject(e);
    }
  }

  getInterface(iid) {
    if (iid.equals(Ci.nsIChannelEventSink)) {
      return this;
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
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
  Services.telemetry
    .getHistogramById("LINK_ICON_SIZES_ATTR_USAGE")
    .add(sizesType);
  if (width > 0) {
    Services.telemetry
      .getHistogramById("LINK_ICON_SIZES_ATTR_DIMENSION")
      .add(width);
  }

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
    uri = uri
      .mutate()
      .setUserPass("")
      .finalize();
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
 * @param {Array} iconInfos A list of IconInfo objects.
 * @param {integer} preferredWidth The preferred width for tab icons.
 */
function selectIcons(iconInfos, preferredWidth) {
  if (!iconInfos.length) {
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
      } else if (
        icon.width == preferredWidth &&
        guessType(preferredIcon) != TYPE_SVG
      ) {
        preferredIcon = icon;
      } else if (
        guessType(icon) == TYPE_ICO &&
        (!preferredIcon || guessType(preferredIcon) == TYPE_ICO)
      ) {
        preferredIcon = icon;
      }

      // Check for an icon larger yet closest to preferredWidth, that can be
      // downscaled efficiently.
      if (
        icon.width >= preferredWidth &&
        (!bestSizedIcon || bestSizedIcon.width >= icon.width)
      ) {
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
    tabIcon,
  };
}

class IconLoader {
  constructor(actor) {
    this.actor = actor;
  }

  async load(iconInfo) {
    if (this._loader) {
      this._loader.cancel();
    }

    if (LOCAL_FAVICON_SCHEMES.includes(iconInfo.iconUri.scheme)) {
      // We need to do a manual security check because the channel won't do
      // it for us.
      try {
        Services.scriptSecurityManager.checkLoadURIWithPrincipal(
          iconInfo.node.nodePrincipal,
          iconInfo.iconUri,
          Services.scriptSecurityManager.ALLOW_CHROME
        );
      } catch (ex) {
        return;
      }
      this.actor.sendAsyncMessage("Link:SetIcon", {
        pageURL: iconInfo.pageUri.spec,
        originalURL: iconInfo.iconUri.spec,
        canUseForTab: !iconInfo.isRichIcon,
        expiration: undefined,
        iconURL: iconInfo.iconUri.spec,
        canStoreIcon: true,
      });
      return;
    }

    // Let the main process that a tab icon is possibly coming.
    this.actor.sendAsyncMessage("Link:LoadingIcon", {
      originalURL: iconInfo.iconUri.spec,
      canUseForTab: !iconInfo.isRichIcon,
    });

    try {
      this._loader = new FaviconLoad(iconInfo);
      let { dataURL, expiration, canStoreIcon } = await this._loader.load();

      this.actor.sendAsyncMessage("Link:SetIcon", {
        pageURL: iconInfo.pageUri.spec,
        originalURL: iconInfo.iconUri.spec,
        canUseForTab: !iconInfo.isRichIcon,
        expiration,
        iconURL: dataURL,
        canStoreIcon,
      });
    } catch (e) {
      if (e.result != Cr.NS_BINDING_ABORTED) {
        Cu.reportError(e);

        // Used mainly for tests currently.
        this.actor.sendAsyncMessage("Link:SetFailedIcon", {
          originalURL: iconInfo.iconUri.spec,
          canUseForTab: !iconInfo.isRichIcon,
        });
      }
    } finally {
      this._loader = null;
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

class FaviconLoader {
  constructor(actor) {
    this.actor = actor;
    this.iconInfos = [];

    // Icons added after onPageShow() are likely added by modifying <link> tags
    // through javascript; we want to avoid storing those permanently because
    // they are probably used to show badges, and many of them could be
    // randomly generated. This boolean can be used to track that case.
    this.beforePageShow = true;

    // For every page we attempt to find a rich icon and a tab icon. These
    // objects take care of the load process for each.
    this.richIconLoader = new IconLoader(actor);
    this.tabIconLoader = new IconLoader(actor);

    this.iconTask = new DeferredTask(
      () => this.loadIcons(),
      FAVICON_PARSING_TIMEOUT
    );
  }

  loadIcons() {
    // If the page is unloaded immediately after the DeferredTask's timer fires
    // we can still attempt to load icons, which will fail since the content
    // window is no longer available. Checking if iconInfos has been cleared
    // allows us to bail out early in this case.
    if (!this.iconInfos.length) {
      return;
    }

    let preferredWidth =
      PREFERRED_WIDTH * Math.ceil(this.actor.contentWindow.devicePixelRatio);
    let { richIcon, tabIcon } = selectIcons(this.iconInfos, preferredWidth);
    this.iconInfos = [];

    if (richIcon) {
      this.richIconLoader.load(richIcon);
    }

    if (tabIcon) {
      this.tabIconLoader.load(tabIcon);
    }
  }

  addIconFromLink(aLink, aIsRichIcon) {
    let iconInfo = makeFaviconFromLink(aLink, aIsRichIcon);
    if (iconInfo) {
      iconInfo.beforePageShow = this.beforePageShow;
      this.iconInfos.push(iconInfo);
      this.iconTask.arm();
      return true;
    }
    return false;
  }

  addDefaultIcon(pageUri) {
    // Currently ImageDocuments will just load the default favicon, see bug
    // 403651 for discussion.
    this.iconInfos.push({
      pageUri,
      iconUri: pageUri
        .mutate()
        .setPathQueryRef("/favicon.ico")
        .finalize(),
      width: -1,
      isRichIcon: false,
      type: TYPE_ICO,
      node: this.actor.document,
      beforePageShow: this.beforePageShow,
    });
    this.iconTask.arm();
  }

  onPageShow() {
    // We're likely done with icon parsing so load the pending icons now.
    if (this.iconTask.isArmed) {
      this.iconTask.disarm();
      this.loadIcons();
    }
    this.beforePageShow = false;
  }

  onPageHide() {
    this.richIconLoader.cancel();
    this.tabIconLoader.cancel();

    this.iconTask.disarm();
    this.iconInfos = [];
  }
}

function makeFaviconFromLink(aLink, aIsRichIcon) {
  let iconUri = getLinkIconURI(aLink);
  if (!iconUri) {
    return null;
  }

  // Extract the size type and width.
  let width = extractIconSize(aLink.sizes);

  return {
    pageUri: aLink.ownerDocument.documentURIObject,
    iconUri,
    width,
    isRichIcon: aIsRichIcon,
    type: aLink.type,
    node: aLink,
  };
}
