/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This content script should work in any browser or iframe and should not
 * depend on the frame being contained in tabbrowser. */

/* eslint-env mozilla/frame-script */
/* eslint no-unused-vars: ["error", {args: "none"}] */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

// TabChildGlobal
var global = this;

XPCOMUtils.defineLazyModuleGetters(this, {
  BlockedSiteContent: "resource:///modules/BlockedSiteContent.jsm",
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  ContentLinkHandler: "resource:///modules/ContentLinkHandler.jsm",
  ContentMetaHandler: "resource:///modules/ContentMetaHandler.jsm",
  ContentWebRTC: "resource:///modules/ContentWebRTC.jsm",
  LoginManagerContent: "resource://gre/modules/LoginManagerContent.jsm",
  LoginFormFactory: "resource://gre/modules/LoginManagerContent.jsm",
  InsecurePasswordUtils: "resource://gre/modules/InsecurePasswordUtils.jsm",
  PluginContent: "resource:///modules/PluginContent.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  FormSubmitObserver: "resource:///modules/FormSubmitObserver.jsm",
  NetErrorContent: "resource:///modules/NetErrorContent.jsm",
  PageMetadata: "resource://gre/modules/PageMetadata.jsm",
  WebNavigationFrames: "resource://gre/modules/WebNavigationFrames.jsm",
  ContextMenu: "resource:///modules/ContextMenu.jsm",
});

XPCOMUtils.defineLazyProxy(this, "contextMenu", () => {
  return new ContextMenu(global);
});

XPCOMUtils.defineLazyProxy(this, "formSubmitObserver", () => {
  return new FormSubmitObserver(content, this);
}, {
  // stub QI
  QueryInterface: ChromeUtils.generateQI([Ci.nsIFormSubmitObserver, Ci.nsISupportsWeakReference])
});

XPCOMUtils.defineLazyProxy(this, "PageInfoListener",
                           "resource:///modules/PageInfoListener.jsm");

XPCOMUtils.defineLazyProxy(this, "LightWeightThemeWebInstallListener",
                           "resource:///modules/LightWeightThemeWebInstallListener.jsm");

Services.els.addSystemEventListener(global, "contextmenu", contextMenu, false);

Services.obs.addObserver(formSubmitObserver, "invalidformsubmit", true);

addMessageListener("PageInfo:getData", PageInfoListener);

addMessageListener("RemoteLogins:fillForm", function(message) {
  // intercept if ContextMenu.jsm had sent a plain object for remote targets
  message.objects.inputElement = contextMenu.getTarget(message, "inputElement");
  LoginManagerContent.receiveMessage(message, content);
});
addEventListener("DOMFormHasPassword", function(event) {
  LoginManagerContent.onDOMFormHasPassword(event, content);
  let formLike = LoginFormFactory.createFromForm(event.originalTarget);
  InsecurePasswordUtils.reportInsecurePasswords(formLike);
});
addEventListener("DOMInputPasswordAdded", function(event) {
  LoginManagerContent.onDOMInputPasswordAdded(event, content);
  let formLike = LoginFormFactory.createFromField(event.originalTarget);
  InsecurePasswordUtils.reportInsecurePasswords(formLike);
});
addEventListener("pageshow", function(event) {
  LoginManagerContent.onPageShow(event, content);
});
addEventListener("DOMAutoComplete", function(event) {
  LoginManagerContent.onUsernameInput(event);
});
addEventListener("blur", function(event) {
  LoginManagerContent.onUsernameInput(event);
});

var AboutBlockedSiteListener = {
  init(chromeGlobal) {
    addMessageListener("DeceptiveBlockedDetails", this);
    chromeGlobal.addEventListener("AboutBlockedLoaded", this, false, true);
  },

  get isBlockedSite() {
    return content.document.documentURI.startsWith("about:blocked");
  },

  receiveMessage(msg) {
    if (!this.isBlockedSite) {
      return;
    }

    BlockedSiteContent.receiveMessage(global, msg);
  },

  handleEvent(aEvent) {
    if (!this.isBlockedSite) {
      return;
    }

    if (aEvent.type != "AboutBlockedLoaded") {
      return;
    }

    BlockedSiteContent.handleEvent(global, aEvent);
  },
};
AboutBlockedSiteListener.init(this);

var AboutNetAndCertErrorListener = {
  init(chromeGlobal) {
    addMessageListener("CertErrorDetails", this);
    addMessageListener("Browser:CaptivePortalFreed", this);
    chromeGlobal.addEventListener("AboutNetErrorLoad", this, false, true);
    chromeGlobal.addEventListener("AboutNetErrorOpenCaptivePortal", this, false, true);
    chromeGlobal.addEventListener("AboutNetErrorSetAutomatic", this, false, true);
    chromeGlobal.addEventListener("AboutNetErrorResetPreferences", this, false, true);
  },

  isAboutNetError(doc) {
    return doc.documentURI.startsWith("about:neterror");
  },

  isAboutCertError(doc) {
    return doc.documentURI.startsWith("about:certerror");
  },

  receiveMessage(msg) {
    if (msg.name == "CertErrorDetails") {
      let frameDocShell = WebNavigationFrames.findDocShell(msg.data.frameId, docShell);
      // We need nsIWebNavigation to access docShell.document.
      frameDocShell && frameDocShell.QueryInterface(Ci.nsIWebNavigation);
      if (!frameDocShell || !this.isAboutCertError(frameDocShell.document)) {
        return;
      }

      NetErrorContent.onCertErrorDetails(global, msg, frameDocShell);
    } else if (msg.name == "Browser:CaptivePortalFreed") {
      // TODO: This check is not correct for frames.
      if (!this.isAboutCertError(content.document)) {
        return;
      }

      this.onCaptivePortalFreed(msg);
    }
  },

  onCaptivePortalFreed(msg) {
    content.dispatchEvent(new content.CustomEvent("AboutNetErrorCaptivePortalFreed"));
  },

  handleEvent(aEvent) {
    // Documents have a null ownerDocument.
    let doc = aEvent.originalTarget.ownerDocument || aEvent.originalTarget;

    if (!this.isAboutNetError(doc) && !this.isAboutCertError(doc)) {
      return;
    }

    NetErrorContent.handleEvent(global, aEvent);
  },
};
AboutNetAndCertErrorListener.init(this);

var ClickEventHandler = {
  init: function init() {
    Services.els.addSystemEventListener(global, "click", this, true);
  },

  handleEvent(event) {
    if (!event.isTrusted || event.defaultPrevented || event.button == 2) {
      return;
    }

    let originalTarget = event.originalTarget;
    let ownerDoc = originalTarget.ownerDocument;
    if (!ownerDoc) {
      return;
    }

    // Handle click events from about pages
    if (event.button == 0) {
      if (AboutNetAndCertErrorListener.isAboutCertError(ownerDoc)) {
        NetErrorContent.onCertError(global, originalTarget, ownerDoc.defaultView);
        return;
      } else if (ownerDoc.documentURI.startsWith("about:blocked")) {
        BlockedSiteContent.onAboutBlocked(global, originalTarget, ownerDoc);
        return;
      } else if (AboutNetAndCertErrorListener.isAboutNetError(ownerDoc)) {
        NetErrorContent.onAboutNetError(global, event, ownerDoc.documentURI);
        return;
      }
    }

    let [href, node, principal] = this._hrefAndLinkNodeForClickEvent(event);

    // get referrer attribute from clicked link and parse it
    // if per element referrer is enabled, the element referrer overrules
    // the document wide referrer
    let referrerPolicy = ownerDoc.referrerPolicy;
    if (node) {
      let referrerAttrValue = Services.netUtils.parseAttributePolicyString(node.
                              getAttribute("referrerpolicy"));
      if (referrerAttrValue !== Ci.nsIHttpChannel.REFERRER_POLICY_UNSET) {
        referrerPolicy = referrerAttrValue;
      }
    }

    let frameOuterWindowID = WebNavigationFrames.getFrameId(ownerDoc.defaultView);

    let json = { button: event.button, shiftKey: event.shiftKey,
                 ctrlKey: event.ctrlKey, metaKey: event.metaKey,
                 altKey: event.altKey, href: null, title: null,
                 bookmark: false, frameOuterWindowID, referrerPolicy,
                 triggeringPrincipal: principal,
                 originAttributes: principal ? principal.originAttributes : {},
                 isContentWindowPrivate: PrivateBrowsingUtils.isContentWindowPrivate(ownerDoc.defaultView)};

    if (href) {
      try {
        BrowserUtils.urlSecurityCheck(href, principal);
      } catch (e) {
        return;
      }

      json.href = href;
      if (node) {
        json.title = node.getAttribute("title");
        if (event.button == 0 && !event.ctrlKey && !event.shiftKey &&
            !event.altKey && !event.metaKey) {
          json.bookmark = node.getAttribute("rel") == "sidebar";
          if (json.bookmark) {
            event.preventDefault(); // Need to prevent the pageload.
          }
        }
      }
      json.noReferrer = BrowserUtils.linkHasNoReferrer(node);

      // Check if the link needs to be opened with mixed content allowed.
      // Only when the owner doc has |mixedContentChannel| and the same origin
      // should we allow mixed content.
      json.allowMixedContent = false;
      let docshell = ownerDoc.docShell;
      if (docShell.mixedContentChannel) {
        const sm = Services.scriptSecurityManager;
        try {
          let targetURI = Services.io.newURI(href);
          sm.checkSameOriginURI(docshell.mixedContentChannel.URI, targetURI, false);
          json.allowMixedContent = true;
        } catch (e) {}
      }
      json.originPrincipal = ownerDoc.nodePrincipal;
      json.triggeringPrincipal = ownerDoc.nodePrincipal;

      sendAsyncMessage("Content:Click", json);
      return;
    }

    // This might be middle mouse navigation.
    if (event.button == 1) {
      sendAsyncMessage("Content:Click", json);
    }
  },

  /**
   * Extracts linkNode and href for the current click target.
   *
   * @param event
   *        The click event.
   * @return [href, linkNode, linkPrincipal].
   *
   * @note linkNode will be null if the click wasn't on an anchor
   *       element. This includes SVG links, because callers expect |node|
   *       to behave like an <a> element, which SVG links (XLink) don't.
   */
  _hrefAndLinkNodeForClickEvent(event) {
    function isHTMLLink(aNode) {
      // Be consistent with what nsContextMenu.js does.
      return ((aNode instanceof content.HTMLAnchorElement && aNode.href) ||
              (aNode instanceof content.HTMLAreaElement && aNode.href) ||
              aNode instanceof content.HTMLLinkElement);
    }

    let node = event.target;
    while (node && !isHTMLLink(node)) {
      node = node.parentNode;
    }

    if (node)
      return [node.href, node, node.ownerDocument.nodePrincipal];

    // If there is no linkNode, try simple XLink.
    let href, baseURI;
    node = event.target;
    while (node && !href) {
      if (node.nodeType == content.Node.ELEMENT_NODE &&
          (node.localName == "a" ||
           node.namespaceURI == "http://www.w3.org/1998/Math/MathML")) {
        href = node.getAttribute("href") ||
               node.getAttributeNS("http://www.w3.org/1999/xlink", "href");
        if (href) {
          baseURI = node.ownerDocument.baseURIObject;
          break;
        }
      }
      node = node.parentNode;
    }

    // In case of XLink, we don't return the node we got href from since
    // callers expect <a>-like elements.
    // Note: makeURI() will throw if aUri is not a valid URI.
    return [href ? Services.io.newURI(href, null, baseURI).spec : null, null,
            node && node.ownerDocument.nodePrincipal];
  }
};
ClickEventHandler.init();

ContentLinkHandler.init(this);
ContentMetaHandler.init(this);

// TODO: Load this lazily so the JSM is run only if a relevant event/message fires.
void new PluginContent(global);

addEventListener("DOMWindowFocus", function(event) {
  sendAsyncMessage("DOMWindowFocus", {});
}, false);

// We use this shim so that ContentWebRTC.jsm will not be loaded until
// it is actually needed.
var ContentWebRTCShim = message => ContentWebRTC.receiveMessage(message);

addMessageListener("rtcpeer:Allow", ContentWebRTCShim);
addMessageListener("rtcpeer:Deny", ContentWebRTCShim);
addMessageListener("webrtc:Allow", ContentWebRTCShim);
addMessageListener("webrtc:Deny", ContentWebRTCShim);
addMessageListener("webrtc:StopSharing", ContentWebRTCShim);

addEventListener("pageshow", function(event) {
  if (event.target == content.document) {
    sendAsyncMessage("PageVisibility:Show", {
      persisted: event.persisted,
    });
  }
});
addEventListener("pagehide", function(event) {
  if (event.target == content.document) {
    sendAsyncMessage("PageVisibility:Hide", {
      persisted: event.persisted,
    });
  }
});

var PageMetadataMessenger = {
  init() {
    addMessageListener("PageMetadata:GetPageData", this);
    addMessageListener("PageMetadata:GetMicroformats", this);
  },
  receiveMessage(message) {
    switch (message.name) {
      case "PageMetadata:GetPageData": {
        let target = contextMenu.getTarget(message);
        let result = PageMetadata.getData(content.document, target);
        sendAsyncMessage("PageMetadata:PageDataResult", result);
        break;
      }
      case "PageMetadata:GetMicroformats": {
        let target = contextMenu.getTarget(message);
        let result = PageMetadata.getMicroformats(content.document, target);
        sendAsyncMessage("PageMetadata:MicroformatsResult", result);
        break;
      }
    }
  }
};
PageMetadataMessenger.init();

addEventListener("InstallBrowserTheme", LightWeightThemeWebInstallListener, false, true);
addEventListener("PreviewBrowserTheme", LightWeightThemeWebInstallListener, false, true);
addEventListener("ResetBrowserThemePreview", LightWeightThemeWebInstallListener, false, true);

let OfflineApps = {
  _docId: 0,
  _docIdMap: new Map(),

  _docManifestSet: new Set(),

  _observerAdded: false,
  registerWindow(aWindow) {
    if (!this._observerAdded) {
      this._observerAdded = true;
      Services.obs.addObserver(this, "offline-cache-update-completed", true);
    }
    let manifestURI = this._getManifestURI(aWindow);
    this._docManifestSet.add(manifestURI.spec);
  },

  handleEvent(event) {
    if (event.type == "MozApplicationManifest") {
      this.offlineAppRequested(event.originalTarget.defaultView);
    }
  },

  _getManifestURI(aWindow) {
    if (!aWindow.document.documentElement)
      return null;

    var attr = aWindow.document.documentElement.getAttribute("manifest");
    if (!attr)
      return null;

    try {
      return Services.io.newURI(attr, aWindow.document.characterSet,
                                Services.io.newURI(aWindow.location.href));
    } catch (e) {
      return null;
    }
  },

  offlineAppRequested(aContentWindow) {
    this.registerWindow(aContentWindow);
    if (!Services.prefs.getBoolPref("browser.offline-apps.notify")) {
      return;
    }

    let currentURI = aContentWindow.document.documentURIObject;
    // don't bother showing UI if the user has already made a decision
    if (Services.perms.testExactPermission(currentURI, "offline-app") != Services.perms.UNKNOWN_ACTION)
      return;

    try {
      if (Services.prefs.getBoolPref("offline-apps.allow_by_default")) {
        // all pages can use offline capabilities, no need to ask the user
        return;
      }
    } catch (e) {
      // this pref isn't set by default, ignore failures
    }
    let docId = ++this._docId;
    this._docIdMap.set(docId, Cu.getWeakReference(aContentWindow.document));
    sendAsyncMessage("OfflineApps:RequestPermission", {
      uri: currentURI.spec,
      docId,
    });
  },

  _startFetching(aDocument) {
    if (!aDocument.documentElement)
      return;

    let manifestURI = this._getManifestURI(aDocument.defaultView);
    if (!manifestURI)
      return;

    var updateService = Cc["@mozilla.org/offlinecacheupdate-service;1"].
                        getService(Ci.nsIOfflineCacheUpdateService);
    updateService.scheduleUpdate(manifestURI, aDocument.documentURIObject,
                                 aDocument.nodePrincipal, aDocument.defaultView);
  },

  receiveMessage(aMessage) {
    if (aMessage.name == "OfflineApps:StartFetching") {
      let doc = this._docIdMap.get(aMessage.data.docId);
      doc = doc && doc.get();
      if (doc) {
        this._startFetching(doc);
      }
      this._docIdMap.delete(aMessage.data.docId);
    }
  },

  observe(aSubject, aTopic, aState) {
    if (aTopic == "offline-cache-update-completed") {
      let cacheUpdate = aSubject.QueryInterface(Ci.nsIOfflineCacheUpdate);
      let uri = cacheUpdate.manifestURI;
      if (uri && this._docManifestSet.has(uri.spec)) {
        sendAsyncMessage("OfflineApps:CheckUsage", {uri: uri.spec});
      }
    }
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference]),
};

addEventListener("MozApplicationManifest", OfflineApps, false);
addMessageListener("OfflineApps:StartFetching", OfflineApps);
