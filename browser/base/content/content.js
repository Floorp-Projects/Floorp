/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This content script should work in any browser or iframe and should not
 * depend on the frame being contained in tabbrowser. */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/ContentWebRTC.jsm");
Cu.import("resource:///modules/ContentObservers.jsm");
Cu.import("resource://gre/modules/InlineSpellChecker.jsm");
Cu.import("resource://gre/modules/InlineSpellCheckerContent.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "E10SUtils",
  "resource:///modules/E10SUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ContentLinkHandler",
  "resource:///modules/ContentLinkHandler.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoginManagerContent",
  "resource://gre/modules/LoginManagerContent.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormLikeFactory",
  "resource://gre/modules/LoginManagerContent.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "InsecurePasswordUtils",
  "resource://gre/modules/InsecurePasswordUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PluginContent",
  "resource:///modules/PluginContent.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormSubmitObserver",
  "resource:///modules/FormSubmitObserver.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PageMetadata",
  "resource://gre/modules/PageMetadata.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUIUtils",
  "resource:///modules/PlacesUIUtils.jsm");
XPCOMUtils.defineLazyGetter(this, "PageMenuChild", function() {
  let tmp = {};
  Cu.import("resource://gre/modules/PageMenu.jsm", tmp);
  return new tmp.PageMenuChild();
});
XPCOMUtils.defineLazyModuleGetter(this, "Feeds",
  "resource:///modules/Feeds.jsm");

Cu.importGlobalProperties(["URL"]);

// TabChildGlobal
var global = this;

// Load the form validation popup handler
var formSubmitObserver = new FormSubmitObserver(content, this);

addMessageListener("ContextMenu:DoCustomCommand", function(message) {
  E10SUtils.wrapHandlingUserInput(
    content, message.data.handlingUserInput,
    () => PageMenuChild.executeMenu(message.data.generatedItemId));
});

addMessageListener("RemoteLogins:fillForm", function(message) {
  LoginManagerContent.receiveMessage(message, content);
});
addEventListener("DOMFormHasPassword", function(event) {
  LoginManagerContent.onDOMFormHasPassword(event, content);
  let formLike = FormLikeFactory.createFromForm(event.target);
  InsecurePasswordUtils.checkForInsecurePasswords(formLike);
});
addEventListener("DOMInputPasswordAdded", function(event) {
  LoginManagerContent.onDOMInputPasswordAdded(event, content);
  let formLike = FormLikeFactory.createFromField(event.target);
  InsecurePasswordUtils.checkForInsecurePasswords(formLike);
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

var handleContentContextMenu = function (event) {
  let defaultPrevented = event.defaultPrevented;
  if (!Services.prefs.getBoolPref("dom.event.contextmenu.enabled")) {
    let plugin = null;
    try {
      plugin = event.target.QueryInterface(Ci.nsIObjectLoadingContent);
    } catch (e) {}
    if (plugin && plugin.displayedType == Ci.nsIObjectLoadingContent.TYPE_PLUGIN) {
      // Don't open a context menu for plugins.
      return;
    }

    defaultPrevented = false;
  }

  if (defaultPrevented)
    return;

  let addonInfo = {};
  let subject = {
    event: event,
    addonInfo: addonInfo,
  };
  subject.wrappedJSObject = subject;
  Services.obs.notifyObservers(subject, "content-contextmenu", null);

  let doc = event.target.ownerDocument;
  let docLocation = doc.mozDocumentURIIfNotForErrorPages;
  docLocation = docLocation && docLocation.spec;
  let charSet = doc.characterSet;
  let baseURI = doc.baseURI;
  let referrer = doc.referrer;
  let referrerPolicy = doc.referrerPolicy;
  let frameOuterWindowID = doc.defaultView.QueryInterface(Ci.nsIInterfaceRequestor)
                                          .getInterface(Ci.nsIDOMWindowUtils)
                                          .outerWindowID;
  let loginFillInfo = LoginManagerContent.getFieldContext(event.target);

  // The same-origin check will be done in nsContextMenu.openLinkInTab.
  let parentAllowsMixedContent = !!docShell.mixedContentChannel;

  // get referrer attribute from clicked link and parse it
  // if per element referrer is enabled, the element referrer overrules
  // the document wide referrer
  if (Services.prefs.getBoolPref("network.http.enablePerElementReferrer")) {
    let referrerAttrValue = Services.netUtils.parseAttributePolicyString(event.target.
                            getAttribute("referrerpolicy"));
    if (referrerAttrValue !== Ci.nsIHttpChannel.REFERRER_POLICY_UNSET) {
      referrerPolicy = referrerAttrValue;
    }
  }

  let disableSetDesktopBg = null;
  // Media related cache info parent needs for saving
  let contentType = null;
  let contentDisposition = null;
  if (event.target.nodeType == Ci.nsIDOMNode.ELEMENT_NODE &&
      event.target instanceof Ci.nsIImageLoadingContent &&
      event.target.currentURI) {
    disableSetDesktopBg = disableSetDesktopBackground(event.target);

    try {
      let imageCache =
        Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools)
                                        .getImgCacheForDocument(doc);
      let props =
        imageCache.findEntryProperties(event.target.currentURI, doc);
      try {
        contentType = props.get("type", Ci.nsISupportsCString).data;
      } catch (e) {}
      try {
        contentDisposition =
          props.get("content-disposition", Ci.nsISupportsCString).data;
      } catch (e) {}
    } catch (e) {}
  }

  let selectionInfo = BrowserUtils.getSelectionDetails(content);

  let loadContext = docShell.QueryInterface(Ci.nsILoadContext);
  let userContextId = loadContext.originAttributes.userContextId;

  if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
    let editFlags = SpellCheckHelper.isEditable(event.target, content);
    let spellInfo;
    if (editFlags &
        (SpellCheckHelper.EDITABLE | SpellCheckHelper.CONTENTEDITABLE)) {
      spellInfo =
        InlineSpellCheckerContent.initContextMenu(event, editFlags, this);
    }

    // Set the event target first as the copy image command needs it to
    // determine what was context-clicked on. Then, update the state of the
    // commands on the context menu.
    docShell.contentViewer.QueryInterface(Ci.nsIContentViewerEdit)
            .setCommandNode(event.target);
    event.target.ownerGlobal.updateCommands("contentcontextmenu");

    let customMenuItems = PageMenuChild.build(event.target);
    let principal = doc.nodePrincipal;
    sendRpcMessage("contextmenu",
                   { editFlags, spellInfo, customMenuItems, addonInfo,
                     principal, docLocation, charSet, baseURI, referrer,
                     referrerPolicy, contentType, contentDisposition,
                     frameOuterWindowID, selectionInfo, disableSetDesktopBg,
                     loginFillInfo, parentAllowsMixedContent, userContextId },
                   { event, popupNode: event.target });
  }
  else {
    // Break out to the parent window and pass the add-on info along
    let browser = docShell.chromeEventHandler;
    let mainWin = browser.ownerGlobal;
    mainWin.gContextMenuContentData = {
      isRemote: false,
      event: event,
      popupNode: event.target,
      browser: browser,
      addonInfo: addonInfo,
      documentURIObject: doc.documentURIObject,
      docLocation: docLocation,
      charSet: charSet,
      referrer: referrer,
      referrerPolicy: referrerPolicy,
      contentType: contentType,
      contentDisposition: contentDisposition,
      selectionInfo: selectionInfo,
      disableSetDesktopBackground: disableSetDesktopBg,
      loginFillInfo,
      parentAllowsMixedContent,
      userContextId,
    };
  }
}

Cc["@mozilla.org/eventlistenerservice;1"]
  .getService(Ci.nsIEventListenerService)
  .addSystemEventListener(global, "contextmenu", handleContentContextMenu, false);

// Values for telemtery bins: see TLS_ERROR_REPORT_UI in Histograms.json
const TLS_ERROR_REPORT_TELEMETRY_UI_SHOWN = 0;
const TLS_ERROR_REPORT_TELEMETRY_EXPANDED = 1;
const TLS_ERROR_REPORT_TELEMETRY_SUCCESS  = 6;
const TLS_ERROR_REPORT_TELEMETRY_FAILURE  = 7;

const SEC_ERROR_BASE          = Ci.nsINSSErrorsService.NSS_SEC_ERROR_BASE;
const MOZILLA_PKIX_ERROR_BASE = Ci.nsINSSErrorsService.MOZILLA_PKIX_ERROR_BASE;

const SEC_ERROR_EXPIRED_CERTIFICATE                = SEC_ERROR_BASE + 11;
const SEC_ERROR_UNKNOWN_ISSUER                     = SEC_ERROR_BASE + 13;
const SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE         = SEC_ERROR_BASE + 30;
const SEC_ERROR_OCSP_FUTURE_RESPONSE               = SEC_ERROR_BASE + 131;
const SEC_ERROR_OCSP_OLD_RESPONSE                  = SEC_ERROR_BASE + 132;
const MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE = MOZILLA_PKIX_ERROR_BASE + 5;
const MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE = MOZILLA_PKIX_ERROR_BASE + 6;

const PREF_BLOCKLIST_CLOCK_SKEW_SECONDS = "services.blocklist.clock_skew_seconds";

const PREF_SSL_IMPACT_ROOTS = ["security.tls.version.", "security.ssl3."];

const PREF_SSL_IMPACT = PREF_SSL_IMPACT_ROOTS.reduce((prefs, root) => {
  return prefs.concat(Services.prefs.getChildList(root));
}, []);


function getSerializedSecurityInfo(docShell) {
  let serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                    .getService(Ci.nsISerializationHelper);

  let securityInfo = docShell.failedChannel && docShell.failedChannel.securityInfo;
  if (!securityInfo) {
    return "";
  }
  securityInfo.QueryInterface(Ci.nsITransportSecurityInfo)
              .QueryInterface(Ci.nsISerializable);

  return serhelper.serializeToString(securityInfo);
}

var AboutNetAndCertErrorListener = {
  init: function(chromeGlobal) {
    addMessageListener("CertErrorDetails", this);
    chromeGlobal.addEventListener('AboutNetErrorLoad', this, false, true);
    chromeGlobal.addEventListener('AboutNetErrorSetAutomatic', this, false, true);
    chromeGlobal.addEventListener('AboutNetErrorOverride', this, false, true);
    chromeGlobal.addEventListener('AboutNetErrorResetPreferences', this, false, true);
  },

  get isAboutNetError() {
    return content.document.documentURI.startsWith("about:neterror");
  },

  get isAboutCertError() {
    return content.document.documentURI.startsWith("about:certerror");
  },

  receiveMessage: function(msg) {
    if (!this.isAboutCertError) {
      return;
    }

    switch (msg.name) {
      case "CertErrorDetails":
        this.onCertErrorDetails(msg);
        break;
    }
  },

  onCertErrorDetails(msg) {
    let div = content.document.getElementById("certificateErrorText");
    div.textContent = msg.data.info;
    let learnMoreLink = content.document.getElementById("learnMoreLink");
    let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");

    switch (msg.data.code) {
      case SEC_ERROR_UNKNOWN_ISSUER:
        learnMoreLink.href = baseURL  + "security-error";
        break;

      // in case the certificate expired we make sure the system clock
      // matches settings server (kinto) time
      case SEC_ERROR_EXPIRED_CERTIFICATE:
      case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
      case SEC_ERROR_OCSP_FUTURE_RESPONSE:
      case SEC_ERROR_OCSP_OLD_RESPONSE:
      case MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE:
      case MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE:

        // use blocklist stats if available
        if (Services.prefs.getPrefType(PREF_BLOCKLIST_CLOCK_SKEW_SECONDS)) {
          let difference = Services.prefs.getIntPref(PREF_BLOCKLIST_CLOCK_SKEW_SECONDS);

          // if the difference is more than a day
          if (Math.abs(difference) > 60 * 60 * 24) {
            let formatter = new Intl.DateTimeFormat();
            let systemDate = formatter.format(new Date());
            // negative difference means local time is behind server time
            let actualDate = formatter.format(new Date(Date.now() - difference * 1000));

            content.document.getElementById("wrongSystemTime_URL")
              .textContent = content.document.location.hostname;
            content.document.getElementById("wrongSystemTime_systemDate")
              .textContent = systemDate;
            content.document.getElementById("wrongSystemTime_actualDate")
              .textContent = actualDate;

            content.document.getElementById("errorShortDesc")
              .style.display = "none";
            content.document.getElementById("wrongSystemTimePanel")
              .style.display = "block";
          }
        }
        learnMoreLink.href = baseURL  + "time-errors";
        break;
    }
  },

  handleEvent: function(aEvent) {
    if (!this.isAboutNetError && !this.isAboutCertError) {
      return;
    }

    switch (aEvent.type) {
    case "AboutNetErrorLoad":
      this.onPageLoad(aEvent);
      break;
    case "AboutNetErrorSetAutomatic":
      this.onSetAutomatic(aEvent);
      break;
    case "AboutNetErrorOverride":
      this.onOverride(aEvent);
      break;
    case "AboutNetErrorResetPreferences":
      this.onResetPreferences(aEvent);
      break;
    }
  },

  changedCertPrefs: function () {
    for (let prefName of PREF_SSL_IMPACT) {
      if (Services.prefs.prefHasUserValue(prefName)) {
        return true;
      }
    }

    return false;
  },

  onPageLoad: function(evt) {
    if (this.isAboutCertError) {
      let originalTarget = evt.originalTarget;
      let ownerDoc = originalTarget.ownerDocument;
      ClickEventHandler.onCertError(originalTarget, ownerDoc);
    }

    let automatic = Services.prefs.getBoolPref("security.ssl.errorReporting.automatic");
    content.dispatchEvent(new content.CustomEvent("AboutNetErrorOptions", {
      detail: JSON.stringify({
        enabled: Services.prefs.getBoolPref("security.ssl.errorReporting.enabled"),
        changedCertPrefs: this.changedCertPrefs(),
        automatic: automatic
      })
    }));

    sendAsyncMessage("Browser:SSLErrorReportTelemetry",
                     {reportStatus: TLS_ERROR_REPORT_TELEMETRY_UI_SHOWN});
  },


  onResetPreferences: function(evt) {
    sendAsyncMessage("Browser:ResetSSLPreferences");
  },

  onSetAutomatic: function(evt) {
    sendAsyncMessage("Browser:SetSSLErrorReportAuto", {
      automatic: evt.detail
    });

    // if we're enabling reports, send a report for this failure
    if (evt.detail) {
      let {host, port} = content.document.mozDocumentURIIfNotForErrorPages;
      sendAsyncMessage("Browser:SendSSLErrorReport", {
        uri: { host, port },
        securityInfo: getSerializedSecurityInfo(docShell),
      });

    }
  },

  onOverride: function(evt) {
    let {host, port} = content.document.mozDocumentURIIfNotForErrorPages;
    sendAsyncMessage("Browser:OverrideWeakCrypto", { uri: {host, port} });
  }
}

AboutNetAndCertErrorListener.init(this);


var ClickEventHandler = {
  init: function init() {
    Cc["@mozilla.org/eventlistenerservice;1"]
      .getService(Ci.nsIEventListenerService)
      .addSystemEventListener(global, "click", this, true);
  },

  handleEvent: function(event) {
    if (!event.isTrusted || event.defaultPrevented || event.button == 2) {
      return;
    }

    let originalTarget = event.originalTarget;
    let ownerDoc = originalTarget.ownerDocument;
    if (!ownerDoc) {
      return;
    }

    // Handle click events from about pages
    if (ownerDoc.documentURI.startsWith("about:certerror")) {
      this.onCertError(originalTarget, ownerDoc);
      return;
    } else if (ownerDoc.documentURI.startsWith("about:blocked")) {
      this.onAboutBlocked(originalTarget, ownerDoc);
      return;
    } else if (ownerDoc.documentURI.startsWith("about:neterror")) {
      this.onAboutNetError(event, ownerDoc.documentURI);
      return;
    }

    let [href, node, principal] = this._hrefAndLinkNodeForClickEvent(event);

    // get referrer attribute from clicked link and parse it
    // if per element referrer is enabled, the element referrer overrules
    // the document wide referrer
    let referrerPolicy = ownerDoc.referrerPolicy;
    if (Services.prefs.getBoolPref("network.http.enablePerElementReferrer") &&
        node) {
      let referrerAttrValue = Services.netUtils.parseAttributePolicyString(node.
                              getAttribute("referrerpolicy"));
      if (referrerAttrValue !== Ci.nsIHttpChannel.REFERRER_POLICY_UNSET) {
        referrerPolicy = referrerAttrValue;
      }
    }

    let json = { button: event.button, shiftKey: event.shiftKey,
                 ctrlKey: event.ctrlKey, metaKey: event.metaKey,
                 altKey: event.altKey, href: null, title: null,
                 bookmark: false, referrerPolicy: referrerPolicy,
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
      json.noReferrer = BrowserUtils.linkHasNoReferrer(node)

      // Check if the link needs to be opened with mixed content allowed.
      // Only when the owner doc has |mixedContentChannel| and the same origin
      // should we allow mixed content.
      json.allowMixedContent = false;
      let docshell = ownerDoc.defaultView.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIWebNavigation)
                             .QueryInterface(Ci.nsIDocShell);
      if (docShell.mixedContentChannel) {
        const sm = Services.scriptSecurityManager;
        try {
          let targetURI = BrowserUtils.makeURI(href);
          sm.checkSameOriginURI(docshell.mixedContentChannel.URI, targetURI, false);
          json.allowMixedContent = true;
        } catch (e) {}
      }

      sendAsyncMessage("Content:Click", json);
      return;
    }

    // This might be middle mouse navigation.
    if (event.button == 1) {
      sendAsyncMessage("Content:Click", json);
    }
  },

  onCertError: function (targetElement, ownerDoc) {
    let docShell = ownerDoc.defaultView.QueryInterface(Ci.nsIInterfaceRequestor)
                                       .getInterface(Ci.nsIWebNavigation)
                                       .QueryInterface(Ci.nsIDocShell);
    sendAsyncMessage("Browser:CertExceptionError", {
      location: ownerDoc.location.href,
      elementId: targetElement.getAttribute("id"),
      isTopFrame: (ownerDoc.defaultView.parent === ownerDoc.defaultView),
      securityInfoAsString: getSerializedSecurityInfo(docShell),
    });
  },

  onAboutBlocked: function (targetElement, ownerDoc) {
    var reason = 'phishing';
    if (/e=malwareBlocked/.test(ownerDoc.documentURI)) {
      reason = 'malware';
    } else if (/e=unwantedBlocked/.test(ownerDoc.documentURI)) {
      reason = 'unwanted';
    }
    sendAsyncMessage("Browser:SiteBlockedError", {
      location: ownerDoc.location.href,
      reason: reason,
      elementId: targetElement.getAttribute("id"),
      isTopFrame: (ownerDoc.defaultView.parent === ownerDoc.defaultView)
    });
  },

  onAboutNetError: function (event, documentURI) {
    let elmId = event.originalTarget.getAttribute("id");
    if (elmId == "returnButton") {
      sendAsyncMessage("Browser:SSLErrorGoBack", {});
      return;
    }
    if (elmId != "errorTryAgain" || !/e=netOffline/.test(documentURI)) {
      return;
    }
    // browser front end will handle clearing offline mode and refreshing
    // the page *if* we're in offline mode now. Otherwise let the error page
    // handle the click.
    if (Services.io.offline) {
      event.preventDefault();
      sendAsyncMessage("Browser:EnableOnlineMode", {});
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
  _hrefAndLinkNodeForClickEvent: function(event) {
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
    return [href ? BrowserUtils.makeURI(href, null, baseURI).spec : null, null,
            node && node.ownerDocument.nodePrincipal];
  }
};
ClickEventHandler.init();

ContentLinkHandler.init(this);

// TODO: Load this lazily so the JSM is run only if a relevant event/message fires.
var pluginContent = new PluginContent(global);

addEventListener("DOMWebNotificationClicked", function(event) {
  sendAsyncMessage("DOMWebNotificationClicked", {});
}, false);

addEventListener("DOMServiceWorkerFocusClient", function(event) {
  sendAsyncMessage("DOMServiceWorkerFocusClient", {});
}, false);

ContentWebRTC.init();
addMessageListener("rtcpeer:Allow", ContentWebRTC);
addMessageListener("rtcpeer:Deny", ContentWebRTC);
addMessageListener("webrtc:Allow", ContentWebRTC);
addMessageListener("webrtc:Deny", ContentWebRTC);
addMessageListener("webrtc:StopSharing", ContentWebRTC);
addMessageListener("webrtc:StartBrowserSharing", () => {
  let windowID = content.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
  sendAsyncMessage("webrtc:response:StartBrowserSharing", {
    windowID: windowID
  });
});

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
        let target = message.objects.target;
        let result = PageMetadata.getData(content.document, target);
        sendAsyncMessage("PageMetadata:PageDataResult", result);
        break;
      }
      case "PageMetadata:GetMicroformats": {
        let target = message.objects.target;
        let result = PageMetadata.getMicroformats(content.document, target);
        sendAsyncMessage("PageMetadata:MicroformatsResult", result);
        break;
      }
    }
  }
}
PageMetadataMessenger.init();

addEventListener("ActivateSocialFeature", function (aEvent) {
  let document = content.document;
  let dwu = content.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils);
  if (!dwu.isHandlingUserInput) {
    Cu.reportError("attempt to activate provider without user input from " + document.nodePrincipal.origin);
    return;
  }

  let node = aEvent.target;
  let ownerDocument = node.ownerDocument;
  let data = node.getAttribute("data-service");
  if (data) {
    try {
      data = JSON.parse(data);
    } catch (e) {
      Cu.reportError("Social Service manifest parse error: " + e);
      return;
    }
  } else {
    Cu.reportError("Social Service manifest not available");
    return;
  }

  sendAsyncMessage("Social:Activation", {
    url: ownerDocument.location.href,
    origin: ownerDocument.nodePrincipal.origin,
    manifest: data
  });
}, true, true);

addMessageListener("ContextMenu:SaveVideoFrameAsImage", (message) => {
  let video = message.objects.target;
  let canvas = content.document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
  canvas.width = video.videoWidth;
  canvas.height = video.videoHeight;

  let ctxDraw = canvas.getContext("2d");
  ctxDraw.drawImage(video, 0, 0);
  sendAsyncMessage("ContextMenu:SaveVideoFrameAsImage:Result", {
    dataURL: canvas.toDataURL("image/jpeg", ""),
  });
});

addMessageListener("ContextMenu:MediaCommand", (message) => {
  let media = message.objects.element;

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
      if (content.document.fullscreenEnabled)
        media.requestFullscreen();
      break;
  }
});

addMessageListener("ContextMenu:Canvas:ToBlobURL", (message) => {
  message.objects.target.toBlob((blob) => {
    let blobURL = URL.createObjectURL(blob);
    sendAsyncMessage("ContextMenu:Canvas:ToBlobURL:Result", { blobURL });
  });
});

addMessageListener("ContextMenu:ReloadFrame", (message) => {
  message.objects.target.ownerDocument.location.reload();
});

addMessageListener("ContextMenu:ReloadImage", (message) => {
  let image = message.objects.target;
  if (image instanceof Ci.nsIImageLoadingContent)
    image.forceReload();
});

addMessageListener("ContextMenu:BookmarkFrame", (message) => {
  let frame = message.objects.target.ownerDocument;
  sendAsyncMessage("ContextMenu:BookmarkFrame:Result",
                   { title: frame.title,
                     description: PlacesUIUtils.getDescriptionFromDocument(frame) });
});

addMessageListener("ContextMenu:SearchFieldBookmarkData", (message) => {
  let node = message.objects.target;

  let charset = node.ownerDocument.characterSet;

  let formBaseURI = BrowserUtils.makeURI(node.form.baseURI,
                                         charset);

  let formURI = BrowserUtils.makeURI(node.form.getAttribute("action"),
                                     charset,
                                     formBaseURI);

  let spec = formURI.spec;

  let isURLEncoded =
               (node.form.method.toUpperCase() == "POST"
                && (node.form.enctype == "application/x-www-form-urlencoded" ||
                    node.form.enctype == ""));

  let title = node.ownerDocument.title;
  let description = PlacesUIUtils.getDescriptionFromDocument(node.ownerDocument);

  let formData = [];

  function escapeNameValuePair(aName, aValue, aIsFormUrlEncoded) {
    if (aIsFormUrlEncoded) {
      return escape(aName + "=" + aValue);
    }
    return escape(aName) + "=" + escape(aValue);
  }

  for (let el of node.form.elements) {
    if (!el.type) // happens with fieldsets
      continue;

    if (el == node) {
      formData.push((isURLEncoded) ? escapeNameValuePair(el.name, "%s", true) :
                                     // Don't escape "%s", just append
                                     escapeNameValuePair(el.name, "", false) + "%s");
      continue;
    }

    let type = el.type.toLowerCase();

    if (((el instanceof content.HTMLInputElement && el.mozIsTextField(true)) ||
        type == "hidden" || type == "textarea") ||
        ((type == "checkbox" || type == "radio") && el.checked)) {
      formData.push(escapeNameValuePair(el.name, el.value, isURLEncoded));
    } else if (el instanceof content.HTMLSelectElement && el.selectedIndex >= 0) {
      for (let j=0; j < el.options.length; j++) {
        if (el.options[j].selected)
          formData.push(escapeNameValuePair(el.name, el.options[j].value,
                                            isURLEncoded));
      }
    }
  }

  let postData;

  if (isURLEncoded)
    postData = formData.join("&");
  else {
    let separator = spec.includes("?") ? "&" : "?";
    spec += separator + formData.join("&");
  }

  sendAsyncMessage("ContextMenu:SearchFieldBookmarkData:Result",
                   { spec, title, description, postData, charset });
});

addMessageListener("Bookmarks:GetPageDetails", (message) => {
  let doc = content.document;
  let isErrorPage = /^about:(neterror|certerror|blocked)/.test(doc.documentURI);
  sendAsyncMessage("Bookmarks:GetPageDetails:Result",
                   { isErrorPage: isErrorPage,
                     description: PlacesUIUtils.getDescriptionFromDocument(doc) });
});

var LightWeightThemeWebInstallListener = {
  _previewWindow: null,

  init: function() {
    addEventListener("InstallBrowserTheme", this, false, true);
    addEventListener("PreviewBrowserTheme", this, false, true);
    addEventListener("ResetBrowserThemePreview", this, false, true);
  },

  handleEvent: function (event) {
    switch (event.type) {
      case "InstallBrowserTheme": {
        sendAsyncMessage("LightWeightThemeWebInstaller:Install", {
          baseURI: event.target.baseURI,
          themeData: event.target.getAttribute("data-browsertheme"),
        });
        break;
      }
      case "PreviewBrowserTheme": {
        sendAsyncMessage("LightWeightThemeWebInstaller:Preview", {
          baseURI: event.target.baseURI,
          themeData: event.target.getAttribute("data-browsertheme"),
        });
        this._previewWindow = event.target.ownerGlobal;
        this._previewWindow.addEventListener("pagehide", this, true);
        break;
      }
      case "pagehide": {
        sendAsyncMessage("LightWeightThemeWebInstaller:ResetPreview");
        this._resetPreviewWindow();
        break;
      }
      case "ResetBrowserThemePreview": {
        if (this._previewWindow) {
          sendAsyncMessage("LightWeightThemeWebInstaller:ResetPreview",
                           {baseURI: event.target.baseURI});
          this._resetPreviewWindow();
        }
        break;
      }
    }
  },

  _resetPreviewWindow: function () {
    this._previewWindow.removeEventListener("pagehide", this, true);
    this._previewWindow = null;
  }
};

LightWeightThemeWebInstallListener.init();

function disableSetDesktopBackground(aTarget) {
  // Disable the Set as Desktop Background menu item if we're still trying
  // to load the image or the load failed.
  if (!(aTarget instanceof Ci.nsIImageLoadingContent))
    return true;

  if (("complete" in aTarget) && !aTarget.complete)
    return true;

  if (aTarget.currentURI.schemeIs("javascript"))
    return true;

  let request = aTarget.QueryInterface(Ci.nsIImageLoadingContent)
                       .getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
  if (!request)
    return true;

  return false;
}

addMessageListener("ContextMenu:SetAsDesktopBackground", (message) => {
  let target = message.objects.target;

  // Paranoia: check disableSetDesktopBackground again, in case the
  // image changed since the context menu was initiated.
  let disable = disableSetDesktopBackground(target);

  if (!disable) {
    try {
      BrowserUtils.urlSecurityCheck(target.currentURI.spec, target.ownerDocument.nodePrincipal);
      let canvas = content.document.createElement("canvas");
      canvas.width = target.naturalWidth;
      canvas.height = target.naturalHeight;
      let ctx = canvas.getContext("2d");
      ctx.drawImage(target, 0, 0);
      let dataUrl = canvas.toDataURL();
      sendAsyncMessage("ContextMenu:SetAsDesktopBackground:Result",
                       { dataUrl });
    }
    catch (e) {
      Cu.reportError(e);
      disable = true;
    }
  }

  if (disable)
    sendAsyncMessage("ContextMenu:SetAsDesktopBackground:Result", { disable });
});

var PageInfoListener = {

  init: function() {
    addMessageListener("PageInfo:getData", this);
  },

  receiveMessage: function(message) {
    let strings = message.data.strings;
    let window;
    let document;

    let frameOuterWindowID = message.data.frameOuterWindowID;

    // If inside frame then get the frame's window and document.
    if (frameOuterWindowID) {
      window = Services.wm.getOuterWindowWithId(frameOuterWindowID);
      document = window.document;
    }
    else {
      window = content.window;
      document = content.document;
    }

    let imageElement = message.objects.imageElement;

    let pageInfoData = {metaViewRows: this.getMetaInfo(document),
                        docInfo: this.getDocumentInfo(document),
                        feeds: this.getFeedsInfo(document, strings),
                        windowInfo: this.getWindowInfo(window),
                        imageInfo: this.getImageInfo(imageElement)};

    sendAsyncMessage("PageInfo:data", pageInfoData);

    // Separate step so page info dialog isn't blank while waiting for this to finish.
    this.getMediaInfo(document, window, strings);
  },

  getImageInfo: function(imageElement) {
    let imageInfo = null;
    if (imageElement) {
      imageInfo = {
        currentSrc: imageElement.currentSrc,
        width: imageElement.width,
        height: imageElement.height,
        imageText: imageElement.title || imageElement.alt
      };
    }
    return imageInfo;
  },

  getMetaInfo: function(document) {
    let metaViewRows = [];

    // Get the meta tags from the page.
    let metaNodes = document.getElementsByTagName("meta");

    for (let metaNode of metaNodes) {
      metaViewRows.push([metaNode.name || metaNode.httpEquiv || metaNode.getAttribute("property"),
                        metaNode.content]);
    }

    return metaViewRows;
  },

  getWindowInfo: function(window) {
    let windowInfo = {};
    windowInfo.isTopWindow = window == window.top;

    let hostName = null;
    try {
      hostName = window.location.host;
    }
    catch (exception) { }

    windowInfo.hostName = hostName;
    return windowInfo;
  },

  getDocumentInfo: function(document) {
    let docInfo = {};
    docInfo.title = document.title;
    docInfo.location = document.location.toString();
    docInfo.referrer = document.referrer;
    docInfo.compatMode = document.compatMode;
    docInfo.contentType = document.contentType;
    docInfo.characterSet = document.characterSet;
    docInfo.lastModified = document.lastModified;
    docInfo.principal = document.nodePrincipal;

    let documentURIObject = {};
    documentURIObject.spec = document.documentURIObject.spec;
    documentURIObject.originCharset = document.documentURIObject.originCharset;
    docInfo.documentURIObject = documentURIObject;

    docInfo.isContentWindowPrivate = PrivateBrowsingUtils.isContentWindowPrivate(content);

    return docInfo;
  },

  getFeedsInfo: function(document, strings) {
    let feeds = [];
    // Get the feeds from the page.
    let linkNodes = document.getElementsByTagName("link");
    let length = linkNodes.length;
    for (let i = 0; i < length; i++) {
      let link = linkNodes[i];
      if (!link.href) {
        continue;
      }
      let rel = link.rel && link.rel.toLowerCase();
      let rels = {};

      if (rel) {
        for (let relVal of rel.split(/\s+/)) {
          rels[relVal] = true;
        }
      }

      if (rels.feed || (link.type && rels.alternate && !rels.stylesheet)) {
        let type = Feeds.isValidFeed(link, document.nodePrincipal, "feed" in rels);
        if (type) {
          type = strings[type] || strings["application/rss+xml"];
          feeds.push([link.title, type, link.href]);
        }
      }
    }
    return feeds;
  },

  // Only called once to get the media tab's media elements from the content page.
  getMediaInfo: function(document, window, strings)
  {
    let frameList = this.goThroughFrames(document, window);
    Task.spawn(() => this.processFrames(document, frameList, strings));
  },

  goThroughFrames: function(document, window)
  {
    let frameList = [document];
    if (window && window.frames.length > 0) {
      let num = window.frames.length;
      for (let i = 0; i < num; i++) {
        // Recurse through the frames.
        frameList.concat(this.goThroughFrames(window.frames[i].document,
                                              window.frames[i]));
      }
    }
    return frameList;
  },

  processFrames: function*(document, frameList, strings)
  {
    let nodeCount = 0;
    for (let doc of frameList) {
      let iterator = doc.createTreeWalker(doc, content.NodeFilter.SHOW_ELEMENT);

      // Goes through all the elements on the doc. imageViewRows takes only the media elements.
      while (iterator.nextNode()) {
        let mediaItems = this.getMediaItems(document, strings, iterator.currentNode);

        if (mediaItems.length) {
          sendAsyncMessage("PageInfo:mediaData",
                           {mediaItems, isComplete: false});
        }

        if (++nodeCount % 500 == 0) {
          // setTimeout every 500 elements so we don't keep blocking the content process.
          yield new Promise(resolve => setTimeout(resolve, 10));
        }
      }
    }
    // Send that page info media fetching has finished.
    sendAsyncMessage("PageInfo:mediaData", {isComplete: true});
  },

  getMediaItems: function(document, strings, elem)
  {
    // Check for images defined in CSS (e.g. background, borders)
    let computedStyle = elem.ownerGlobal.getComputedStyle(elem);
    // A node can have multiple media items associated with it - for example,
    // multiple background images.
    let mediaItems = [];

    let addImage = (url, type, alt, elem, isBg) => {
      let element = this.serializeElementInfo(document, url, type, alt, elem, isBg);
      mediaItems.push([url, type, alt, element, isBg]);
    };

    if (computedStyle) {
      let addImgFunc = (label, val) => {
        if (val.primitiveType == content.CSSPrimitiveValue.CSS_URI) {
          addImage(val.getStringValue(), label, strings.notSet, elem, true);
        }
        else if (val.primitiveType == content.CSSPrimitiveValue.CSS_STRING) {
          // This is for -moz-image-rect.
          // TODO: Reimplement once bug 714757 is fixed.
          let strVal = val.getStringValue();
          if (strVal.search(/^.*url\(\"?/) > -1) {
            let url = strVal.replace(/^.*url\(\"?/, "").replace(/\"?\).*$/, "");
            addImage(url, label, strings.notSet, elem, true);
          }
        }
        else if (val.cssValueType == content.CSSValue.CSS_VALUE_LIST) {
          // Recursively resolve multiple nested CSS value lists.
          for (let i = 0; i < val.length; i++) {
            addImgFunc(label, val.item(i));
          }
        }
      };

      addImgFunc(strings.mediaBGImg, computedStyle.getPropertyCSSValue("background-image"));
      addImgFunc(strings.mediaBorderImg, computedStyle.getPropertyCSSValue("border-image-source"));
      addImgFunc(strings.mediaListImg, computedStyle.getPropertyCSSValue("list-style-image"));
      addImgFunc(strings.mediaCursor, computedStyle.getPropertyCSSValue("cursor"));
    }

    // One swi^H^H^Hif-else to rule them all.
    if (elem instanceof content.HTMLImageElement) {
      addImage(elem.src, strings.mediaImg,
               (elem.hasAttribute("alt")) ? elem.alt : strings.notSet, elem, false);
    }
    else if (elem instanceof content.SVGImageElement) {
      try {
        // Note: makeURLAbsolute will throw if either the baseURI is not a valid URI
        //       or the URI formed from the baseURI and the URL is not a valid URI.
        let href = makeURLAbsolute(elem.baseURI, elem.href.baseVal);
        addImage(href, strings.mediaImg, "", elem, false);
      } catch (e) { }
    }
    else if (elem instanceof content.HTMLVideoElement) {
      addImage(elem.currentSrc, strings.mediaVideo, "", elem, false);
    }
    else if (elem instanceof content.HTMLAudioElement) {
      addImage(elem.currentSrc, strings.mediaAudio, "", elem, false);
    }
    else if (elem instanceof content.HTMLLinkElement) {
      if (elem.rel && /\bicon\b/i.test(elem.rel)) {
        addImage(elem.href, strings.mediaLink, "", elem, false);
      }
    }
    else if (elem instanceof content.HTMLInputElement || elem instanceof content.HTMLButtonElement) {
      if (elem.type.toLowerCase() == "image") {
        addImage(elem.src, strings.mediaInput,
                 (elem.hasAttribute("alt")) ? elem.alt : strings.notSet, elem, false);
      }
    }
    else if (elem instanceof content.HTMLObjectElement) {
      addImage(elem.data, strings.mediaObject, this.getValueText(elem), elem, false);
    }
    else if (elem instanceof content.HTMLEmbedElement) {
      addImage(elem.src, strings.mediaEmbed, "", elem, false);
    }

    return mediaItems;
  },

  /**
   * Set up a JSON element object with all the instanceOf and other infomation that
   * makePreview in pageInfo.js uses to figure out how to display the preview.
   */

  serializeElementInfo: function(document, url, type, alt, item, isBG)
  {
    let result = {};

    let imageText;
    if (!isBG &&
        !(item instanceof content.SVGImageElement) &&
        !(document instanceof content.ImageDocument)) {
      imageText = item.title || item.alt;

      if (!imageText && !(item instanceof content.HTMLImageElement)) {
        imageText = this.getValueText(item);
      }
    }

    result.imageText = imageText;
    result.longDesc = item.longDesc;
    result.numFrames = 1;

    if (item instanceof content.HTMLObjectElement ||
      item instanceof content.HTMLEmbedElement ||
      item instanceof content.HTMLLinkElement) {
      result.mimeType = item.type;
    }

    if (!result.mimeType && !isBG && item instanceof Ci.nsIImageLoadingContent) {
      // Interface for image loading content.
      let imageRequest = item.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
      if (imageRequest) {
        result.mimeType = imageRequest.mimeType;
        let image = !(imageRequest.imageStatus & imageRequest.STATUS_ERROR) && imageRequest.image;
        if (image) {
          result.numFrames = image.numFrames;
        }
      }
    }

    // If we have a data url, get the MIME type from the url.
    if (!result.mimeType && url.startsWith("data:")) {
      let dataMimeType = /^data:(image\/[^;,]+)/i.exec(url);
      if (dataMimeType)
        result.mimeType = dataMimeType[1].toLowerCase();
    }

    result.HTMLLinkElement = item instanceof content.HTMLLinkElement;
    result.HTMLInputElement = item instanceof content.HTMLInputElement;
    result.HTMLImageElement = item instanceof content.HTMLImageElement;
    result.HTMLObjectElement = item instanceof content.HTMLObjectElement;
    result.SVGImageElement = item instanceof content.SVGImageElement;
    result.HTMLVideoElement = item instanceof content.HTMLVideoElement;
    result.HTMLAudioElement = item instanceof content.HTMLAudioElement;

    if (isBG) {
      // Items that are showing this image as a background
      // image might not necessarily have a width or height,
      // so we'll dynamically generate an image and send up the
      // natural dimensions.
      let img = content.document.createElement("img");
      img.src = url;
      result.naturalWidth = img.naturalWidth;
      result.naturalHeight = img.naturalHeight;
    } else {
      // Otherwise, we can use the current width and height
      // of the image.
      result.width = item.width;
      result.height = item.height;
    }

    if (item instanceof content.SVGImageElement) {
      result.SVGImageElementWidth = item.width.baseVal.value;
      result.SVGImageElementHeight = item.height.baseVal.value;
    }

    result.baseURI = item.baseURI;

    return result;
  },

  //******** Other Misc Stuff
  // Modified from the Links Panel v2.3, http://segment7.net/mozilla/links/links.html
  // parse a node to extract the contents of the node
  getValueText: function(node)
  {

    let valueText = "";

    // Form input elements don't generally contain information that is useful to our callers, so return nothing.
    if (node instanceof content.HTMLInputElement ||
        node instanceof content.HTMLSelectElement ||
        node instanceof content.HTMLTextAreaElement) {
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
      }
      // And elements can have more text inside them.
      else if (nodeType == content.Node.ELEMENT_NODE) {
        // Images are special, we want to capture the alt text as if the image weren't there.
        if (childNode instanceof content.HTMLImageElement) {
          valueText += " " + this.getAltText(childNode);
        }
        else {
          valueText += " " + this.getValueText(childNode);
        }
      }
    }

    return this.stripWS(valueText);
  },

  // Copied from the Links Panel v2.3, http://segment7.net/mozilla/links/links.html.
  // Traverse the tree in search of an img or area element and grab its alt tag.
  getAltText: function(node)
  {
    let altText = "";

    if (node.alt) {
      return node.alt;
    }
    let length = node.childNodes.length;
    for (let i = 0; i < length; i++) {
      if ((altText = this.getAltText(node.childNodes[i]) != undefined)) { // stupid js warning...
        return altText;
      }
    }
    return "";
  },

  // Copied from the Links Panel v2.3, http://segment7.net/mozilla/links/links.html.
  // Strip leading and trailing whitespace, and replace multiple consecutive whitespace characters with a single space.
  stripWS: function(text)
  {
    let middleRE = /\s+/g;
    let endRE = /(^\s+)|(\s+$)/g;

    text = text.replace(middleRE, " ");
    return text.replace(endRE, "");
  }
};
PageInfoListener.init();

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
      var contentURI = BrowserUtils.makeURI(aWindow.location.href, null, null);
      return BrowserUtils.makeURI(attr, aWindow.document.characterSet, contentURI);
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
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),
};

addEventListener("MozApplicationManifest", OfflineApps, false);
addMessageListener("OfflineApps:StartFetching", OfflineApps);
