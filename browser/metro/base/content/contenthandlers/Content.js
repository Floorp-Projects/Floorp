// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This stays here because otherwise it's hard to tell if there's a parsing error
dump("### Content.js loaded\n");

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;
let Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "Services", function() {
  Cu.import("resource://gre/modules/Services.jsm");
  return Services;
});

XPCOMUtils.defineLazyGetter(this, "Rect", function() {
  Cu.import("resource://gre/modules/Geometry.jsm");
  return Rect;
});

XPCOMUtils.defineLazyGetter(this, "Point", function() {
  Cu.import("resource://gre/modules/Geometry.jsm");
  return Point;
});

XPCOMUtils.defineLazyServiceGetter(this, "gFocusManager",
  "@mozilla.org/focus-manager;1", "nsIFocusManager");

XPCOMUtils.defineLazyServiceGetter(this, "gDOMUtils",
  "@mozilla.org/inspector/dom-utils;1", "inIDOMUtils");

let XULDocument = Ci.nsIDOMXULDocument;
let HTMLHtmlElement = Ci.nsIDOMHTMLHtmlElement;
let HTMLIFrameElement = Ci.nsIDOMHTMLIFrameElement;
let HTMLFrameElement = Ci.nsIDOMHTMLFrameElement;
let HTMLFrameSetElement = Ci.nsIDOMHTMLFrameSetElement;
let HTMLSelectElement = Ci.nsIDOMHTMLSelectElement;
let HTMLOptionElement = Ci.nsIDOMHTMLOptionElement;

const kReferenceDpi = 240; // standard "pixel" size used in some preferences

const kStateActive = 0x00000001; // :active pseudoclass for elements

/*
 * getBoundingContentRect
 *
 * @param aElement
 * @return Bounding content rect adjusted for scroll and frame offsets.
 */
function getBoundingContentRect(aElement) {
  if (!aElement)
    return new Rect(0, 0, 0, 0);

  let document = aElement.ownerDocument;
  while(document.defaultView.frameElement)
    document = document.defaultView.frameElement.ownerDocument;

  let offset = ContentScroll.getScrollOffset(content);
  offset = new Point(offset.x, offset.y);

  let r = aElement.getBoundingClientRect();

  // step out of iframes and frames, offsetting scroll values
  let view = aElement.ownerDocument.defaultView;
  for (let frame = view; frame != content; frame = frame.parent) {
    // adjust client coordinates' origin to be top left of iframe viewport
    let rect = frame.frameElement.getBoundingClientRect();
    let left = frame.getComputedStyle(frame.frameElement, "").borderLeftWidth;
    let top = frame.getComputedStyle(frame.frameElement, "").borderTopWidth;
    offset.add(rect.left + parseInt(left), rect.top + parseInt(top));
  }

  return new Rect(r.left + offset.x, r.top + offset.y, r.width, r.height);
}

/*
 * getOverflowContentBoundingRect
 *
 * @param aElement
 * @return Bounding content rect adjusted for scroll and frame offsets.
 */

function getOverflowContentBoundingRect(aElement) {
  let r = getBoundingContentRect(aElement);

  // If the overflow is hidden don't bother calculating it
  let computedStyle = aElement.ownerDocument.defaultView.getComputedStyle(aElement);
  let blockDisplays = ["block", "inline-block", "list-item"];
  if ((blockDisplays.indexOf(computedStyle.getPropertyValue("display")) != -1 &&
       computedStyle.getPropertyValue("overflow") == "hidden") ||
      aElement instanceof HTMLSelectElement) {
    return r;
  }

  for (let i = 0; i < aElement.childElementCount; i++) {
    r = r.union(getBoundingContentRect(aElement.children[i]));
  }

  return r;
}

/*
 * Content
 *
 * Browser event receiver for content.
 */
let Content = {
  _debugEvents: false,

  get formAssistant() {
    delete this.formAssistant;
    return this.formAssistant = new FormAssistant();
  },

  init: function init() {
    this._isZoomedToElement = false;

    // Asyncronous messages sent from the browser
    addMessageListener("Browser:Blur", this);
    addMessageListener("Browser:SaveAs", this);
    addMessageListener("Browser:MozApplicationCache:Fetch", this);
    addMessageListener("Browser:SetCharset", this);
    addMessageListener("Browser:CanUnload", this);
    addMessageListener("Browser:PanBegin", this);

    addEventListener("touchstart", this, false);
    addEventListener("click", this, true);
    addEventListener("keydown", this);
    addEventListener("keyup", this);

    // Synchronous events caught during the bubbling phase
    addEventListener("MozApplicationManifest", this, false);
    addEventListener("DOMContentLoaded", this, false);
    addEventListener("pagehide", this, false);
    // Attach a listener to watch for "click" events bubbling up from error
    // pages and other similar page. This lets us fix bugs like 401575 which
    // require error page UI to do privileged things, without letting error
    // pages have any privilege themselves.
    addEventListener("click", this, false);

    docShell.useGlobalHistory = true;
  },

  /*******************************************
   * Events
   */

  handleEvent: function handleEvent(aEvent) {
    if (this._debugEvents) Util.dumpLn("Content:", aEvent.type);
    switch (aEvent.type) {
      case "MozApplicationManifest": {
        let doc = aEvent.originalTarget;
        sendAsyncMessage("Browser:MozApplicationManifest", {
          location: doc.documentURIObject.spec,
          manifest: doc.documentElement.getAttribute("manifest"),
          charset: doc.characterSet
        });
        break;
      }

      case "keydown":
        if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE)
          this.formAssistant.close();
        break;

      case "keyup":
        // If after a key is pressed we still have no input, then close
        // the autocomplete.  Perhaps the user used backspace or delete.
        if (!aEvent.target.value)
          this.formAssistant.close();
        else
          this.formAssistant.open(aEvent.target);
        break;

      case "click":
        if (aEvent.eventPhase == aEvent.BUBBLING_PHASE)
          this._onClickBubble(aEvent);
        else
          this._onClickCapture(aEvent);
        break;
      
      case "DOMContentLoaded":
        this._maybeNotifyErrorPage();
        break;

      case "pagehide":
        if (aEvent.target == content.document)
          this._resetFontSize();          
        break;

      case "touchstart":
        this._onTouchStart(aEvent);
        break;
    }
  },

  receiveMessage: function receiveMessage(aMessage) {
    if (this._debugEvents) Util.dumpLn("Content:", aMessage.name);
    let json = aMessage.json;
    let x = json.x;
    let y = json.y;
    let modifiers = json.modifiers;

    switch (aMessage.name) {
      case "Browser:Blur":
        gFocusManager.clearFocus(content);
        break;

      case "Browser:CanUnload":
        let canUnload = docShell.contentViewer.permitUnload();
        sendSyncMessage("Browser:CanUnload:Return", { permit: canUnload });
        break;

      case "Browser:SaveAs":
        break;

      case "Browser:MozApplicationCache:Fetch": {
        let currentURI = Services.io.newURI(json.location, json.charset, null);
        let manifestURI = Services.io.newURI(json.manifest, json.charset, currentURI);
        let updateService = Cc["@mozilla.org/offlinecacheupdate-service;1"]
                            .getService(Ci.nsIOfflineCacheUpdateService);
        updateService.scheduleUpdate(manifestURI, currentURI, content);
        break;
      }

      case "Browser:SetCharset": {
        docShell.gatherCharsetMenuTelemetry();
        docShell.charset = json.charset;

        let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
        webNav.reload(Ci.nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
        break;
      }

      case "Browser:PanBegin":
        this._cancelTapHighlight();
        break;
    }
  },

  /******************************************************
   * Event handlers
   */

  _onTouchStart: function _onTouchStart(aEvent) {
    let element = aEvent.target;

    // There is no need to have a feedback for disabled element
    let isDisabled = element instanceof HTMLOptionElement ?
      (element.disabled || element.parentNode.disabled) : element.disabled;
    if (isDisabled)
      return;

    // Set the target element to active
    this._doTapHighlight(element);
  },

  _onClickCapture: function _onClickCapture(aEvent) {
    let element = aEvent.target;

    ContextMenuHandler.reset();

    // Only show autocomplete after the item is clicked
    if (!this.lastClickElement || this.lastClickElement != element) {
      this.lastClickElement = element;
      if (aEvent.mozInputSource == Ci.nsIDOMMouseEvent.MOZ_SOURCE_MOUSE &&
          !(element instanceof HTMLSelectElement)) {
        return;
      }
    }

    this.formAssistant.focusSync = true;
    this.formAssistant.open(element, aEvent);
    this._cancelTapHighlight();
    this.formAssistant.focusSync = false;

    // A tap on a form input triggers touch input caret selection
    if (Util.isTextInput(element) &&
        aEvent.mozInputSource == Ci.nsIDOMMouseEvent.MOZ_SOURCE_TOUCH) {
      let { offsetX, offsetY } = Util.translateToTopLevelWindow(element);
      sendAsyncMessage("Content:SelectionCaret", {
        xPos: aEvent.clientX + offsetX,
        yPos: aEvent.clientY + offsetY
      });
    }
  },

  // Checks clicks we care about - events bubbling up from about pages.
  _onClickBubble: function _onClickBubble(aEvent) {
    // Don't trust synthetic events
    if (!aEvent.isTrusted)
      return;

    let ot = aEvent.originalTarget;
    let errorDoc = ot.ownerDocument;
    if (!errorDoc)
      return;

    // If the event came from an ssl error page, it is probably either 
    // "Add Exception…" or "Get me out of here!" button.
    if (/^about:certerror\?e=nssBadCert/.test(errorDoc.documentURI)) {
      let perm = errorDoc.getElementById("permanentExceptionButton");
      let temp = errorDoc.getElementById("temporaryExceptionButton");
      if (ot == temp || ot == perm) {
        let action = (ot == perm ? "permanent" : "temporary");
        sendAsyncMessage("Browser:CertException",
                         { url: errorDoc.location.href, action: action });
      } else if (ot == errorDoc.getElementById("getMeOutOfHereButton")) {
        sendAsyncMessage("Browser:CertException",
                         { url: errorDoc.location.href, action: "leave" });
      }
    } else if (/^about:blocked/.test(errorDoc.documentURI)) {
      // The event came from a button on a malware/phishing block page
      // First check whether it's malware or phishing, so that we can
      // use the right strings/links.
      let isMalware = /e=malwareBlocked/.test(errorDoc.documentURI);
    
      if (ot == errorDoc.getElementById("getMeOutButton")) {
        sendAsyncMessage("Browser:BlockedSite",
                         { url: errorDoc.location.href, action: "leave" });
      } else if (ot == errorDoc.getElementById("reportButton")) {
        // This is the "Why is this site blocked" button.  For malware,
        // we can fetch a site-specific report, for phishing, we redirect
        // to the generic page describing phishing protection.
        let action = isMalware ? "report-malware" : "report-phishing";
        sendAsyncMessage("Browser:BlockedSite",
                         { url: errorDoc.location.href, action: action });
      } else if (ot == errorDoc.getElementById("ignoreWarningButton")) {
        // Allow users to override and continue through to the site,
        // but add a notify bar as a reminder, so that they don't lose
        // track after, e.g., tab switching.
        let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
        webNav.loadURI(content.location,
                       Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CLASSIFIER,
                       null, null, null);
      }
    }
  },


  /******************************************************
   * General utilities
   */

  _getContentClientRects: function getContentClientRects(aElement) {
    let offset = ContentScroll.getScrollOffset(content);
    offset = new Point(offset.x, offset.y);

    let nativeRects = aElement.getClientRects();
    // step out of iframes and frames, offsetting scroll values
    for (let frame = aElement.ownerDocument.defaultView; frame != content;
         frame = frame.parent) {
      // adjust client coordinates' origin to be top left of iframe viewport
      let rect = frame.frameElement.getBoundingClientRect();
      let left = frame.getComputedStyle(frame.frameElement, "").borderLeftWidth;
      let top = frame.getComputedStyle(frame.frameElement, "").borderTopWidth;
      offset.add(rect.left + parseInt(left), rect.top + parseInt(top));
    }

    let result = [];
    for (let i = nativeRects.length - 1; i >= 0; i--) {
      let r = nativeRects[i];
      result.push({ left: r.left + offset.x,
                    top: r.top + offset.y,
                    width: r.width,
                    height: r.height
                  });
    }
    return result;
  },

  _maybeNotifyErrorPage: function _maybeNotifyErrorPage() {
    // Notify browser that an error page is being shown instead
    // of the target location. Necessary to get proper thumbnail
    // updates on chrome for error pages.
    if (content.location.href !== content.document.documentURI)
      sendAsyncMessage("Browser:ErrorPage", null);
  },

  _resetFontSize: function _resetFontSize() {
    this._isZoomedToElement = false;
    this._setMinFontSize(0);
  },

  _highlightElement: null,

  _doTapHighlight: function _doTapHighlight(aElement) {
    gDOMUtils.setContentState(aElement, kStateActive);
    this._highlightElement = aElement;
  },

  _cancelTapHighlight: function _cancelTapHighlight(aElement) {
    gDOMUtils.setContentState(content.document.documentElement, kStateActive);
    this._highlightElement = null;
  },

  /*
   * _sendMouseEvent
   *
   * Delivers mouse events directly to the content window, bypassing
   * the input overlay.
   */
  _sendMouseEvent: function _sendMouseEvent(aName, aElement, aX, aY, aButton) {
    // Elements can be off from the aX/aY point because due to touch radius.
    // If outside, we move the touch point to the center of the element.
    if (!(aElement instanceof HTMLHtmlElement)) {
      let isTouchClick = true;
      let rects = this._getContentClientRects(aElement);
      for (let i = 0; i < rects.length; i++) {
        let rect = rects[i];
        // We might be able to deal with fractional pixels, but mouse
        // events won't. Deflate the bounds in by 1 pixel to deal with
        // any fractional scroll offset issues.
        let inBounds = 
          (aX > rect.left + 1 && aX < (rect.left + rect.width - 1)) &&
          (aY > rect.top + 1 && aY < (rect.top + rect.height - 1));
        if (inBounds) {
          isTouchClick = false;
          break;
        }
      }

      if (isTouchClick) {
        let rect = new Rect(rects[0].left, rects[0].top,
                            rects[0].width, rects[0].height);
        if (rect.isEmpty())
          return;

        let point = rect.center();
        aX = point.x;
        aY = point.y;
      }
    }

    let button = aButton || 0;
    let scrollOffset = ContentScroll.getScrollOffset(content);
    let x = aX - scrollOffset.x;
    let y = aY - scrollOffset.y;

    // setting touch source here is important so that when this gets
    // captured by our precise input detection we can ignore it.
    let windowUtils = Util.getWindowUtils(content);
    windowUtils.sendMouseEventToWindow(aName, x, y, button, 1, 0, true,
                                       1.0, Ci.nsIDOMMouseEvent.MOZ_SOURCE_MOUSE);
  },

  _setMinFontSize: function _setMinFontSize(aSize) {
    let viewer = docShell.contentViewer.QueryInterface(Ci.nsIMarkupDocumentViewer);
    if (viewer)
      viewer.minFontSize = aSize;
  }
};

Content.init();

var FormSubmitObserver = {
  init: function init(){
    addMessageListener("Browser:TabOpen", this);
    addMessageListener("Browser:TabClose", this);

    addEventListener("pageshow", this, false);

    Services.obs.addObserver(this, "invalidformsubmit", false);
  },

  handleEvent: function handleEvent(aEvent) {
    let target = aEvent.originalTarget;
    let isRootDocument = (target == content.document || target.ownerDocument == content.document);
    if (!isRootDocument)
      return;

    // Reset invalid submit state on each pageshow
    if (aEvent.type == "pageshow")
      Content.formAssistant.invalidSubmit = false;
  },

  receiveMessage: function receiveMessage(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Browser:TabOpen":
        Services.obs.addObserver(this, "formsubmit", false);
        break;
      case "Browser:TabClose":
        Services.obs.removeObserver(this, "formsubmit");
        break;
    }
  },

  notify: function notify(aFormElement, aWindow, aActionURI, aCancelSubmit) {
    // Do not notify unless this is the window where the submit occurred
    if (aWindow == content)
      // We don't need to send any data along
      sendAsyncMessage("Browser:FormSubmit", {});
  },

  notifyInvalidSubmit: function notifyInvalidSubmit(aFormElement, aInvalidElements) {
    if (!aInvalidElements.length)
      return;

    let element = aInvalidElements.queryElementAt(0, Ci.nsISupports);
    if (!(element instanceof HTMLInputElement ||
          element instanceof HTMLTextAreaElement ||
          element instanceof HTMLSelectElement ||
          element instanceof HTMLButtonElement)) {
      return;
    }

    Content.formAssistant.invalidSubmit = true;
    Content.formAssistant.open(element);
  },

  QueryInterface : function(aIID) {
    if (!aIID.equals(Ci.nsIFormSubmitObserver) &&
        !aIID.equals(Ci.nsISupportsWeakReference) &&
        !aIID.equals(Ci.nsISupports))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

FormSubmitObserver.init();
