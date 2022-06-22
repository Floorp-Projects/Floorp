/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ContextMenuChild"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  SpellCheckHelper: "resource://gre/modules/InlineSpellChecker.jsm",
  LoginManagerChild: "resource://gre/modules/LoginManagerChild.jsm",
  WebNavigationFrames: "resource://gre/modules/WebNavigationFrames.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  SelectionUtils: "resource://gre/modules/SelectionUtils.jsm",
  InlineSpellCheckerContent:
    "resource://gre/modules/InlineSpellCheckerContent.jsm",
  ContentDOMReference: "resource://gre/modules/ContentDOMReference.jsm",
});

let contextMenus = new WeakMap();

class ContextMenuChild extends JSWindowActorChild {
  // PUBLIC
  constructor() {
    super();

    this.target = null;
    this.context = null;
    this.lastMenuTarget = null;
  }

  static getTarget(browsingContext, message, key) {
    let actor = contextMenus.get(browsingContext);
    if (!actor) {
      throw new Error(
        "Can't find ContextMenu actor for browsing context with " +
          "ID: " +
          browsingContext.id
      );
    }
    return actor.getTarget(message, key);
  }

  static getLastTarget(browsingContext) {
    let contextMenu = contextMenus.get(browsingContext);
    return contextMenu && contextMenu.lastMenuTarget;
  }

  receiveMessage(message) {
    switch (message.name) {
      case "ContextMenu:GetFrameTitle": {
        let target = lazy.ContentDOMReference.resolve(
          message.data.targetIdentifier
        );
        return Promise.resolve(target.ownerDocument.title);
      }

      case "ContextMenu:Canvas:ToBlobURL": {
        let target = lazy.ContentDOMReference.resolve(
          message.data.targetIdentifier
        );
        return new Promise(resolve => {
          target.toBlob(blob => {
            let blobURL = URL.createObjectURL(blob);
            resolve(blobURL);
          });
        });
      }

      case "ContextMenu:Hiding": {
        this.context = null;
        this.target = null;
        break;
      }

      case "ContextMenu:MediaCommand": {
        lazy.E10SUtils.wrapHandlingUserInput(
          this.contentWindow,
          message.data.handlingUserInput,
          () => {
            let media = lazy.ContentDOMReference.resolve(
              message.data.targetIdentifier
            );

            switch (message.data.command) {
              case "play":
                media.play();
                break;
              case "pause":
                media.pause();
                break;
              case "loop":
                media.loop = !media.loop;
                break;
              case "mute":
                media.muted = true;
                break;
              case "unmute":
                media.muted = false;
                break;
              case "playbackRate":
                media.playbackRate = message.data.data;
                break;
              case "hidecontrols":
                media.removeAttribute("controls");
                break;
              case "showcontrols":
                media.setAttribute("controls", "true");
                break;
              case "fullscreen":
                if (this.document.fullscreenEnabled) {
                  media.requestFullscreen();
                }
                break;
              case "pictureinpicture":
                Services.telemetry.keyedScalarAdd(
                  "pictureinpicture.opened_method",
                  "contextmenu",
                  1
                );
                let args = {
                  method: "contextMenu",
                  firstTimeToggle: (!Services.prefs.getBoolPref(
                    "media.videocontrols.picture-in-picture.video-toggle.has-used"
                  )).toString(),
                };
                Services.telemetry.recordEvent(
                  "pictureinpicture",
                  "opened_method",
                  "method",
                  null,
                  args
                );
                let event = new this.contentWindow.CustomEvent(
                  "MozTogglePictureInPicture",
                  {
                    bubbles: true,
                  },
                  this.contentWindow
                );
                media.dispatchEvent(event);
                break;
            }
          }
        );
        break;
      }

      case "ContextMenu:ReloadFrame": {
        let target = lazy.ContentDOMReference.resolve(
          message.data.targetIdentifier
        );
        target.ownerDocument.location.reload(message.data.forceReload);
        break;
      }

      case "ContextMenu:GetImageText": {
        let img = lazy.ContentDOMReference.resolve(
          message.data.targetIdentifier
        );
        img.recognizeCurrentImageText();
        break;
      }

      case "ContextMenu:ToggleRevealPassword": {
        let target = lazy.ContentDOMReference.resolve(
          message.data.targetIdentifier
        );
        target.revealPassword = !target.revealPassword;
        break;
      }

      case "ContextMenu:ReloadImage": {
        let image = lazy.ContentDOMReference.resolve(
          message.data.targetIdentifier
        );

        if (image instanceof Ci.nsIImageLoadingContent) {
          image.forceReload();
        }
        break;
      }

      case "ContextMenu:SearchFieldBookmarkData": {
        let node = lazy.ContentDOMReference.resolve(
          message.data.targetIdentifier
        );
        let charset = node.ownerDocument.characterSet;
        let formBaseURI = Services.io.newURI(node.form.baseURI, charset);
        let formURI = Services.io.newURI(
          node.form.getAttribute("action"),
          charset,
          formBaseURI
        );
        let spec = formURI.spec;
        let isURLEncoded =
          node.form.method.toUpperCase() == "POST" &&
          (node.form.enctype == "application/x-www-form-urlencoded" ||
            node.form.enctype == "");
        let title = node.ownerDocument.title;

        function escapeNameValuePair([aName, aValue]) {
          if (isURLEncoded) {
            return escape(aName + "=" + aValue);
          }

          return escape(aName) + "=" + escape(aValue);
        }
        let formData = new this.contentWindow.FormData(node.form);
        formData.delete(node.name);
        formData = Array.from(formData).map(escapeNameValuePair);
        formData.push(
          escape(node.name) + (isURLEncoded ? escape("=%s") : "=%s")
        );

        let postData;

        if (isURLEncoded) {
          postData = formData.join("&");
        } else {
          let separator = spec.includes("?") ? "&" : "?";
          spec += separator + formData.join("&");
        }

        return Promise.resolve({ spec, title, postData, charset });
      }

      case "ContextMenu:SaveVideoFrameAsImage": {
        let video = lazy.ContentDOMReference.resolve(
          message.data.targetIdentifier
        );
        let canvas = this.document.createElementNS(
          "http://www.w3.org/1999/xhtml",
          "canvas"
        );
        canvas.width = video.videoWidth;
        canvas.height = video.videoHeight;

        let ctxDraw = canvas.getContext("2d");
        ctxDraw.drawImage(video, 0, 0);

        // Note: if changing the content type, don't forget to update
        // consumers that also hardcode this content type.
        return Promise.resolve(canvas.toDataURL("image/jpeg", ""));
      }

      case "ContextMenu:SetAsDesktopBackground": {
        let target = lazy.ContentDOMReference.resolve(
          message.data.targetIdentifier
        );

        // Paranoia: check disableSetDesktopBackground again, in case the
        // image changed since the context menu was initiated.
        let disable = this._disableSetDesktopBackground(target);

        if (!disable) {
          try {
            Services.scriptSecurityManager.checkLoadURIWithPrincipal(
              target.ownerDocument.nodePrincipal,
              target.currentURI
            );
            let canvas = this.document.createElement("canvas");
            canvas.width = target.naturalWidth;
            canvas.height = target.naturalHeight;
            let ctx = canvas.getContext("2d");
            ctx.drawImage(target, 0, 0);
            let dataURL = canvas.toDataURL();
            let url = new URL(target.ownerDocument.location.href).pathname;
            let imageName = url.substr(url.lastIndexOf("/") + 1);
            return Promise.resolve({ failed: false, dataURL, imageName });
          } catch (e) {
            Cu.reportError(e);
          }
        }

        return Promise.resolve({
          failed: true,
          dataURL: null,
          imageName: null,
        });
      }
    }

    return undefined;
  }

  /**
   * Returns the event target of the context menu, using a locally stored
   * reference if possible. If not, and aMessage.objects is defined,
   * aMessage.objects[aKey] is returned. Otherwise null.
   * @param  {Object} aMessage Message with a objects property
   * @param  {String} aKey     Key for the target on aMessage.objects
   * @return {Object}          Context menu target
   */
  getTarget(aMessage, aKey = "target") {
    return this.target || (aMessage.objects && aMessage.objects[aKey]);
  }

  // PRIVATE
  _isXULTextLinkLabel(aNode) {
    const XUL_NS =
      "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    return (
      aNode.namespaceURI == XUL_NS &&
      aNode.tagName == "label" &&
      aNode.classList.contains("text-link") &&
      aNode.href
    );
  }

  // Generate fully qualified URL for clicked-on link.
  _getLinkURL() {
    let href = this.context.link.href;

    if (href) {
      // Handle SVG links:
      if (typeof href == "object" && href.animVal) {
        return this._makeURLAbsolute(this.context.link.baseURI, href.animVal);
      }

      return href;
    }

    href =
      this.context.link.getAttribute("href") ||
      this.context.link.getAttributeNS("http://www.w3.org/1999/xlink", "href");

    if (!href || !href.match(/\S/)) {
      // Without this we try to save as the current doc,
      // for example, HTML case also throws if empty
      throw new Error("Empty href");
    }

    return this._makeURLAbsolute(this.context.link.baseURI, href);
  }

  _getLinkURI() {
    try {
      return Services.io.newURI(this.context.linkURL);
    } catch (ex) {
      // e.g. empty URL string
    }

    return null;
  }

  // Get text of link.
  _getLinkText() {
    let text = this._gatherTextUnder(this.context.link);

    if (!text || !text.match(/\S/)) {
      text = this.context.link.getAttribute("title");
      if (!text || !text.match(/\S/)) {
        text = this.context.link.getAttribute("alt");
        if (!text || !text.match(/\S/)) {
          text = this.context.linkURL;
        }
      }
    }

    return text;
  }

  _getLinkProtocol() {
    if (this.context.linkURI) {
      return this.context.linkURI.scheme; // can be |undefined|
    }

    return null;
  }

  // Returns true if clicked-on link targets a resource that can be saved.
  _isLinkSaveable(aLink) {
    // We don't do the Right Thing for news/snews yet, so turn them off
    // until we do.
    return (
      this.context.linkProtocol &&
      !(
        this.context.linkProtocol == "mailto" ||
        this.context.linkProtocol == "tel" ||
        this.context.linkProtocol == "javascript" ||
        this.context.linkProtocol == "news" ||
        this.context.linkProtocol == "snews"
      )
    );
  }

  // Gather all descendent text under given document node.
  _gatherTextUnder(root) {
    let text = "";
    let node = root.firstChild;
    let depth = 1;
    while (node && depth > 0) {
      // See if this node is text.
      if (node.nodeType == node.TEXT_NODE) {
        // Add this text to our collection.
        text += " " + node.data;
      } else if (this.contentWindow.HTMLImageElement.isInstance(node)) {
        // If it has an "alt" attribute, add that.
        let altText = node.getAttribute("alt");
        if (altText && altText != "") {
          text += " " + altText;
        }
      }
      // Find next node to test.
      // First, see if this node has children.
      if (node.hasChildNodes()) {
        // Go to first child.
        node = node.firstChild;
        depth++;
      } else {
        // No children, try next sibling (or parent next sibling).
        while (depth > 0 && !node.nextSibling) {
          node = node.parentNode;
          depth--;
        }
        if (node.nextSibling) {
          node = node.nextSibling;
        }
      }
    }

    // Strip leading and tailing whitespace.
    text = text.trim();
    // Compress remaining whitespace.
    text = text.replace(/\s+/g, " ");
    return text;
  }

  // Returns a "url"-type computed style attribute value, with the url() stripped.
  _getComputedURL(aElem, aProp) {
    let urls = aElem.ownerGlobal.getComputedStyle(aElem).getCSSImageURLs(aProp);

    if (!urls.length) {
      return null;
    }

    if (urls.length != 1) {
      throw new Error("found multiple URLs");
    }

    return urls[0];
  }

  _makeURLAbsolute(aBase, aUrl) {
    return Services.io.newURI(aUrl, null, Services.io.newURI(aBase)).spec;
  }

  _isProprietaryDRM() {
    return (
      this.context.target.isEncrypted &&
      this.context.target.mediaKeys &&
      this.context.target.mediaKeys.keySystem != "org.w3.clearkey"
    );
  }

  _isMediaURLReusable(aURL) {
    if (aURL.startsWith("blob:")) {
      return URL.isValidURL(aURL);
    }

    return true;
  }

  _isTargetATextBox(node) {
    if (this.contentWindow.HTMLInputElement.isInstance(node)) {
      return node.mozIsTextField(false);
    }

    return this.contentWindow.HTMLTextAreaElement.isInstance(node);
  }

  /**
   * Check if we are in the parent process and the current iframe is the RDM iframe.
   */
  _isTargetRDMFrame(node) {
    return (
      Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT &&
      node.tagName === "iframe" &&
      node.hasAttribute("mozbrowser")
    );
  }

  _isSpellCheckEnabled(aNode) {
    // We can always force-enable spellchecking on textboxes
    if (this._isTargetATextBox(aNode)) {
      return true;
    }

    // We can never spell check something which is not content editable
    let editable = aNode.isContentEditable;

    if (!editable && aNode.ownerDocument) {
      editable = aNode.ownerDocument.designMode == "on";
    }

    if (!editable) {
      return false;
    }

    // Otherwise make sure that nothing in the parent chain disables spellchecking
    return aNode.spellcheck;
  }

  _disableSetDesktopBackground(aTarget) {
    // Disable the Set as Desktop Background menu item if we're still trying
    // to load the image or the load failed.
    if (!(aTarget instanceof Ci.nsIImageLoadingContent)) {
      return true;
    }

    if ("complete" in aTarget && !aTarget.complete) {
      return true;
    }

    if (aTarget.currentURI.schemeIs("javascript")) {
      return true;
    }

    let request = aTarget.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);

    if (!request) {
      return true;
    }

    return false;
  }

  async handleEvent(aEvent) {
    contextMenus.set(this.browsingContext, this);

    let defaultPrevented = aEvent.defaultPrevented;

    if (
      // If the event is not from a chrome-privileged document, and if
      // `dom.event.contextmenu.enabled` is false, force defaultPrevented=false.
      !aEvent.composedTarget.nodePrincipal.isSystemPrincipal &&
      !Services.prefs.getBoolPref("dom.event.contextmenu.enabled")
    ) {
      defaultPrevented = false;
    }

    if (defaultPrevented) {
      return;
    }

    if (this._isTargetRDMFrame(aEvent.composedTarget)) {
      // The target is in the DevTools RDM iframe, a proper context menu event
      // will be created from the RDM browser.
      return;
    }

    let doc = aEvent.composedTarget.ownerDocument;
    let {
      mozDocumentURIIfNotForErrorPages: docLocation,
      characterSet: charSet,
      baseURI,
      cookieJarSettings,
    } = doc;
    docLocation = docLocation && docLocation.spec;
    let frameID = lazy.WebNavigationFrames.getFrameId(doc.defaultView);
    let frameBrowsingContextID = doc.defaultView.docShell.browsingContext.id;
    const loginManagerChild = lazy.LoginManagerChild.forWindow(doc.defaultView);
    const docState = loginManagerChild.stateForDocument(doc);
    const loginFillInfo = docState.getFieldContext(aEvent.composedTarget);

    let disableSetDesktopBackground = null;

    // Media related cache info parent needs for saving
    let contentType = null;
    let contentDisposition = null;
    if (
      aEvent.composedTarget.nodeType == aEvent.composedTarget.ELEMENT_NODE &&
      aEvent.composedTarget instanceof Ci.nsIImageLoadingContent &&
      aEvent.composedTarget.currentURI
    ) {
      disableSetDesktopBackground = this._disableSetDesktopBackground(
        aEvent.composedTarget
      );

      try {
        let imageCache = Cc["@mozilla.org/image/tools;1"]
          .getService(Ci.imgITools)
          .getImgCacheForDocument(doc);
        // The image cache's notion of where this image is located is
        // the currentURI of the image loading content.
        let props = imageCache.findEntryProperties(
          aEvent.composedTarget.currentURI,
          doc
        );

        try {
          contentType = props.get("type", Ci.nsISupportsCString).data;
        } catch (e) {}

        try {
          contentDisposition = props.get(
            "content-disposition",
            Ci.nsISupportsCString
          ).data;
        } catch (e) {}
      } catch (e) {}
    }

    let selectionInfo = lazy.SelectionUtils.getSelectionDetails(
      this.contentWindow
    );
    let loadContext = this.docShell.QueryInterface(Ci.nsILoadContext);
    let userContextId = loadContext.originAttributes.userContextId;

    this._setContext(aEvent);
    let context = this.context;
    this.target = context.target;

    let spellInfo = null;
    let editFlags = null;
    let principal = null;

    let referrerInfo = Cc["@mozilla.org/referrer-info;1"].createInstance(
      Ci.nsIReferrerInfo
    );
    referrerInfo.initWithElement(aEvent.composedTarget);
    referrerInfo = lazy.E10SUtils.serializeReferrerInfo(referrerInfo);

    // In the case "onLink" we may have to send link referrerInfo to use in
    // _openLinkInParameters
    let linkReferrerInfo = null;
    if (context.onLink) {
      linkReferrerInfo = Cc["@mozilla.org/referrer-info;1"].createInstance(
        Ci.nsIReferrerInfo
      );
      linkReferrerInfo.initWithElement(context.link);
    }

    let target = context.target;
    if (target) {
      this._cleanContext();
    }

    editFlags = lazy.SpellCheckHelper.isEditable(
      aEvent.composedTarget,
      this.contentWindow
    );

    if (editFlags & lazy.SpellCheckHelper.SPELLCHECKABLE) {
      spellInfo = lazy.InlineSpellCheckerContent.initContextMenu(
        aEvent,
        editFlags,
        this
      );
    }

    // Set the event target first as the copy image command needs it to
    // determine what was context-clicked on. Then, update the state of the
    // commands on the context menu.
    this.docShell.contentViewer
      .QueryInterface(Ci.nsIContentViewerEdit)
      .setCommandNode(aEvent.composedTarget);
    aEvent.composedTarget.ownerGlobal.updateCommands("contentcontextmenu");

    let data = {
      context,
      charSet,
      baseURI,
      referrerInfo,
      editFlags,
      principal,
      contentType,
      docLocation,
      loginFillInfo,
      selectionInfo,
      userContextId,
      contentDisposition,
      frameID,
      frameBrowsingContextID,
      disableSetDesktopBackground,
    };

    if (context.inFrame && !context.inSrcdocFrame) {
      data.frameReferrerInfo = lazy.E10SUtils.serializeReferrerInfo(
        doc.referrerInfo
      );
    }

    if (linkReferrerInfo) {
      data.linkReferrerInfo = lazy.E10SUtils.serializeReferrerInfo(
        linkReferrerInfo
      );
    }

    Services.obs.notifyObservers(
      { wrappedJSObject: data },
      "on-prepare-contextmenu"
    );

    data.principal = doc.nodePrincipal;
    data.context.principal = context.principal;
    data.storagePrincipal = doc.effectiveStoragePrincipal;
    data.context.storagePrincipal = context.storagePrincipal;

    data.cookieJarSettings = lazy.E10SUtils.serializeCookieJarSettings(
      cookieJarSettings
    );

    // In the event that the content is running in the parent process, we don't
    // actually want the contextmenu events to reach the parent - we'll dispatch
    // a new contextmenu event after the async message has reached the parent
    // instead.
    aEvent.stopPropagation();

    data.spellInfo = null;
    if (!spellInfo) {
      this.sendAsyncMessage("contextmenu", data);
      return;
    }

    try {
      data.spellInfo = await spellInfo;
    } catch (ex) {}
    this.sendAsyncMessage("contextmenu", data);
  }

  /**
   * Some things are not serializable, so we either have to only send
   * their needed data or regenerate them in nsContextMenu.js
   * - target and target.ownerDocument
   * - link
   * - linkURI
   */
  _cleanContext(aEvent) {
    const context = this.context;
    const cleanTarget = Object.create(null);

    cleanTarget.ownerDocument = {
      // used for nsContextMenu.initLeaveDOMFullScreenItems and
      // nsContextMenu.initMediaPlayerItems
      fullscreen: context.target.ownerDocument.fullscreen,

      // used for nsContextMenu.initMiscItems
      contentType: context.target.ownerDocument.contentType,

      // used for nsContextMenu.saveLink
      isPrivate: lazy.PrivateBrowsingUtils.isContentWindowPrivate(
        context.target.ownerGlobal
      ),
    };

    // used for nsContextMenu.initMediaPlayerItems
    Object.assign(cleanTarget, {
      ended: context.target.ended,
      muted: context.target.muted,
      paused: context.target.paused,
      controls: context.target.controls,
      duration: context.target.duration,
    });

    const onMedia = context.onVideo || context.onAudio;

    if (onMedia) {
      Object.assign(cleanTarget, {
        loop: context.target.loop,
        error: context.target.error,
        networkState: context.target.networkState,
        playbackRate: context.target.playbackRate,
        NETWORK_NO_SOURCE: context.target.NETWORK_NO_SOURCE,
      });

      if (context.onVideo) {
        Object.assign(cleanTarget, {
          readyState: context.target.readyState,
          HAVE_CURRENT_DATA: context.target.HAVE_CURRENT_DATA,
        });
      }
    }

    context.target = cleanTarget;

    if (context.link) {
      context.link = { href: context.linkURL };
    }

    delete context.linkURI;
  }

  _setContext(aEvent) {
    this.context = Object.create(null);
    const context = this.context;

    context.timeStamp = aEvent.timeStamp;
    context.screenXDevPx = aEvent.screenX * this.contentWindow.devicePixelRatio;
    context.screenYDevPx = aEvent.screenY * this.contentWindow.devicePixelRatio;
    context.mozInputSource = aEvent.mozInputSource;

    let node = aEvent.composedTarget;

    // Set the node to containing <video>/<audio>/<embed>/<object> if the node
    // is in the videocontrols UA Widget.
    if (node.containingShadowRoot?.isUAWidget()) {
      const host = node.containingShadowRoot.host;
      if (
        this.contentWindow.HTMLMediaElement.isInstance(host) ||
        this.contentWindow.HTMLEmbedElement.isInstance(host) ||
        this.contentWindow.HTMLObjectElement.isInstance(host)
      ) {
        node = host;
      }
    }

    const XUL_NS =
      "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

    context.shouldDisplay = true;

    if (
      node.nodeType == node.DOCUMENT_NODE ||
      // Don't display for XUL element unless <label class="text-link">
      (node.namespaceURI == XUL_NS && !this._isXULTextLinkLabel(node))
    ) {
      context.shouldDisplay = false;
      return;
    }

    const isAboutDevtoolsToolbox = this.document.documentURI.startsWith(
      "about:devtools-toolbox"
    );
    const editFlags = lazy.SpellCheckHelper.isEditable(
      node,
      this.contentWindow
    );

    if (
      isAboutDevtoolsToolbox &&
      (editFlags & lazy.SpellCheckHelper.TEXTINPUT) === 0
    ) {
      // Don't display for about:devtools-toolbox page unless the source was text input.
      context.shouldDisplay = false;
      return;
    }

    // Initialize context to be sent to nsContextMenu
    // Keep this consistent with the similar code in nsContextMenu's setContext
    context.bgImageURL = "";
    context.imageDescURL = "";
    context.imageInfo = null;
    context.mediaURL = "";
    context.webExtBrowserType = "";

    context.canSpellCheck = false;
    context.hasBGImage = false;
    context.hasMultipleBGImages = false;
    context.isDesignMode = false;
    context.inFrame = false;
    context.inPDFViewer = false;
    context.inSrcdocFrame = false;
    context.inSyntheticDoc = false;
    context.inTabBrowser = true;
    context.inWebExtBrowser = false;

    context.link = null;
    context.linkDownload = "";
    context.linkProtocol = "";
    context.linkTextStr = "";
    context.linkURL = "";
    context.linkURI = null;

    context.onAudio = false;
    context.onCanvas = false;
    context.onCompletedImage = false;
    context.onDRMMedia = false;
    context.onPiPVideo = false;
    context.onEditable = false;
    context.onImage = false;
    context.onKeywordField = false;
    context.onLink = false;
    context.onLoadedImage = false;
    context.onMailtoLink = false;
    context.onTelLink = false;
    context.onMozExtLink = false;
    context.onNumeric = false;
    context.onPassword = false;
    context.passwordRevealed = false;
    context.onSaveableLink = false;
    context.onSpellcheckable = false;
    context.onTextInput = false;
    context.onVideo = false;

    // Remember the node and its owner document that was clicked
    // This may be modifed before sending to nsContextMenu
    context.target = node;
    context.targetIdentifier = lazy.ContentDOMReference.get(node);

    context.principal = context.target.ownerDocument.nodePrincipal;
    context.storagePrincipal =
      context.target.ownerDocument.effectiveStoragePrincipal;
    context.csp = lazy.E10SUtils.serializeCSP(context.target.ownerDocument.csp);

    context.frameID = lazy.WebNavigationFrames.getFrameId(
      context.target.ownerGlobal
    );

    context.frameOuterWindowID =
      context.target.ownerGlobal.docShell.outerWindowID;

    context.frameBrowsingContextID =
      context.target.ownerGlobal.browsingContext.id;

    // Check if we are in the PDF Viewer.
    context.inPDFViewer =
      context.target.ownerDocument.nodePrincipal.originNoSuffix ==
      "resource://pdf.js";

    // Check if we are in a synthetic document (stand alone image, video, etc.).
    context.inSyntheticDoc = context.target.ownerDocument.mozSyntheticDocument;

    context.shouldInitInlineSpellCheckerUINoChildren = false;
    context.shouldInitInlineSpellCheckerUIWithChildren = false;

    this._setContextForNodesNoChildren(editFlags);
    this._setContextForNodesWithChildren(editFlags);

    this.lastMenuTarget = {
      // Remember the node for extensions.
      targetRef: Cu.getWeakReference(node),
      // The timestamp is used to verify that the target wasn't changed since the observed menu event.
      timeStamp: context.timeStamp,
    };

    if (isAboutDevtoolsToolbox) {
      // Setup the menu items on text input in about:devtools-toolbox.
      context.inAboutDevtoolsToolbox = true;
      context.canSpellCheck = false;
      context.inTabBrowser = false;
      context.inFrame = false;
      context.inSrcdocFrame = false;
      context.onSpellcheckable = false;
    }
  }

  /**
   * Sets up the parts of the context menu for when when nodes have no children.
   *
   * @param {Integer} editFlags The edit flags for the node. See SpellCheckHelper
   *                            for the details.
   */
  _setContextForNodesNoChildren(editFlags) {
    const context = this.context;

    if (context.target.nodeType == context.target.TEXT_NODE) {
      // For text nodes, look at the parent node to determine the spellcheck attribute.
      context.canSpellCheck =
        context.target.parentNode && this._isSpellCheckEnabled(context.target);
      return;
    }

    // We only deal with TEXT_NODE and ELEMENT_NODE in this function, so return
    // early if we don't have one.
    if (context.target.nodeType != context.target.ELEMENT_NODE) {
      return;
    }

    // See if the user clicked on an image. This check mirrors
    // nsDocumentViewer::GetInImage. Make sure to update both if this is
    // changed.
    if (
      context.target instanceof Ci.nsIImageLoadingContent &&
      (context.target.currentRequestFinalURI || context.target.currentURI)
    ) {
      context.onImage = true;

      context.imageInfo = {
        currentSrc: context.target.currentSrc,
        width: context.target.width,
        height: context.target.height,
        imageText: context.target.title || context.target.alt,
      };
      const { SVGAnimatedLength } = context.target.ownerGlobal;
      if (SVGAnimatedLength.isInstance(context.imageInfo.height)) {
        context.imageInfo.height = context.imageInfo.height.animVal.value;
      }
      if (SVGAnimatedLength.isInstance(context.imageInfo.width)) {
        context.imageInfo.width = context.imageInfo.width.animVal.value;
      }

      const request = context.target.getRequest(
        Ci.nsIImageLoadingContent.CURRENT_REQUEST
      );

      if (request && request.imageStatus & request.STATUS_SIZE_AVAILABLE) {
        context.onLoadedImage = true;
      }

      if (
        request &&
        request.imageStatus & request.STATUS_LOAD_COMPLETE &&
        !(request.imageStatus & request.STATUS_ERROR)
      ) {
        context.onCompletedImage = true;
      }

      // The URL of the image before redirects is the currentURI.  This is
      // intended to be used for "Copy Image Link".
      context.originalMediaURL = (() => {
        let currentURI = context.target.currentURI?.spec;
        if (currentURI && this._isMediaURLReusable(currentURI)) {
          return currentURI;
        }
        return "";
      })();

      // The actual URL the image was loaded from (after redirects) is the
      // currentRequestFinalURI.  We should use that as the URL for purposes of
      // deciding on the filename, if it is present. It might not be present
      // if images are blocked.
      //
      // It is important to check both the final and the current URI, as they
      // could be different blob URIs, see bug 1625786.
      context.mediaURL = (() => {
        let finalURI = context.target.currentRequestFinalURI?.spec;
        if (finalURI && this._isMediaURLReusable(finalURI)) {
          return finalURI;
        }
        let currentURI = context.target.currentURI?.spec;
        if (currentURI && this._isMediaURLReusable(currentURI)) {
          return currentURI;
        }
        return "";
      })();

      const descURL = context.target.getAttribute("longdesc");

      if (descURL) {
        context.imageDescURL = this._makeURLAbsolute(
          context.target.ownerDocument.body.baseURI,
          descURL
        );
      }
    } else if (
      this.contentWindow.HTMLCanvasElement.isInstance(context.target)
    ) {
      context.onCanvas = true;
    } else if (this.contentWindow.HTMLVideoElement.isInstance(context.target)) {
      const mediaURL = context.target.currentSrc || context.target.src;

      if (this._isMediaURLReusable(mediaURL)) {
        context.mediaURL = mediaURL;
      }

      if (this._isProprietaryDRM()) {
        context.onDRMMedia = true;
      }

      if (context.target.isCloningElementVisually) {
        context.onPiPVideo = true;
      }

      // Firefox always creates a HTMLVideoElement when loading an ogg file
      // directly. If the media is actually audio, be smarter and provide a
      // context menu with audio operations.
      if (
        context.target.readyState >= context.target.HAVE_METADATA &&
        (context.target.videoWidth == 0 || context.target.videoHeight == 0)
      ) {
        context.onAudio = true;
      } else {
        context.onVideo = true;
      }
    } else if (this.contentWindow.HTMLAudioElement.isInstance(context.target)) {
      context.onAudio = true;
      const mediaURL = context.target.currentSrc || context.target.src;

      if (this._isMediaURLReusable(mediaURL)) {
        context.mediaURL = mediaURL;
      }

      if (this._isProprietaryDRM()) {
        context.onDRMMedia = true;
      }
    } else if (
      editFlags &
      (lazy.SpellCheckHelper.INPUT | lazy.SpellCheckHelper.TEXTAREA)
    ) {
      context.onTextInput = (editFlags & lazy.SpellCheckHelper.TEXTINPUT) !== 0;
      context.onNumeric = (editFlags & lazy.SpellCheckHelper.NUMERIC) !== 0;
      context.onEditable = (editFlags & lazy.SpellCheckHelper.EDITABLE) !== 0;
      context.onPassword = (editFlags & lazy.SpellCheckHelper.PASSWORD) !== 0;
      context.passwordRevealed =
        context.onPassword && context.target.revealPassword;
      context.onSpellcheckable =
        (editFlags & lazy.SpellCheckHelper.SPELLCHECKABLE) !== 0;

      // This is guaranteed to be an input or textarea because of the condition above,
      // so the no-children flag is always correct. We deal with contenteditable elsewhere.
      if (context.onSpellcheckable) {
        context.shouldInitInlineSpellCheckerUINoChildren = true;
      }

      context.onKeywordField = editFlags & lazy.SpellCheckHelper.KEYWORD;
    } else if (this.contentWindow.HTMLHtmlElement.isInstance(context.target)) {
      const bodyElt = context.target.ownerDocument.body;

      if (bodyElt) {
        let computedURL;

        try {
          computedURL = this._getComputedURL(bodyElt, "background-image");
          context.hasMultipleBGImages = false;
        } catch (e) {
          context.hasMultipleBGImages = true;
        }

        if (computedURL) {
          context.hasBGImage = true;
          context.bgImageURL = this._makeURLAbsolute(
            bodyElt.baseURI,
            computedURL
          );
        }
      }
    }

    context.canSpellCheck = this._isSpellCheckEnabled(context.target);
  }

  /**
   * Sets up the parts of the context menu for when when nodes have children.
   *
   * @param {Integer} editFlags The edit flags for the node. See SpellCheckHelper
   *                            for the details.
   */
  _setContextForNodesWithChildren(editFlags) {
    const context = this.context;

    // Second, bubble out, looking for items of interest that can have childen.
    // Always pick the innermost link, background image, etc.
    let elem = context.target;

    while (elem) {
      if (elem.nodeType == elem.ELEMENT_NODE) {
        // Link?
        const XLINK_NS = "http://www.w3.org/1999/xlink";

        if (
          !context.onLink &&
          // Be consistent with what hrefAndLinkNodeForClickEvent
          // does in browser.js
          (this._isXULTextLinkLabel(elem) ||
            (this.contentWindow.HTMLAnchorElement.isInstance(elem) &&
              elem.href) ||
            (this.contentWindow.SVGAElement.isInstance(elem) &&
              (elem.href || elem.hasAttributeNS(XLINK_NS, "href"))) ||
            (this.contentWindow.HTMLAreaElement.isInstance(elem) &&
              elem.href) ||
            this.contentWindow.HTMLLinkElement.isInstance(elem) ||
            elem.getAttributeNS(XLINK_NS, "type") == "simple")
        ) {
          // Target is a link or a descendant of a link.
          context.onLink = true;

          // Remember corresponding element.
          context.link = elem;
          context.linkURL = this._getLinkURL();
          context.linkURI = this._getLinkURI();
          context.linkTextStr = this._getLinkText();
          context.linkProtocol = this._getLinkProtocol();
          context.onMailtoLink = context.linkProtocol == "mailto";
          context.onTelLink = context.linkProtocol == "tel";
          context.onMozExtLink = context.linkProtocol == "moz-extension";
          context.onSaveableLink = this._isLinkSaveable(context.link);

          try {
            if (elem.download) {
              // Ignore download attribute on cross-origin links
              context.principal.checkMayLoad(context.linkURI, true);
              context.linkDownload = elem.download;
            }
          } catch (ex) {}
        }

        // Background image?  Don't bother if we've already found a
        // background image further down the hierarchy.  Otherwise,
        // we look for the computed background-image style.
        if (!context.hasBGImage && !context.hasMultipleBGImages) {
          let bgImgUrl = null;

          try {
            bgImgUrl = this._getComputedURL(elem, "background-image");
            context.hasMultipleBGImages = false;
          } catch (e) {
            context.hasMultipleBGImages = true;
          }

          if (bgImgUrl) {
            context.hasBGImage = true;
            context.bgImageURL = this._makeURLAbsolute(elem.baseURI, bgImgUrl);
          }
        }
      }

      elem = elem.flattenedTreeParentNode;
    }

    // See if the user clicked in a frame.
    const docDefaultView = context.target.ownerGlobal;

    if (docDefaultView != docDefaultView.top) {
      context.inFrame = true;

      if (context.target.ownerDocument.isSrcdocDocument) {
        context.inSrcdocFrame = true;
      }
    }

    // if the document is editable, show context menu like in text inputs
    if (!context.onEditable) {
      if (editFlags & lazy.SpellCheckHelper.CONTENTEDITABLE) {
        // If this.onEditable is false but editFlags is CONTENTEDITABLE, then
        // the document itself must be editable.
        context.onTextInput = true;
        context.onKeywordField = false;
        context.onImage = false;
        context.onLoadedImage = false;
        context.onCompletedImage = false;
        context.inFrame = false;
        context.inSrcdocFrame = false;
        context.hasBGImage = false;
        context.isDesignMode = true;
        context.onEditable = true;
        context.onSpellcheckable = true;
        context.shouldInitInlineSpellCheckerUIWithChildren = true;
      }
    }
  }

  _destructionObservers = new Set();
  registerDestructionObserver(obj) {
    this._destructionObservers.add(obj);
  }

  unregisterDestructionObserver(obj) {
    this._destructionObservers.delete(obj);
  }

  didDestroy() {
    for (let obs of this._destructionObservers) {
      obs.actorDestroyed(this);
    }
    this._destructionObservers = null;
  }
}
