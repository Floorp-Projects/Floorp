/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// the "exported" symbols
var SocialUI,
    SocialFlyout,
    SocialShare,
    SocialSidebar,
    SocialActivationListener;

(function() {

XPCOMUtils.defineLazyModuleGetter(this, "PanelFrame",
  "resource:///modules/PanelFrame.jsm");

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

XPCOMUtils.defineLazyGetter(this, "sizeSocialPanelToContent", function() {
  let tmp = {};
  Cu.import("resource:///modules/Social.jsm", tmp);
  return tmp.sizeSocialPanelToContent;
});

XPCOMUtils.defineLazyGetter(this, "hookWindowCloseForPanelClose", function() {
  let tmp = {};
  Cu.import("resource://gre/modules/MozSocialAPI.jsm", tmp);
  return tmp.hookWindowCloseForPanelClose;
});

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

    Services.obs.addObserver(this, "social:ambient-notification-changed", false);
    Services.obs.addObserver(this, "social:providers-changed", false);
    Services.obs.addObserver(this, "social:provider-reload", false);
    Services.obs.addObserver(this, "social:provider-enabled", false);
    Services.obs.addObserver(this, "social:provider-disabled", false);

    Services.prefs.addObserver("social.toast-notifications.enabled", this, false);

    CustomizableUI.addListener(this);
    SocialActivationListener.init();
    messageManager.addMessageListener("Social:Notification", this);

    // menupopups that list social providers. we only populate them when shown,
    // and if it has not been done already.
    document.getElementById("viewSidebarMenu").addEventListener("popupshowing", SocialSidebar.populateSidebarMenu, true);
    document.getElementById("social-statusarea-popup").addEventListener("popupshowing", SocialSidebar.populateSidebarMenu, true);

    Social.init().then((update) => {
      if (update)
        this._providersChanged();
      // handle SessionStore for the sidebar state
      SocialSidebar.restoreWindowState();
    });

    this._initialized = true;
  },

  // Called on window unload
  uninit: function SocialUI_uninit() {
    if (!this._initialized) {
      return;
    }
    SocialSidebar.saveWindowState();

    Services.obs.removeObserver(this, "social:ambient-notification-changed");
    Services.obs.removeObserver(this, "social:providers-changed");
    Services.obs.removeObserver(this, "social:provider-reload");
    Services.obs.removeObserver(this, "social:provider-enabled");
    Services.obs.removeObserver(this, "social:provider-disabled");

    Services.prefs.removeObserver("social.toast-notifications.enabled", this);
    CustomizableUI.removeListener(this);
    SocialActivationListener.uninit();
    messageManager.removeMessageListener("Social:Notification", this);

    document.getElementById("viewSidebarMenu").removeEventListener("popupshowing", SocialSidebar.populateSidebarMenu, true);
    document.getElementById("social-statusarea-popup").removeEventListener("popupshowing", SocialSidebar.populateSidebarMenu, true);

    this._initialized = false;
  },

  receiveMessage: function(aMessage) {
    if (aMessage.name == "Social:Notification") {
      let provider = Social._getProviderFromOrigin(aMessage.data.origin);
      if (provider) {
        provider.setAmbientNotification(aMessage.data.detail);
      }
    }
  },

  observe: function SocialUI_observe(subject, topic, data) {
    switch (topic) {
      case "social:provider-enabled":
        break;
      case "social:provider-disabled":
        SocialSidebar.disableProvider(data);
        break;
      case "social:provider-reload":
        // if the reloaded provider is our current provider, fall through
        // to social:providers-changed so the ui will be reset
        if (!SocialSidebar.provider || SocialSidebar.provider.origin != data)
          return;
        // currently only the sidebar and flyout have a selected provider.
        // sidebar provider has changed (possibly to null), ensure the content
        // is unloaded and the frames are reset, they will be loaded in
        // providers-changed below if necessary.
        SocialSidebar.unloadSidebar();
        SocialFlyout.unload();
        // fall through to providers-changed to ensure the reloaded provider
        // is correctly reflected in any UI and the multi-provider menu
      case "social:providers-changed":
        this._providersChanged();
        break;
      // Provider-specific notifications
      case "social:ambient-notification-changed":
        break;
      case "nsPref:changed":
        if (data == "social.toast-notifications.enabled") {
          SocialSidebar.updateToggleNotifications();
        }
        break;
    }
  },

  _providersChanged: function() {
    SocialSidebar.clearProviderMenus();
    SocialSidebar.update();
    SocialShare.populateProviderMenu();
  },

  showLearnMore: function() {
    let url = Services.urlFormatter.formatURLPref("app.support.baseURL") + "social-api";
    openUILinkIn(url, "tab");
  },

  closeSocialPanelForLinkTraversal: function (target, linkNode) {
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
                     docElem.getAttribute('chromehidden').includes("toolbar");
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

  canSharePage: function(aURI) {
    return (aURI && (aURI.schemeIs('http') || aURI.schemeIs('https')));
  },

  onCustomizeEnd: function(aWindow) {
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
  updateState: function() {
    goSetCommandEnabled("Social:PageShareable", this.canSharePage(gBrowser.currentURI));
  }
}

// message manager handlers
SocialActivationListener = {
  init: function() {
    messageManager.addMessageListener("Social:Activation", this);
  },
  uninit: function() {
    messageManager.removeMessageListener("Social:Activation", this);
  },
  receiveMessage: function(aMessage) {
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
        if (provider.sidebarURL) {
          SocialSidebar.show(provider.origin);
        }
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
          SocialShare.iframe.setAttribute('src', 'data:text/plain;charset=utf8,');
          SocialShare.iframe.setAttribute('origin', provider.origin);
          // get the right button selected
          SocialShare.populateProviderMenu();
          if (SocialShare.panel.state == "open") {
            SocialShare.sharePage(provider.origin);
          }
        }
        if (provider.postActivationURL) {
          // if activated from an open share panel, we load the landing page in
          // a background tab
          gBrowser.loadOneTab(provider.postActivationURL, {inBackground: SocialShare.panel.state == "open"});
        }
      });
    }, options);
  }
}

SocialFlyout = {
  get panel() {
    return document.getElementById("social-flyout-panel");
  },

  get iframe() {
    if (!this.panel.firstChild)
      this._createFrame();
    return this.panel.firstChild;
  },

  dispatchPanelEvent: function(name) {
    let doc = this.iframe.contentDocument;
    let evt = doc.createEvent("CustomEvent");
    evt.initCustomEvent(name, true, true, {});
    doc.documentElement.dispatchEvent(evt);
  },

  _createFrame: function() {
    let panel = this.panel;
    if (!SocialUI.enabled || panel.firstChild)
      return;
    // create and initialize the panel for this window
    let iframe = document.createElement("browser");
    iframe.setAttribute("type", "content");
    iframe.setAttribute("class", "social-panel-frame");
    iframe.setAttribute("flex", "1");
    iframe.setAttribute("message", "true");
    iframe.setAttribute("messagemanagergroup", "social");
    iframe.setAttribute("disablehistory", "true");
    iframe.setAttribute("tooltip", "aHTMLTooltip");
    iframe.setAttribute("context", "contentAreaContextMenu");
    iframe.setAttribute("origin", SocialSidebar.provider.origin);
    panel.appendChild(iframe);
    this.messageManager.sendAsyncMessage("Social:SetErrorURL",
                        { template: "about:socialerror?mode=compactInfo&origin=%{origin}" });
  },

  get messageManager() {
    // The xbl bindings for the iframe may not exist yet, so we can't
    // access iframe.messageManager directly - but can get at it with this dance.
    return this.iframe.QueryInterface(Components.interfaces.nsIFrameLoaderOwner).frameLoader.messageManager;
  },

  unload: function() {
    let panel = this.panel;
    panel.hidePopup();
    if (!panel.firstChild)
      return
    let iframe = panel.firstChild;
    panel.removeChild(iframe);
  },

  onShown: function(aEvent) {
    let panel = this.panel;
    let iframe = this.iframe;
    this._dynamicResizer = new DynamicResizeWatcher();
    iframe.docShellIsActive = true;
    if (iframe.contentDocument.readyState == "complete") {
      this._dynamicResizer.start(panel, iframe);
    } else {
      // first time load, wait for load and dispatch after load
      let mm = this.messageManager;
      mm.addMessageListener("DOMContentLoaded", function panelBrowserOnload(e) {
        mm.removeMessageListener("DOMContentLoaded", panelBrowserOnload);
        setTimeout(function() {
          if (SocialFlyout._dynamicResizer) { // may go null if hidden quickly
            SocialFlyout._dynamicResizer.start(panel, iframe);
          }
        }, 0);
      });
    }
  },

  onHidden: function(aEvent) {
    this._dynamicResizer.stop();
    this._dynamicResizer = null;
    this.iframe.docShellIsActive = false;
  },

  load: function(aURL, cb) {
    if (!SocialSidebar.provider)
      return;

    this.panel.hidden = false;
    let iframe = this.iframe;
    // same url with only ref difference does not cause a new load, so we
    // want to go right to the callback
    let src = iframe.contentDocument && iframe.contentDocument.documentURIObject;
    if (!src || !src.equalsExceptRef(Services.io.newURI(aURL, null, null))) {
      let mm = this.messageManager;
      mm.addMessageListener("DOMContentLoaded", function documentLoaded(e) {
        mm.removeMessageListener("DOMContentLoaded", documentLoaded);
        cb();
      });
      iframe.setAttribute("src", aURL);
    } else {
      // we still need to set the src to trigger the contents hashchange event
      // for ref changes
      iframe.setAttribute("src", aURL);
      cb();
    }
  },

  open: function(aURL, yOffset, aCallback) {
    // Hide any other social panels that may be open.
    document.getElementById("social-notification-panel").hidePopup();

    if (!SocialUI.enabled)
      return;
    let panel = this.panel;
    let iframe = this.iframe;

    this.load(aURL, function() {
      sizeSocialPanelToContent(panel, iframe);
      let anchor = document.getElementById("social-sidebar-browser");
      if (panel.state == "open") {
        panel.moveToAnchor(anchor, "start_before", 0, yOffset, false);
      } else {
        panel.openPopup(anchor, "start_before", 0, yOffset, false, false);
      }
      if (aCallback) {
        try {
          aCallback(iframe.contentWindow);
        } catch(e) {
          Cu.reportError(e);
        }
      }
    });
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

  uninit: function () {
    if (this.iframe) {
      let mm = this.messageManager;
      mm.removeMessageListener("PageVisibility:Show", this);
      mm.removeMessageListener("PageVisibility:Hide", this);
      this.iframe.removeEventListener("load", this);
      this.iframe.remove();
    }
  },

  _createFrame: function() {
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

    this.populateProviderMenu();
  },

  get messageManager() {
    // The xbl bindings for the iframe may not exist yet, so we can't
    // access iframe.messageManager directly - but can get at it with this dance.
    return this.iframe.QueryInterface(Components.interfaces.nsIFrameLoaderOwner).frameLoader.messageManager;
  },

  receiveMessage: function(aMessage) {
    let iframe = this.iframe;
    switch(aMessage.name) {
      case "PageVisibility:Show":
        SocialShare._dynamicResizer.start(iframe.parentNode, iframe);
        break;
      case "PageVisibility:Hide":
        SocialShare._dynamicResizer.stop();
        break;
    }
  },

  handleEvent: function(event) {
    switch (event.type) {
      case "load": {
        let iframe = this.iframe;
        iframe.parentNode.removeAttribute("loading");
        // to support standard share endpoints mimick window.open by setting
        // window.opener, some share endpoints rely on w.opener to know they
        // should close the window when done.
        iframe.contentWindow.opener = iframe.contentWindow;
        this.messageManager.sendAsyncMessage("Social:HookWindowCloseForPanelClose");
        this.messageManager.sendAsyncMessage("Social:DisableDialogs", {});
        if (this.currentShare)
          SocialShare.messageManager.sendAsyncMessage("Social:OpenGraphData", this.currentShare);
      }
    }
  },

  getSelectedProvider: function() {
    let provider;
    let lastProviderOrigin = this.iframe && this.iframe.getAttribute("origin");
    if (lastProviderOrigin) {
      provider = Social._getProviderFromOrigin(lastProviderOrigin);
    }
    return provider;
  },

  createTooltip: function(event) {
    let tt = event.target;
    let provider = Social._getProviderFromOrigin(tt.triggerNode.getAttribute("origin"));
    tt.firstChild.setAttribute("value", provider.name);
    tt.lastChild.setAttribute("value", provider.origin);
  },

  populateProviderMenu: function() {
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
    if (!window.CustomizableUI)
      return null;
    let widget = CustomizableUI.getWidget("social-share-button");
    if (!widget || !widget.areaType)
      return null;
    return widget.forWindow(window).node;
  },

  _onclick: function() {
    Services.telemetry.getHistogramById("SOCIAL_PANEL_CLICKS").add(0);
  },

  onShowing: function() {
    (this._currentAnchor || this.anchor).setAttribute("open", "true");
    this.iframe.addEventListener("click", this._onclick, true);
  },

  onHidden: function() {
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

  sharePage: function(providerOrigin, graphData, target, anchor) {
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
    let pageData = graphData ? graphData : this.currentShare;
    let sharedURI = pageData ? Services.io.newURI(pageData.url, null, null) :
                                gBrowser.currentURI;
    if (!SocialUI.canSharePage(sharedURI))
      return;

    // the point of this action type is that we can use existing share
    // endpoints (e.g. oexchange) that do not support additional
    // socialapi functionality.  One tweak is that we shoot an event
    // containing the open graph data.
    let _dataFn;
    if (!pageData || sharedURI == gBrowser.currentURI) {
      messageManager.addMessageListener("PageMetadata:PageDataResult", _dataFn = (msg) => {
        messageManager.removeMessageListener("PageMetadata:PageDataResult", _dataFn);
        let pageData = msg.json;
        if (graphData) {
          // overwrite data retreived from page with data given to us as a param
          for (let p in graphData) {
            pageData[p] = graphData[p];
          }
        }
        this.sharePage(providerOrigin, pageData, target, anchor);
      });
      gBrowser.selectedBrowser.messageManager.sendAsyncMessage("PageMetadata:GetPageData", null, { target });
      return;
    }
    // if this is a share of a selected item, get any microformats
    if (!pageData.microformats && target) {
      messageManager.addMessageListener("PageMetadata:MicroformatsResult", _dataFn = (msg) => {
        messageManager.removeMessageListener("PageMetadata:MicroformatsResult", _dataFn);
        pageData.microformats = msg.data;
        this.sharePage(providerOrigin, pageData, target, anchor);
      });
      gBrowser.selectedBrowser.messageManager.sendAsyncMessage("PageMetadata:GetMicroformats", null, { target });
      return;
    }
    this.currentShare = pageData;

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

    let shareEndpoint = OpenGraphBuilder.generateEndpointURL(provider.shareURL, pageData);

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
    let uri = Services.io.newURI(shareEndpoint, null, null);
    iframe.setAttribute("origin", provider.origin);
    iframe.setAttribute("src", shareEndpoint);
    this._openPanel(anchor);
  },

  showDirectory: function(anchor) {
    this._createFrame();
    let iframe = this.iframe;
    if (iframe.getAttribute("src") == "about:providerdirectory")
      return;
    iframe.removeAttribute("origin");
    iframe.parentNode.setAttribute("loading", "true");

    iframe.setAttribute("src", "about:providerdirectory");
    this._openPanel(anchor);
  },

  _openPanel: function(anchor) {
    this._currentAnchor = anchor || this.anchor;
    anchor = document.getAnonymousElementByAttribute(this._currentAnchor, "class", "toolbarbutton-icon");
    this.panel.openPopup(anchor, "bottomcenter topright", 0, 0, false, false);
    Services.telemetry.getHistogramById("SOCIAL_TOOLBAR_BUTTONS").add(0);
  }
};

SocialSidebar = {
  _openStartTime: 0,

  get browser() {
    return document.getElementById("social-sidebar-browser");
  },

  // Whether the sidebar can be shown for this window.
  get canShow() {
    if (!SocialUI.enabled || document.fullscreenElement)
      return false;
    return Social.providers.some(p => p.sidebarURL);
  },

  // Whether the user has toggled the sidebar on (for windows where it can appear)
  get opened() {
    let broadcaster = document.getElementById("socialSidebarBroadcaster");
    return !broadcaster.hidden;
  },

  restoreWindowState: function() {
    // Window state is used to allow different sidebar providers in each window.
    // We also store the provider used in a pref as the default sidebar to
    // maintain that state for users who do not restore window state. The
    // existence of social.sidebar.provider means the sidebar is open with that
    // provider.
    this._initialized = true;
    if (!this.canShow)
      return;

    if (Services.prefs.prefHasUserValue("social.provider.current")) {
      // "upgrade" when the first window opens if we have old prefs.  We get the
      // values from prefs this one time, window state will be saved when this
      // window is closed.
      let origin = Services.prefs.getCharPref("social.provider.current");
      Services.prefs.clearUserPref("social.provider.current");
      // social.sidebar.open default was true, but we only opened if there was
      // a current provider
      let opened = origin && true;
      if (Services.prefs.prefHasUserValue("social.sidebar.open")) {
        opened = origin && Services.prefs.getBoolPref("social.sidebar.open");
        Services.prefs.clearUserPref("social.sidebar.open");
      }
      let data = {
        "hidden": !opened,
        "origin": origin
      };
      SessionStore.setWindowValue(window, "socialSidebar", JSON.stringify(data));
    }

    let data = SessionStore.getWindowValue(window, "socialSidebar");
    // if this window doesn't have it's own state, use the state from the opener
    if (!data && window.opener && !window.opener.closed) {
      try {
        data = SessionStore.getWindowValue(window.opener, "socialSidebar");
      } catch(e) {
        // Window is not tracked, which happens on osx if the window is opened
        // from the hidden window. That happens when you close the last window
        // without quiting firefox, then open a new window.
      }
    }
    if (data) {
      data = JSON.parse(data);
      this.browser.setAttribute("origin", data.origin);
      if (!data.hidden)
        this.show(data.origin);
    } else if (Services.prefs.prefHasUserValue("social.sidebar.provider")) {
      // no window state, use the global state if it is available
      this.show(Services.prefs.getCharPref("social.sidebar.provider"));
    }
  },

  saveWindowState: function() {
    let broadcaster = document.getElementById("socialSidebarBroadcaster");
    let sidebarOrigin = this.browser.getAttribute("origin");
    let data = {
      "hidden": broadcaster.hidden,
      "origin": sidebarOrigin
    };
    if (broadcaster.hidden) {
      Services.telemetry.getHistogramById("SOCIAL_SIDEBAR_OPEN_DURATION").add(Date.now()  / 1000 - this._openStartTime);
    } else {
      this._openStartTime = Date.now() / 1000;
    }

    // Save a global state for users who do not restore state.
    if (broadcaster.hidden)
      Services.prefs.clearUserPref("social.sidebar.provider");
    else
      Services.prefs.setCharPref("social.sidebar.provider", sidebarOrigin);

    try {
      SessionStore.setWindowValue(window, "socialSidebar", JSON.stringify(data));
    } catch(e) {
      // window not tracked during uninit
    }
  },

  setSidebarVisibilityState: function(aEnabled) {
    let sbrowser = document.getElementById("social-sidebar-browser");
    // it's possible we'll be called twice with aEnabled=false so let's
    // just assume we may often be called with the same state.
    if (aEnabled == sbrowser.docShellIsActive)
      return;
    sbrowser.docShellIsActive = aEnabled;
  },

  updateToggleNotifications: function() {
    let command = document.getElementById("Social:ToggleNotifications");
    command.setAttribute("checked", Services.prefs.getBoolPref("social.toast-notifications.enabled"));
    command.setAttribute("hidden", !SocialUI.enabled);
  },

  update: function SocialSidebar_update() {
    // ensure we never update before restoreWindowState
    if (!this._initialized)
      return;
    this.ensureProvider();
    this.updateToggleNotifications();
    this._updateHeader();
    clearTimeout(this._unloadTimeoutId);
    // Hide the toggle menu item if the sidebar cannot appear
    let command = document.getElementById("Social:ToggleSidebar");
    command.setAttribute("hidden", this.canShow ? "false" : "true");

    // Hide the sidebar if it cannot appear, or has been toggled off.
    // Also set the command "checked" state accordingly.
    let hideSidebar = !this.canShow || !this.opened;
    let broadcaster = document.getElementById("socialSidebarBroadcaster");
    broadcaster.hidden = hideSidebar;
    command.setAttribute("checked", !hideSidebar);

    let sbrowser = this.browser;

    if (hideSidebar) {
      sbrowser.messageManager.removeMessageListener("DOMContentLoaded", SocialSidebar._loadListener);
      this.setSidebarVisibilityState(false);
      // If we've been disabled, unload the sidebar content immediately;
      // if the sidebar was just toggled to invisible, wait a timeout
      // before unloading.
      if (!this.canShow) {
        this.unloadSidebar();
      } else {
        this._unloadTimeoutId = setTimeout(
          this.unloadSidebar,
          Services.prefs.getIntPref("social.sidebar.unload_timeout_ms")
        );
      }
    } else {
      sbrowser.setAttribute("origin", this.provider.origin);

      // Make sure the right sidebar URL is loaded
      if (sbrowser.getAttribute("src") != this.provider.sidebarURL) {
        sbrowser.setAttribute("src", this.provider.sidebarURL);
        PopupNotifications.locationChange(sbrowser);
        document.getElementById("social-sidebar-button").setAttribute("loading", "true");
        sbrowser.messageManager.addMessageListener("DOMContentLoaded", SocialSidebar._loadListener);
      } else {
        // if the document has not loaded, delay until it is
        if (sbrowser.contentDocument.readyState != "complete") {
          document.getElementById("social-sidebar-button").setAttribute("loading", "true");
          sbrowser.messageManager.addMessageListener("DOMContentLoaded", SocialSidebar._loadListener);
        } else {
          this.setSidebarVisibilityState(true);
        }
      }
    }
    this._updateCheckedMenuItems(this.opened && this.provider ? this.provider.origin : null);
  },

  _onclick: function() {
    Services.telemetry.getHistogramById("SOCIAL_PANEL_CLICKS").add(3);
  },

  _loadListener: function SocialSidebar_loadListener() {
    let sbrowser = SocialSidebar.browser;
    sbrowser.messageManager.removeMessageListener("DOMContentLoaded", SocialSidebar._loadListener);
    document.getElementById("social-sidebar-button").removeAttribute("loading");
    SocialSidebar.setSidebarVisibilityState(true);
    sbrowser.addEventListener("click", SocialSidebar._onclick, true);
  },

  unloadSidebar: function SocialSidebar_unloadSidebar() {
    let sbrowser = SocialSidebar.browser;
    if (!sbrowser.hasAttribute("origin"))
      return;

    sbrowser.removeEventListener("click", SocialSidebar._onclick, true);
    sbrowser.stop();
    sbrowser.removeAttribute("origin");
    sbrowser.setAttribute("src", "about:blank");
    // We need to explicitly create a new content viewer because the old one
    // doesn't get destroyed until about:blank has loaded (which does not happen
    // as long as the element is hidden).
    sbrowser.messageManager.sendAsyncMessage("Social:ClearFrame");
    SocialFlyout.unload();
  },

  _unloadTimeoutId: 0,

  _provider: null,
  ensureProvider: function() {
    if (this._provider)
      return;
    // origin for sidebar is persisted, so get the previously selected sidebar
    // first, otherwise fallback to the first provider in the list
    let origin = this.browser.getAttribute("origin");
    let providers = Social.providers.filter(p => p.sidebarURL);
    let provider;
    if (origin)
      provider = Social._getProviderFromOrigin(origin);
    if (!provider && providers.length > 0)
      provider = providers[0];
    if (provider)
      this.provider = provider;
  },

  get provider() {
    return this._provider;
  },

  set provider(provider) {
    if (!provider || provider.sidebarURL) {
      this._provider = provider;
      this._updateHeader();
      this._updateCheckedMenuItems(provider && provider.origin);
      this.update();
    }
  },

  disableProvider: function(origin) {
    if (this._provider && this._provider.origin == origin) {
      this._provider = null;
      // force a selection of the next provider if there is one
      this.ensureProvider();
    }
  },

  _updateHeader: function() {
    let provider = this.provider;
    let image, title;
    if (provider) {
      image = "url(" + (provider.icon32URL || provider.iconURL) + ")";
      title = provider.name;
    }
    document.getElementById("social-sidebar-favico").style.listStyleImage = image;
    document.getElementById("social-sidebar-title").value = title;
  },

  _updateCheckedMenuItems: function(origin) {
    // update selected menuitems
    let menuitems = document.getElementsByClassName("social-provider-menuitem");
    for (let mi of menuitems) {
      if (origin && mi.getAttribute("origin") == origin) {
        mi.setAttribute("checked", "true");
        mi.setAttribute("oncommand", "SocialSidebar.hide();");
      } else if (mi.getAttribute("checked")) {
        mi.removeAttribute("checked");
        mi.setAttribute("oncommand", "SocialSidebar.show(this.getAttribute('origin'));");
      }
    }
  },

  show: function(origin) {
    // always show the sidebar, and set the provider
    let broadcaster = document.getElementById("socialSidebarBroadcaster");
    broadcaster.hidden = false;
    if (origin)
      this.provider = Social._getProviderFromOrigin(origin);
    else
      SocialSidebar.update();
    this.saveWindowState();
    Services.telemetry.getHistogramById("SOCIAL_SIDEBAR_STATE").add(true);
  },

  hide: function() {
    let broadcaster = document.getElementById("socialSidebarBroadcaster");
    broadcaster.hidden = true;
    this._updateCheckedMenuItems();
    this.clearProviderMenus();
    SocialSidebar.update();
    this.saveWindowState();
    Services.telemetry.getHistogramById("SOCIAL_SIDEBAR_STATE").add(false);
  },

  toggleSidebar: function SocialSidebar_toggle() {
    let broadcaster = document.getElementById("socialSidebarBroadcaster");
    if (broadcaster.hidden)
      this.show();
    else
      this.hide();
  },

  populateSidebarMenu: function(event) {
    // Providers are removed from the view->sidebar menu when there is a change
    // in providers, so we only have to populate onshowing if there are no
    // provider menus. We populate this menu so long as there are enabled
    // providers with sidebars.
    let popup = event.target;
    let providerMenuSeps = popup.getElementsByClassName("social-provider-menu");
    if (providerMenuSeps[0].previousSibling.nodeName == "menuseparator")
      SocialSidebar.populateProviderMenu(providerMenuSeps[0]);
  },

  clearProviderMenus: function() {
    // called when there is a change in the provider list we clear all menus,
    // they will be repopulated when the menu is shown
    let providerMenuSeps = document.getElementsByClassName("social-provider-menu");
    for (let providerMenuSep of providerMenuSeps) {
      while (providerMenuSep.previousSibling.nodeName == "menuitem") {
        let menu = providerMenuSep.parentNode;
        menu.removeChild(providerMenuSep.previousSibling);
      }
    }
  },

  populateProviderMenu: function(providerMenuSep) {
    let menu = providerMenuSep.parentNode;
    // selectable providers are inserted before the provider-menu seperator,
    // remove any menuitems in that area
    while (providerMenuSep.previousSibling.nodeName == "menuitem") {
      menu.removeChild(providerMenuSep.previousSibling);
    }
    // only show a selection in the sidebar header menu if there is more than one
    let providers = Social.providers.filter(p => p.sidebarURL);
    if (providers.length < 2 && menu.id != "viewSidebarMenu") {
      providerMenuSep.hidden = true;
      return;
    }
    let topSep = providerMenuSep.previousSibling;
    for (let provider of providers) {
      let menuitem = document.createElement("menuitem");
      menuitem.className = "menuitem-iconic social-provider-menuitem";
      menuitem.setAttribute("image", provider.iconURL);
      menuitem.setAttribute("label", provider.name);
      menuitem.setAttribute("origin", provider.origin);
      if (this.opened && provider == this.provider) {
        menuitem.setAttribute("checked", "true");
        menuitem.setAttribute("oncommand", "SocialSidebar.hide();");
      } else {
        menuitem.setAttribute("oncommand", "SocialSidebar.show(this.getAttribute('origin'));");
      }
      menu.insertBefore(menuitem, providerMenuSep);
    }
    topSep.hidden = topSep.nextSibling == providerMenuSep;
    providerMenuSep.hidden = !providerMenuSep.nextSibling;
  }
}

})();
