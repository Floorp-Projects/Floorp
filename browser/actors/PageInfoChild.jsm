/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["PageInfoChild"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

class PageInfoChild extends JSWindowActorChild {
  async receiveMessage(message) {
    let window = this.contentWindow;
    let document = window.document;

    //Handles two different types of messages: one for general info (PageInfo:getData)
    //and one for media info (PageInfo:getMediaData)
    switch (message.name) {
      case "PageInfo:getData": {
        return Promise.resolve({
          metaViewRows: this.getMetaInfo(document),
          docInfo: this.getDocumentInfo(document),
          windowInfo: this.getWindowInfo(window),
        });
      }
      case "PageInfo:getMediaData": {
        return Promise.resolve({
          mediaItems: await this.getDocumentMedia(document),
        });
      }
    }

    return undefined;
  }

  getMetaInfo(document) {
    let metaViewRows = [];

    // Get the meta tags from the page.
    let metaNodes = document.getElementsByTagName("meta");

    for (let metaNode of metaNodes) {
      metaViewRows.push([
        metaNode.name ||
          metaNode.httpEquiv ||
          metaNode.getAttribute("property"),
        metaNode.content,
      ]);
    }

    return metaViewRows;
  }

  getWindowInfo(window) {
    let windowInfo = {};
    windowInfo.isTopWindow = window == window.top;

    let hostName = null;
    try {
      hostName = Services.io.newURI(window.location.href).displayHost;
    } catch (exception) {}

    windowInfo.hostName = hostName;
    return windowInfo;
  }

  getDocumentInfo(document) {
    let docInfo = {};
    docInfo.title = document.title;
    docInfo.location = document.location.toString();
    try {
      docInfo.location = Services.io.newURI(
        document.location.toString()
      ).displaySpec;
    } catch (exception) {}
    docInfo.referrer = document.referrer;
    try {
      if (document.referrer) {
        docInfo.referrer = Services.io.newURI(document.referrer).displaySpec;
      }
    } catch (exception) {}
    docInfo.compatMode = document.compatMode;
    docInfo.contentType = document.contentType;
    docInfo.characterSet = document.characterSet;
    docInfo.lastModified = document.lastModified;
    docInfo.principal = document.nodePrincipal;
    docInfo.cookieJarSettings = lazy.E10SUtils.serializeCookieJarSettings(
      document.cookieJarSettings
    );

    let documentURIObject = {};
    documentURIObject.spec = document.documentURIObject.spec;
    docInfo.documentURIObject = documentURIObject;

    docInfo.isContentWindowPrivate = lazy.PrivateBrowsingUtils.isContentWindowPrivate(
      document.ownerGlobal
    );

    return docInfo;
  }

  /**
   * Returns an array that stores all mediaItems found in the document
   * Calls getMediaItems for all nodes within the constructed tree walker and forms
   * resulting array.
   */
  async getDocumentMedia(document) {
    let nodeCount = 0;
    let content = document.ownerGlobal;
    let iterator = document.createTreeWalker(
      document,
      content.NodeFilter.SHOW_ELEMENT
    );

    let totalMediaItems = [];

    while (iterator.nextNode()) {
      let mediaItems = this.getMediaItems(document, iterator.currentNode);

      if (++nodeCount % 500 == 0) {
        // setTimeout every 500 elements so we don't keep blocking the content process.
        await new Promise(resolve => lazy.setTimeout(resolve, 10));
      }
      totalMediaItems.push(...mediaItems);
    }

    return totalMediaItems;
  }

  getMediaItems(document, elem) {
    // Check for images defined in CSS (e.g. background, borders)
    let computedStyle = elem.ownerGlobal.getComputedStyle(elem);
    // A node can have multiple media items associated with it - for example,
    // multiple background images.
    let mediaItems = [];
    let content = document.ownerGlobal;

    let addMedia = (url, type, alt, el, isBg, altNotProvided = false) => {
      let element = this.serializeElementInfo(document, url, el, isBg);
      mediaItems.push({
        url,
        type,
        alt,
        altNotProvided,
        element,
        isBg,
      });
    };

    if (computedStyle) {
      let addImgFunc = (type, urls) => {
        for (let url of urls) {
          addMedia(url, type, "", elem, true, true);
        }
      };
      // FIXME: This is missing properties. See the implementation of
      // getCSSImageURLs for a list of properties.
      //
      // If you don't care about the message you can also pass "all" here and
      // get all the ones the browser knows about.
      addImgFunc("bg-img", computedStyle.getCSSImageURLs("background-image"));
      addImgFunc(
        "border-img",
        computedStyle.getCSSImageURLs("border-image-source")
      );
      addImgFunc("list-img", computedStyle.getCSSImageURLs("list-style-image"));
      addImgFunc("cursor", computedStyle.getCSSImageURLs("cursor"));
    }

    // One swi^H^H^Hif-else to rule them all.
    if (content.HTMLImageElement.isInstance(elem)) {
      addMedia(
        elem.src,
        "img",
        elem.getAttribute("alt"),
        elem,
        false,
        !elem.hasAttribute("alt")
      );
    } else if (content.SVGImageElement.isInstance(elem)) {
      try {
        // Note: makeURLAbsolute will throw if either the baseURI is not a valid URI
        //       or the URI formed from the baseURI and the URL is not a valid URI.
        if (elem.href.baseVal) {
          let href = Services.io.newURI(
            elem.href.baseVal,
            null,
            Services.io.newURI(elem.baseURI)
          ).spec;
          addMedia(href, "img", "", elem, false);
        }
      } catch (e) {}
    } else if (content.HTMLVideoElement.isInstance(elem)) {
      addMedia(elem.currentSrc, "video", "", elem, false);
    } else if (content.HTMLAudioElement.isInstance(elem)) {
      addMedia(elem.currentSrc, "audio", "", elem, false);
    } else if (content.HTMLLinkElement.isInstance(elem)) {
      if (elem.rel && /\bicon\b/i.test(elem.rel)) {
        addMedia(elem.href, "link", "", elem, false);
      }
    } else if (
      content.HTMLInputElement.isInstance(elem) ||
      content.HTMLButtonElement.isInstance(elem)
    ) {
      if (elem.type.toLowerCase() == "image") {
        addMedia(
          elem.src,
          "input",
          elem.getAttribute("alt"),
          elem,
          false,
          !elem.hasAttribute("alt")
        );
      }
    } else if (content.HTMLObjectElement.isInstance(elem)) {
      addMedia(elem.data, "object", this.getValueText(elem), elem, false);
    } else if (content.HTMLEmbedElement.isInstance(elem)) {
      addMedia(elem.src, "embed", "", elem, false);
    }

    return mediaItems;
  }

  /**
   * Set up a JSON element object with all the instanceOf and other infomation that
   * makePreview in pageInfo.js uses to figure out how to display the preview.
   */

  serializeElementInfo(document, url, item, isBG) {
    let result = {};
    let content = document.ownerGlobal;

    let imageText;
    if (
      !isBG &&
      !content.SVGImageElement.isInstance(item) &&
      !content.ImageDocument.isInstance(document)
    ) {
      imageText = item.title || item.alt;

      if (!imageText && !content.HTMLImageElement.isInstance(item)) {
        imageText = this.getValueText(item);
      }
    }

    result.imageText = imageText;
    result.longDesc = item.longDesc;
    result.numFrames = 1;

    if (
      content.HTMLObjectElement.isInstance(item) ||
      content.HTMLEmbedElement.isInstance(item) ||
      content.HTMLLinkElement.isInstance(item)
    ) {
      result.mimeType = item.type;
    }

    if (
      !result.mimeType &&
      !isBG &&
      item instanceof Ci.nsIImageLoadingContent
    ) {
      // Interface for image loading content.
      let imageRequest = item.getRequest(
        Ci.nsIImageLoadingContent.CURRENT_REQUEST
      );
      if (imageRequest) {
        result.mimeType = imageRequest.mimeType;
        let image =
          !(imageRequest.imageStatus & imageRequest.STATUS_ERROR) &&
          imageRequest.image;
        if (image) {
          result.numFrames = image.numFrames;
        }
      }
    }

    // If we have a data url, get the MIME type from the url.
    if (!result.mimeType && url.startsWith("data:")) {
      let dataMimeType = /^data:(image\/[^;,]+)/i.exec(url);
      if (dataMimeType) {
        result.mimeType = dataMimeType[1].toLowerCase();
      }
    }

    result.HTMLLinkElement = content.HTMLLinkElement.isInstance(item);
    result.HTMLInputElement = content.HTMLInputElement.isInstance(item);
    result.HTMLImageElement = content.HTMLImageElement.isInstance(item);
    result.HTMLObjectElement = content.HTMLObjectElement.isInstance(item);
    result.SVGImageElement = content.SVGImageElement.isInstance(item);
    result.HTMLVideoElement = content.HTMLVideoElement.isInstance(item);
    result.HTMLAudioElement = content.HTMLAudioElement.isInstance(item);

    if (isBG) {
      // Items that are showing this image as a background
      // image might not necessarily have a width or height,
      // so we'll dynamically generate an image and send up the
      // natural dimensions.
      let img = content.document.createElement("img");
      img.src = url;
      result.naturalWidth = img.naturalWidth;
      result.naturalHeight = img.naturalHeight;
    } else if (!content.SVGImageElement.isInstance(item)) {
      // SVG items do not have integer values for height or width,
      // so we must handle them differently in order to correctly
      // serialize

      // Otherwise, we can use the current width and height
      // of the image.
      result.width = item.width;
      result.height = item.height;
    }

    if (content.SVGImageElement.isInstance(item)) {
      result.SVGImageElementWidth = item.width.baseVal.value;
      result.SVGImageElementHeight = item.height.baseVal.value;
    }

    result.baseURI = item.baseURI;

    return result;
  }

  // Other Misc Stuff
  // Modified from the Links Panel v2.3, http://segment7.net/mozilla/links/links.html
  // parse a node to extract the contents of the node
  getValueText(node) {
    let valueText = "";
    let content = node.ownerGlobal;

    // Form input elements don't generally contain information that is useful to our callers, so return nothing.
    if (
      content.HTMLInputElement.isInstance(node) ||
      content.HTMLSelectElement.isInstance(node) ||
      content.HTMLTextAreaElement.isInstance(node)
    ) {
      return valueText;
    }

    // Otherwise recurse for each child.
    let length = node.childNodes.length;

    for (let i = 0; i < length; i++) {
      let childNode = node.childNodes[i];
      let nodeType = childNode.nodeType;

      // Text nodes are where the goods are.
      if (nodeType == content.Node.TEXT_NODE) {
        valueText += " " + childNode.nodeValue;
      } else if (nodeType == content.Node.ELEMENT_NODE) {
        // And elements can have more text inside them.
        // Images are special, we want to capture the alt text as if the image weren't there.
        if (content.HTMLImageElement.isInstance(childNode)) {
          valueText += " " + this.getAltText(childNode);
        } else {
          valueText += " " + this.getValueText(childNode);
        }
      }
    }

    return this.stripWS(valueText);
  }

  // Copied from the Links Panel v2.3, http://segment7.net/mozilla/links/links.html.
  // Traverse the tree in search of an img or area element and grab its alt tag.
  getAltText(node) {
    let altText = "";

    if (node.alt) {
      return node.alt;
    }
    let length = node.childNodes.length;
    for (let i = 0; i < length; i++) {
      if ((altText = this.getAltText(node.childNodes[i]) != undefined)) {
        // stupid js warning...
        return altText;
      }
    }
    return "";
  }

  // Copied from the Links Panel v2.3, http://segment7.net/mozilla/links/links.html.
  // Strip leading and trailing whitespace, and replace multiple consecutive whitespace characters with a single space.
  stripWS(text) {
    let middleRE = /\s+/g;
    let endRE = /(^\s+)|(\s+$)/g;

    text = text.replace(middleRE, " ");
    return text.replace(endRE, "");
  }
}
