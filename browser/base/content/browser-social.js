/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */
/* global OpenGraphBuilder:false, DynamicResizeWatcher:false, Utils:false*/

// the "exported" symbols
var SocialUI,
    SocialShare,
    SocialActivationListener;

(function() {
"use strict";

XPCOMUtils.defineLazyGetter(this, "OpenGraphBuilder", function() {
  let tmp = {};
  Cu.import("resource:///modules/Social.jsm", tmp);
  return tmp.OpenGraphBuilder;
});

XPCOMUtils.defineLazyGetter(this, "DynamicResizeWatcher", function() {
  let tmp = {};
  Cu.import("resource:///modules/Social.jsm", tmp);
  return tmp.DynamicResizeWatcher;
});

let messageManager = window.messageManager;
let openUILinkIn = window.openUILinkIn;

SocialUI = {
  _initialized: false,

  // Called on delayed startup to initialize the UI
  init: function SocialUI_init() {
    if (this._initialized) {
      return;
    }
    let mm = window.getGroupMessageManager("social");
    mm.loadFrameScript("chrome://browser/content/content.js", true);
    mm.loadFrameScript("chrome://browser/content/social-content.js", true);

    Services.obs.addObserver(this, "social:providers-changed");

    CustomizableUI.addListener(this);
    SocialActivationListener.init();

    Social.init().then((update) => {
      if (update)
        this._providersChanged();
    });

    this._initialized = true;
  },

  // Called on window unload
  uninit: function SocialUI_uninit() {
    if (!this._initialized) {
      return;
    }
    Services.obs.removeObserver(this, "social:providers-changed");

    CustomizableUI.removeListener(this);
    SocialActivationListener.uninit();

    this._initialized = false;
  },

  observe: function SocialUI_observe(subject, topic, data) {
    switch (topic) {
      case "social:providers-changed":
        this._providersChanged();
        break;
    }
  },

  _providersChanged() {
    SocialShare.populateProviderMenu();
  },

  showLearnMore() {
    let url = Services.urlFormatter.formatURLPref("app.support.baseURL") + "social-api";
    openUILinkIn(url, "tab");
  },

  closeSocialPanelForLinkTraversal(target, linkNode) {
    // No need to close the panel if this traversal was not retargeted
    if (target == "" || target == "_self")
      return;

    // Check to see whether this link traversal was in a social panel
    let win = linkNode.ownerGlobal;
    let container = win.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIWebNavigation)
                                  .QueryInterface(Ci.nsIDocShell)
                                  .chromeEventHandler;
    let containerParent = container.parentNode;
    if (containerParent.classList.contains("social-panel") &&
        containerParent instanceof Ci.nsIDOMXULPopupElement) {
      // allow the link traversal to finish before closing the panel
      setTimeout(() => {
        containerParent.hidePopup();
      }, 0);
    }
  },

  get _chromeless() {
    // Is this a popup window that doesn't want chrome shown?
    let docElem = document.documentElement;
    // extrachrome is not restored during session restore, so we need
    // to check for the toolbar as well.
    let chromeless = docElem.getAttribute("chromehidden").includes("extrachrome") ||
                     docElem.getAttribute("chromehidden").includes("toolbar");
    // This property is "fixed" for a window, so avoid doing the check above
    // multiple times...
    delete this._chromeless;
    this._chromeless = chromeless;
    return chromeless;
  },

  get enabled() {
    // Returns whether social is enabled *for this window*.
    if (this._chromeless)
      return false;
    return Social.providers.length > 0;
  },

  canSharePage(aURI) {
    return (aURI && (aURI.schemeIs("http") || aURI.schemeIs("https")));
  },

  onCustomizeEnd(aWindow) {
    if (aWindow != window)
      return;
    // customization mode gets buttons out of sync with command updating, fix
    // the disabled state
    let canShare = this.canSharePage(gBrowser.currentURI);
    let shareButton = SocialShare.shareButton;
    if (shareButton) {
      if (canShare) {
        shareButton.removeAttribute("disabled")
      } else {
        shareButton.setAttribute("disabled", "true")
      }
    }
  },

  // called on tab/urlbar/location changes and after customization. Update
  // anything that is tab specific.
  updateState() {
    goSetCommandEnabled("Social:PageShareable", this.canSharePage(gBrowser.currentURI));
  }
}

// message manager handlers
SocialActivationListener = {
  init() {
    messageManager.addMessageListener("Social:Activation", this);
  },
  uninit() {
    messageManager.removeMessageListener("Social:Activation", this);
  },
  receiveMessage(aMessage) {
    let data = aMessage.json;
    let browser = aMessage.target;
    data.window = window;
    // if the source if the message is the share panel, we do a one-click
    // installation. The source of activations is controlled by the
    // social.directories preference
    let options;
    if (browser == SocialShare.iframe && Services.prefs.getBoolPref("social.share.activationPanelEnabled")) {
      options = { bypassContentCheck: true, bypassInstallPanel: true };
    }

    Social.installProvider(data, function(manifest) {
      Social.activateFromOrigin(manifest.origin, function(provider) {
        if (provider.shareURL) {
          // Ensure that the share button is somewhere usable.
          // SocialShare.shareButton may return null if it is in the menu-panel
          // and has never been visible, so we check the widget directly. If
          // there is no area for the widget we move it into the toolbar.
          let widget = CustomizableUI.getWidget("social-share-button");
          // If the panel is already open, we can be sure that the provider can
          // already be accessed, possibly anchored to another toolbar button.
          // In that case we don't move the widget.
          if (!widget.areaType && SocialShare.panel.state != "open") {
            CustomizableUI.addWidgetToArea("social-share-button", CustomizableUI.AREA_NAVBAR);
            // Ensure correct state.
            SocialUI.onCustomizeEnd(window);
          }

          // make this new provider the selected provider. If the panel hasn't
          // been opened, we need to make the frame first.
          SocialShare._createFrame();
          SocialShare.iframe.setAttribute("src", "data:text/plain;charset=utf8,");
          SocialShare.iframe.setAttribute("origin", provider.origin);
          // get the right button selected
          SocialShare.populateProviderMenu();
          if (SocialShare.panel.state == "open") {
            SocialShare.sharePage(provider.origin);
          }
        }
        if (provider.postActivationURL) {
          // if activated from an open share panel, we load the landing page in
          // a background tab
          let triggeringPrincipal = Utils.deserializePrincipal(aMessage.data.triggeringPrincipal);
          gBrowser.loadOneTab(provider.postActivationURL, {
            inBackground: SocialShare.panel.state == "open",
            triggeringPrincipal,
          });
        }
      });
    }, options);
  }
}

SocialShare = {
  get _dynamicResizer() {
    delete this._dynamicResizer;
    this._dynamicResizer = new DynamicResizeWatcher();
    return this._dynamicResizer;
  },

  // Share panel may be attached to the overflow or menu button depending on
  // customization, we need to manage open state of the anchor.
  get anchor() {
    let widget = CustomizableUI.getWidget("social-share-button");
    return widget.forWindow(window).anchor;
  },
  // Holds the anchor node in use whilst the panel is open, because it may vary.
  _currentAnchor: null,

  get panel() {
    return document.getElementById("social-share-panel");
  },

  get iframe() {
    // panel.firstChild is our toolbar hbox, panel.lastChild is the iframe
    // container hbox used for an interstitial "loading" graphic
    return this.panel.lastChild.firstChild;
  },

  uninit() {
    if (this.iframe) {
      let mm = this.messageManager;
      mm.removeMessageListener("PageVisibility:Show", this);
      mm.removeMessageListener("PageVisibility:Hide", this);
      mm.removeMessageListener("Social:DOMWindowClose", this);
      this.iframe.removeEventListener("load", this);
      this.iframe.remove();
    }
  },

  _createFrame() {
    let panel = this.panel;
    if (this.iframe)
      return;
    this.panel.hidden = false;
    // create and initialize the panel for this window
    let iframe = document.createElement("browser");
    iframe.setAttribute("type", "content");
    iframe.setAttribute("class", "social-share-frame");
    iframe.setAttribute("context", "contentAreaContextMenu");
    iframe.setAttribute("tooltip", "aHTMLTooltip");
    iframe.setAttribute("disableglobalhistory", "true");
    iframe.setAttribute("flex", "1");
    iframe.setAttribute("message", "true");
    iframe.setAttribute("messagemanagergroup", "social");
    panel.lastChild.appendChild(iframe);
    let mm = this.messageManager;
    mm.addMessageListener("PageVisibility:Show", this);
    mm.addMessageListener("PageVisibility:Hide", this);
    mm.sendAsyncMessage("Social:SetErrorURL",
                        { template: "about:socialerror?mode=compactInfo&origin=%{origin}&url=%{url}" });
    iframe.addEventListener("load", this, true);
    mm.addMessageListener("Social:DOMWindowClose", this);

    this.populateProviderMenu();
  },

  get messageManager() {
    // The xbl bindings for the iframe may not exist yet, so we can't
    // access iframe.messageManager directly - but can get at it with this dance.
    return this.iframe.QueryInterface(Components.interfaces.nsIFrameLoaderOwner).frameLoader.messageManager;
  },

  receiveMessage(aMessage) {
    let iframe = this.iframe;
    switch (aMessage.name) {
      case "PageVisibility:Show":
        SocialShare._dynamicResizer.start(iframe.parentNode, iframe);
        break;
      case "PageVisibility:Hide":
        SocialShare._dynamicResizer.stop();
        break;
      case "Social:DOMWindowClose":
        this.panel.hidePopup();
        break;
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "load": {
        this.iframe.parentNode.removeAttribute("loading");
        if (this.currentShare)
          SocialShare.messageManager.sendAsyncMessage("Social:OpenGraphData", this.currentShare);
      }
    }
  },

  getSelectedProvider() {
    let provider;
    let lastProviderOrigin = this.iframe && this.iframe.getAttribute("origin");
    if (lastProviderOrigin) {
      provider = Social._getProviderFromOrigin(lastProviderOrigin);
    }
    return provider;
  },

  createTooltip(event) {
    let tt = event.target;
    let provider = Social._getProviderFromOrigin(tt.triggerNode.getAttribute("origin"));
    tt.firstChild.setAttribute("value", provider.name);
    tt.lastChild.setAttribute("value", provider.origin);
  },

  populateProviderMenu() {
    if (!this.iframe)
      return;
    let providers = Social.providers.filter(p => p.shareURL);
    let hbox = document.getElementById("social-share-provider-buttons");
    // remove everything before the add-share-provider button (which should also
    // be lastChild if any share providers were added)
    let addButton = document.getElementById("add-share-provider");
    while (hbox.lastChild != addButton) {
      hbox.removeChild(hbox.lastChild);
    }
    let selectedProvider = this.getSelectedProvider();
    for (let provider of providers) {
      let button = document.createElement("toolbarbutton");
      button.setAttribute("class", "toolbarbutton-1 share-provider-button");
      button.setAttribute("type", "radio");
      button.setAttribute("group", "share-providers");
      button.setAttribute("image", provider.iconURL);
      button.setAttribute("tooltip", "share-button-tooltip");
      button.setAttribute("origin", provider.origin);
      button.setAttribute("label", provider.name);
      button.setAttribute("oncommand", "SocialShare.sharePage(this.getAttribute('origin'));");
      if (provider == selectedProvider) {
        this.defaultButton = button;
      }
      hbox.appendChild(button);
    }
    if (!this.defaultButton) {
      this.defaultButton = addButton;
    }
    this.defaultButton.setAttribute("checked", "true");
  },

  get shareButton() {
    // web-panels (bookmark/sidebar) don't include customizableui, so
    // nsContextMenu fails when accessing shareButton, breaking
    // browser_bug409481.js.
    if (document.documentElement.getAttribute("windowtype") !== "navigator:browser")
      return null;
    let widget = CustomizableUI.getWidget("social-share-button");
    if (!widget || !widget.areaType)
      return null;
    return widget.forWindow(window).node;
  },

  _onclick() {
    Services.telemetry.getHistogramById("SOCIAL_PANEL_CLICKS").add(0);
  },

  onShowing() {
    (this._currentAnchor || this.anchor).setAttribute("open", "true");
    this.iframe.addEventListener("click", this._onclick, true);
  },

  onHidden() {
    (this._currentAnchor || this.anchor).removeAttribute("open");
    this._currentAnchor = null;
    this.iframe.docShellIsActive = false;
    this.iframe.removeEventListener("click", this._onclick, true);
    this.iframe.setAttribute("src", "data:text/plain;charset=utf8,");
    // make sure that the frame is unloaded after it is hidden
    this.messageManager.sendAsyncMessage("Social:ClearFrame");
    this.currentShare = null;
    // share panel use is over, purge any history
    this.iframe.purgeSessionHistory();
  },

  sharePage(providerOrigin, graphData, target, anchor) {
    // if providerOrigin is undefined, we use the last-used provider, or the
    // current/default provider.  The provider selection in the share panel
    // will call sharePage with an origin for us to switch to.
    this._createFrame();
    let iframe = this.iframe;

    // graphData is an optional param that either defines the full set of data
    // to be shared, or partial data about the current page. It is set by a call
    // in mozSocial API, or via nsContentMenu calls. If it is present, it MUST
    // define at least url. If it is undefined, we're sharing the current url in
    // the browser tab.
    let sharedPageData = graphData || this.currentShare;
    let sharedURI = sharedPageData ? Services.io.newURI(sharedPageData.url) :
                                gBrowser.currentURI;
    if (!SocialUI.canSharePage(sharedURI))
      return;

    let browserMM = gBrowser.selectedBrowser.messageManager;

    // the point of this action type is that we can use existing share
    // endpoints (e.g. oexchange) that do not support additional
    // socialapi functionality.  One tweak is that we shoot an event
    // containing the open graph data.
    let _dataFn;
    if (!sharedPageData || sharedURI == gBrowser.currentURI) {
      browserMM.addMessageListener("PageMetadata:PageDataResult", _dataFn = (msg) => {
        browserMM.removeMessageListener("PageMetadata:PageDataResult", _dataFn);
        let pageData = msg.json;
        if (graphData) {
          // overwrite data retreived from page with data given to us as a param
          for (let p in graphData) {
            pageData[p] = graphData[p];
          }
        }
        this.sharePage(providerOrigin, pageData, target, anchor);
      });
      browserMM.sendAsyncMessage("PageMetadata:GetPageData", null, { target });
      return;
    }
    // if this is a share of a selected item, get any microformats
    if (!sharedPageData.microformats && target) {
      browserMM.addMessageListener("PageMetadata:MicroformatsResult", _dataFn = (msg) => {
        browserMM.removeMessageListener("PageMetadata:MicroformatsResult", _dataFn);
        sharedPageData.microformats = msg.data;
        this.sharePage(providerOrigin, sharedPageData, target, anchor);
      });
      browserMM.sendAsyncMessage("PageMetadata:GetMicroformats", null, { target });
      return;
    }
    this.currentShare = sharedPageData;

    let provider;
    if (providerOrigin)
      provider = Social._getProviderFromOrigin(providerOrigin);
    else
      provider = this.getSelectedProvider();
    if (!provider || !provider.shareURL) {
      this.showDirectory(anchor);
      return;
    }
    // check the menu button
    let hbox = document.getElementById("social-share-provider-buttons");
    let btn = hbox.querySelector("[origin='" + provider.origin + "']");
    if (btn)
      btn.checked = true;

    let shareEndpoint = OpenGraphBuilder.generateEndpointURL(provider.shareURL, sharedPageData);

    this._dynamicResizer.stop();
    let size = provider.getPageSize("share");
    if (size) {
      // let the css on the share panel define width, but height
      // calculations dont work on all sites, so we allow that to be
      // defined.
      delete size.width;
    }

    // if we've already loaded this provider/page share endpoint, we don't want
    // to add another load event listener.
    let endpointMatch = shareEndpoint == iframe.getAttribute("src");
    if (endpointMatch) {
      this._dynamicResizer.start(iframe.parentNode, iframe, size);
      iframe.docShellIsActive = true;
      SocialShare.messageManager.sendAsyncMessage("Social:OpenGraphData", this.currentShare);
    } else {
      iframe.parentNode.setAttribute("loading", "true");
    }
    // if the user switched between share providers we do not want that history
    // available.
    iframe.purgeSessionHistory();

    // always ensure that origin belongs to the endpoint
    iframe.setAttribute("origin", provider.origin);
    iframe.setAttribute("src", shareEndpoint);
    this._openPanel(anchor);
  },

  showDirectory(anchor) {
    this._createFrame();
    let iframe = this.iframe;
    if (iframe.getAttribute("src") == "about:providerdirectory")
      return;
    iframe.removeAttribute("origin");
    iframe.parentNode.setAttribute("loading", "true");

    iframe.setAttribute("src", "about:providerdirectory");
    this._openPanel(anchor);
  },

  _openPanel(anchor) {
    this._currentAnchor = anchor || this.anchor;
    anchor = document.getAnonymousElementByAttribute(this._currentAnchor, "class", "toolbarbutton-icon");
    this.panel.openPopup(anchor, "bottomcenter topright", 0, 0, false, false);
    Services.telemetry.getHistogramById("SOCIAL_TOOLBAR_BUTTONS").add(0);
  }
};

}).call(this);
