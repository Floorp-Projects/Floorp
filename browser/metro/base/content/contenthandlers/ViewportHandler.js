/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

dump("### ViewportHandler.js loaded\n");

// See getViewportMetadata. Blindly copied from Safari
// documentation for now.
const kViewportMinScale  = 0;
const kViewportMaxScale  = 10;
const kViewportMinWidth  = 200;
const kViewportMaxWidth  = 10000;
const kViewportMinHeight = 223;
const kViewportMaxHeight = 10000;

/*
 * ViewportHandler
 *
 * Plucks zoom info out of web page metadata and forwards it to the
 * browser.
 */ 

let ViewportHandler = {
  init: function init() {
    addEventListener("DOMWindowCreated", this, false);
    addEventListener("DOMMetaAdded", this, false);
    addEventListener("DOMContentLoaded", this, false);
    addEventListener("pageshow", this, false);
  },

  handleEvent: function handleEvent(aEvent) {
    let target = aEvent.originalTarget;
    let isRootDocument = (target == content.document || target.ownerDocument == content.document);
    if (!isRootDocument)
      return;

    switch (aEvent.type) {
      case "DOMWindowCreated":
        this.resetMetadata();
        break;

      case "DOMMetaAdded":
        if (target.name == "viewport")
          this.updateMetadata();
        break;

      case "DOMContentLoaded":
      case "pageshow":
        this.updateMetadata();
        break;
    }
  },

  resetMetadata: function resetMetadata() {
    sendAsyncMessage("Browser:ViewportMetadata", null);
  },

  updateMetadata: function updateMetadata() {
    sendAsyncMessage("Browser:ViewportMetadata", this.getViewportMetadata());
  },

  /**
   * Returns an object with the page's preferred viewport properties:
   *   defaultZoom (optional float): The initial scale when the page is loaded.
   *   minZoom (optional float): The minimum zoom level.
   *   maxZoom (optional float): The maximum zoom level.
   *   width (optional int): The CSS viewport width in px.
   *   height (optional int): The CSS viewport height in px.
   *   autoSize (boolean): Resize the CSS viewport when the window resizes.
   *   allowZoom (boolean): Let the user zoom in or out.
   *   autoScale (boolean): Adjust the viewport properties to account for display density.
   */
  getViewportMetadata: function getViewportMetadata() {
    let doctype = content.document.doctype;
    if (doctype && /(WAP|WML|Mobile)/.test(doctype.publicId))
      return { defaultZoom: 1, autoSize: true, allowZoom: true, autoScale: true };

    let windowUtils = Util.getWindowUtils(content);
    let handheldFriendly = windowUtils.getDocumentMetadata("HandheldFriendly");
    if (handheldFriendly == "true")
      return { defaultZoom: 1, autoSize: true, allowZoom: true, autoScale: true };

    if (content.document instanceof XULDocument)
      return { defaultZoom: 1, autoSize: true, allowZoom: false, autoScale: false };

    // HACK: Since we can't set the scale in local tabs (bug 597081), we force
    // them to device-width and scale=1 so they will lay out reasonably.
    if (Util.isLocalScheme(content.document.documentURI))
      return { defaultZoom: 1, autoSize: true, allowZoom: false, autoScale: false };

    // HACK: Since we can't set the scale correctly in frameset pages yet (bug 645756), we force
    // them to device-width and scale=1 so they will lay out reasonably.
    if (content.frames.length > 0 && (content.document.body instanceof HTMLFrameSetElement))
      return { defaultZoom: 1, autoSize: true, allowZoom: false, autoScale: false };

    // viewport details found here
    // http://developer.apple.com/safari/library/documentation/AppleApplications/Reference/SafariHTMLRef/Articles/MetaTags.html
    // http://developer.apple.com/safari/library/documentation/AppleApplications/Reference/SafariWebContent/UsingtheViewport/UsingtheViewport.html

    // Note: These values will be NaN if parseFloat or parseInt doesn't find a number.
    // Remember that NaN is contagious: Math.max(1, NaN) == Math.min(1, NaN) == NaN.
    let scale = parseFloat(windowUtils.getDocumentMetadata("viewport-initial-scale"));
    let minScale = parseFloat(windowUtils.getDocumentMetadata("viewport-minimum-scale"));
    let maxScale = parseFloat(windowUtils.getDocumentMetadata("viewport-maximum-scale"));

    let widthStr = windowUtils.getDocumentMetadata("viewport-width");
    let heightStr = windowUtils.getDocumentMetadata("viewport-height");
    let width = Util.clamp(parseInt(widthStr), kViewportMinWidth, kViewportMaxWidth);
    let height = Util.clamp(parseInt(heightStr), kViewportMinHeight, kViewportMaxHeight);

    let allowZoomStr = windowUtils.getDocumentMetadata("viewport-user-scalable");
    let allowZoom = !/^(0|no|false)$/.test(allowZoomStr); // WebKit allows 0, "no", or "false"

    scale = Util.clamp(scale, kViewportMinScale, kViewportMaxScale);
    minScale = Util.clamp(minScale, kViewportMinScale, kViewportMaxScale);
    maxScale = Util.clamp(maxScale, kViewportMinScale, kViewportMaxScale);

    // If initial scale is 1.0 and width is not set, assume width=device-width
    let autoSize = (widthStr == "device-width" ||
                    (!widthStr && (heightStr == "device-height" || scale == 1.0)));

    return {
      defaultZoom: scale,
      minZoom: minScale,
      maxZoom: maxScale,
      width: width,
      height: height,
      autoSize: autoSize,
      allowZoom: allowZoom,
      autoScale: true
    };
  }
};

ViewportHandler.init();

