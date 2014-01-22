// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// the "exported" symbols
let SocialUI,
    SocialChatBar,
    SocialFlyout,
    SocialMarks,
    SocialShare,
    SocialSidebar,
    SocialStatus;

(function() {

// The minimum sizes for the auto-resize panel code.
const PANEL_MIN_HEIGHT = 100;
const PANEL_MIN_WIDTH = 330;

XPCOMUtils.defineLazyModuleGetter(this, "SharedFrame",
  "resource:///modules/SharedFrame.jsm");

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

XPCOMUtils.defineLazyGetter(this, "CreateSocialStatusWidget", function() {
  let tmp = {};
  Cu.import("resource:///modules/Social.jsm", tmp);
  return tmp.CreateSocialStatusWidget;
});

XPCOMUtils.defineLazyGetter(this, "CreateSocialMarkWidget", function() {
  let tmp = {};
  Cu.import("resource:///modules/Social.jsm", tmp);
  return tmp.CreateSocialMarkWidget;
});

SocialUI = {
  // Called on delayed startup to initialize the UI
  init: function SocialUI_init() {
    Services.obs.addObserver(this, "social:ambient-notification-changed", false);
    Services.obs.addObserver(this, "social:profile-changed", false);
    Services.obs.addObserver(this, "social:frameworker-error", false);
    Services.obs.addObserver(this, "social:provider-set", false);
    Services.obs.addObserver(this, "social:providers-changed", false);
    Services.obs.addObserver(this, "social:provider-reload", false);
    Services.obs.addObserver(this, "social:provider-enabled", false);
    Services.obs.addObserver(this, "social:provider-disabled", false);

    Services.prefs.addObserver("social.sidebar.open", this, false);
    Services.prefs.addObserver("social.toast-notifications.enabled", this, false);

    gBrowser.addEventListener("ActivateSocialFeature", this._activationEventHandler.bind(this), true, true);
    document.getElementById("PanelUI-popup").addEventListener("popupshown", SocialMarks.updatePanelButtons, true);

    // menupopups that list social providers. we only populate them when shown,
    // and if it has not been done already.
    document.getElementById("viewSidebarMenu").addEventListener("popupshowing", SocialSidebar.populateSidebarMenu, true);
    document.getElementById("social-statusarea-popup").addEventListener("popupshowing", SocialSidebar.populateSidebarMenu, true);

    if (!Social.initialized) {
      Social.init();
    } else if (Social.providers.length > 0) {
      // Social was initialized during startup in a previous window. If we have
      // providers enabled initialize the UI for this window.
      this.observe(null, "social:providers-changed", null);
      this.observe(null, "social:provider-set", Social.provider ? Social.provider.origin : null);
    }
  },

  // Called on window unload
  uninit: function SocialUI_uninit() {
    Services.obs.removeObserver(this, "social:ambient-notification-changed");
    Services.obs.removeObserver(this, "social:profile-changed");
    Services.obs.removeObserver(this, "social:frameworker-error");
    Services.obs.removeObserver(this, "social:provider-set");
    Services.obs.removeObserver(this, "social:providers-changed");
    Services.obs.removeObserver(this, "social:provider-reload");
    Services.obs.removeObserver(this, "social:provider-enabled");
    Services.obs.removeObserver(this, "social:provider-disabled");

    Services.prefs.removeObserver("social.sidebar.open", this);
    Services.prefs.removeObserver("social.toast-notifications.enabled", this);

    document.getElementById("PanelUI-popup").removeEventListener("popupshown", SocialMarks.updatePanelButtons, true);
    document.getElementById("viewSidebarMenu").removeEventListener("popupshowing", SocialSidebar.populateSidebarMenu, true);
    document.getElementById("social-statusarea-popup").removeEventListener("popupshowing", SocialSidebar.populateSidebarMenu, true);
  },

  _matchesCurrentProvider: function (origin) {
    return Social.provider && Social.provider.origin == origin;
  },

  observe: function SocialUI_observe(subject, topic, data) {
    // Exceptions here sometimes don't get reported properly, report them
    // manually :(
    try {
      switch (topic) {
        case "social:provider-enabled":
          SocialMarks.populateToolbarPalette();
          SocialStatus.populateToolbarPalette();
          break;
        case "social:provider-disabled":
          SocialMarks.removeProvider(data);
          SocialStatus.removeProvider(data);
          break;
        case "social:provider-reload":
          SocialStatus.reloadProvider(data);
          // if the reloaded provider is our current provider, fall through
          // to social:provider-set so the ui will be reset
          if (!Social.provider || Social.provider.origin != data)
            return;
          // be sure to unload the sidebar as it will not reload if the origin
          // has not changed, it will be loaded in provider-set below. Other
          // panels will be unloaded or handle reload.
          SocialSidebar.unloadSidebar();
          // fall through to social:provider-set
        case "social:provider-set":
          // Social.provider has changed (possibly to null), update any state
          // which depends on it.
          this._updateActiveUI();

          SocialFlyout.unload();
          SocialChatBar.update();
          SocialShare.update();
          SocialSidebar.update();
          SocialStatus.populateToolbarPalette();
          SocialMarks.populateToolbarPalette();
          break;
        case "social:providers-changed":
          // the list of providers changed - this may impact the "active" UI.
          this._updateActiveUI();
          // and the multi-provider menu
          SocialSidebar.clearProviderMenus();
          SocialShare.populateProviderMenu();
          SocialStatus.populateToolbarPalette();
          SocialMarks.populateToolbarPalette();
          break;

        // Provider-specific notifications
        case "social:ambient-notification-changed":
          SocialStatus.updateButton(data);
          break;
        case "social:profile-changed":
          // make sure anything that happens here only affects the provider for
          // which the profile is changing, and that anything we call actually
          // needs to change based on profile data.
          SocialStatus.updateButton(data);
          break;
        case "social:frameworker-error":
          if (this.enabled && Social.provider.origin == data) {
            SocialSidebar.setSidebarErrorMessage();
          }
          break;

        case "nsPref:changed":
          if (data == "social.sidebar.open") {
            SocialSidebar.update();
          } else if (data == "social.toast-notifications.enabled") {
            SocialSidebar.updateToggleNotifications();
          }
          break;
      }
    } catch (e) {
      Components.utils.reportError(e + "\n" + e.stack);
      throw e;
    }
  },

  _updateActiveUI: function SocialUI_updateActiveUI() {
    // The "active" UI isn't dependent on there being a provider, just on
    // social being "active" (but also chromeless/PB)
    let enabled = Social.providers.length > 0 && !this._chromeless &&
                  !PrivateBrowsingUtils.isWindowPrivate(window);

    let toggleCommand = document.getElementById("Social:Toggle");
    toggleCommand.setAttribute("hidden", enabled ? "false" : "true");

    if (enabled) {
      // enabled == true means we at least have a defaultProvider
      let provider = Social.provider || Social.defaultProvider;
      // We only need to update the command itself - all our menu items use it.
      let label;
      if (Social.providers.length == 1) {
        label = gNavigatorBundle.getFormattedString(Social.provider
                                                    ? "social.turnOff.label"
                                                    : "social.turnOn.label",
                                                    [provider.name]);
      } else {
        label = gNavigatorBundle.getString(Social.provider
                                           ? "social.turnOffAll.label"
                                           : "social.turnOnAll.label");
      }
      let accesskey = gNavigatorBundle.getString(Social.provider
                                                 ? "social.turnOff.accesskey"
                                                 : "social.turnOn.accesskey");
      toggleCommand.setAttribute("label", label);
      toggleCommand.setAttribute("accesskey", accesskey);
    }
  },

  // This handles "ActivateSocialFeature" events fired against content documents
  // in this window.
  _activationEventHandler: function SocialUI_activationHandler(e) {
    let targetDoc;
    let node;
    if (e.target instanceof HTMLDocument) {
      // version 0 support
      targetDoc = e.target;
      node = targetDoc.documentElement
    } else {
      targetDoc = e.target.ownerDocument;
      node = e.target;
    }
    if (!(targetDoc instanceof HTMLDocument))
      return;

    // Ignore events fired in background tabs or iframes
    if (targetDoc.defaultView != content)
      return;

    // If we are in PB mode, we silently do nothing (bug 829404 exists to
    // do something sensible here...)
    if (PrivateBrowsingUtils.isWindowPrivate(window))
      return;

    // If the last event was received < 1s ago, ignore this one
    let now = Date.now();
    if (now - Social.lastEventReceived < 1000)
      return;
    Social.lastEventReceived = now;

    // We only want to activate if it is as a result of user input.
    let dwu = window.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindowUtils);
    if (!dwu.isHandlingUserInput) {
      Cu.reportError("attempt to activate provider without user input from " + targetDoc.nodePrincipal.origin);
      return;
    }

    let data = node.getAttribute("data-service");
    if (data) {
      try {
        data = JSON.parse(data);
      } catch(e) {
        Cu.reportError("Social Service manifest parse error: "+e);
        return;
      }
    }
    Social.installProvider(targetDoc, data, function(manifest) {
      Social.activateFromOrigin(manifest.origin);
    });
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
    let win = linkNode.ownerDocument.defaultView;
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
    let chromeless = docElem.getAttribute("chromehidden").contains("extrachrome") ||
                     docElem.getAttribute('chromehidden').contains("toolbar");
    // This property is "fixed" for a window, so avoid doing the check above
    // multiple times...
    delete this._chromeless;
    this._chromeless = chromeless;
    return chromeless;
  },

  get enabled() {
    // Returns whether social is enabled *for this window*.
    if (this._chromeless || PrivateBrowsingUtils.isWindowPrivate(window))
      return false;
    return !!Social.provider;
  },

  // called on tab/urlbar/location changes and after customization. Update
  // anything that is tab specific.
  updateState: function() {
    if (!this.enabled)
      return;
    SocialMarks.update();
    SocialShare.update();
  }
}

SocialChatBar = {
  get chatbar() {
    return document.getElementById("pinnedchats");
  },
  // Whether the chatbar is available for this window.  Note that in full-screen
  // mode chats are available, but not shown.
  get isAvailable() {
    return SocialUI.enabled;
  },
  // Does this chatbar have any chats (whether minimized, collapsed or normal)
  get hasChats() {
    return !!this.chatbar.firstElementChild;
  },
  openChat: function(aProvider, aURL, aCallback, aMode) {
    if (!this.isAvailable)
      return false;
    this.chatbar.openChat(aProvider, aURL, aCallback, aMode);
    // We only want to focus the chat if it is as a result of user input.
    let dwu = window.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindowUtils);
    if (dwu.isHandlingUserInput)
      this.chatbar.focus();
    return true;
  },
  update: function() {
    let command = document.getElementById("Social:FocusChat");
    if (!this.isAvailable) {
      this.chatbar.hidden = command.hidden = true;
    } else {
      this.chatbar.hidden = command.hidden = false;
    }
    command.setAttribute("disabled", command.hidden ? "true" : "false");
  },
  focus: function SocialChatBar_focus() {
    this.chatbar.focus();
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
    let iframe = document.createElement("iframe");
    iframe.setAttribute("type", "content");
    iframe.setAttribute("class", "social-panel-frame");
    iframe.setAttribute("flex", "1");
    iframe.setAttribute("tooltip", "aHTMLTooltip");
    iframe.setAttribute("origin", Social.provider.origin);
    panel.appendChild(iframe);
  },

  setFlyoutErrorMessage: function SF_setFlyoutErrorMessage() {
    this.iframe.removeAttribute("src");
    this.iframe.webNavigation.loadURI("about:socialerror?mode=compactInfo", null, null, null, null);
    sizeSocialPanelToContent(this.panel, this.iframe);
  },

  unload: function() {
    let panel = this.panel;
    panel.hidePopup();
    if (!panel.firstChild)
      return
    let iframe = panel.firstChild;
    if (iframe.socialErrorListener)
      iframe.socialErrorListener.remove();
    panel.removeChild(iframe);
  },

  onShown: function(aEvent) {
    let panel = this.panel;
    let iframe = this.iframe;
    this._dynamicResizer = new DynamicResizeWatcher();
    iframe.docShell.isActive = true;
    iframe.docShell.isAppTab = true;
    if (iframe.contentDocument.readyState == "complete") {
      this._dynamicResizer.start(panel, iframe);
      this.dispatchPanelEvent("socialFrameShow");
    } else {
      // first time load, wait for load and dispatch after load
      iframe.addEventListener("load", function panelBrowserOnload(e) {
        iframe.removeEventListener("load", panelBrowserOnload, true);
        setTimeout(function() {
          if (SocialFlyout._dynamicResizer) { // may go null if hidden quickly
            SocialFlyout._dynamicResizer.start(panel, iframe);
            SocialFlyout.dispatchPanelEvent("socialFrameShow");
          }
        }, 0);
      }, true);
    }
  },

  onHidden: function(aEvent) {
    this._dynamicResizer.stop();
    this._dynamicResizer = null;
    this.iframe.docShell.isActive = false;
    this.dispatchPanelEvent("socialFrameHide");
  },

  load: function(aURL, cb) {
    if (!Social.provider)
      return;

    this.panel.hidden = false;
    let iframe = this.iframe;
    // same url with only ref difference does not cause a new load, so we
    // want to go right to the callback
    let src = iframe.contentDocument && iframe.contentDocument.documentURIObject;
    if (!src || !src.equalsExceptRef(Services.io.newURI(aURL, null, null))) {
      iframe.addEventListener("load", function documentLoaded() {
        iframe.removeEventListener("load", documentLoaded, true);
        cb();
      }, true);
      // Force a layout flush by calling .clientTop so
      // that the docShell of this frame is created
      iframe.clientTop;
      Social.setErrorListener(iframe, SocialFlyout.setFlyoutErrorMessage.bind(SocialFlyout))
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
  get panel() {
    return document.getElementById("social-share-panel");
  },

  get iframe() {
    // first element is our menu vbox.
    if (this.panel.childElementCount == 1)
      return null;
    else
      return this.panel.lastChild;
  },

  uninit: function () {
    if (this.iframe) {
      this.iframe.remove();
    }
  },

  _createFrame: function() {
    let panel = this.panel;
    if (!SocialUI.enabled || this.iframe)
      return;
    this.panel.hidden = false;
    // create and initialize the panel for this window
    let iframe = document.createElement("iframe");
    iframe.setAttribute("type", "content");
    iframe.setAttribute("class", "social-share-frame");
    iframe.setAttribute("context", "contentAreaContextMenu");
    iframe.setAttribute("tooltip", "aHTMLTooltip");
    iframe.setAttribute("flex", "1");
    panel.appendChild(iframe);
    this.populateProviderMenu();
  },

  getSelectedProvider: function() {
    let provider;
    let lastProviderOrigin = this.iframe && this.iframe.getAttribute("origin");
    if (lastProviderOrigin) {
      provider = Social._getProviderFromOrigin(lastProviderOrigin);
    }
    if (!provider)
      provider = Social.provider || Social.defaultProvider;
    // if our provider has no shareURL, select the first one that does
    if (provider && !provider.shareURL) {
      let providers = [p for (p of Social.providers) if (p.shareURL)];
      provider = providers.length > 0  && providers[0];
    }
    return provider;
  },

  populateProviderMenu: function() {
    if (!this.iframe)
      return;
    let providers = [p for (p of Social.providers) if (p.shareURL)];
    let hbox = document.getElementById("social-share-provider-buttons");
    // selectable providers are inserted before the provider-menu seperator,
    // remove any menuitems in that area
    while (hbox.firstChild) {
      hbox.removeChild(hbox.firstChild);
    }
    // reset our share toolbar
    // only show a selection if there is more than one
    if (!SocialUI.enabled || providers.length < 2) {
      this.panel.firstChild.hidden = true;
      return;
    }
    let selectedProvider = this.getSelectedProvider();
    for (let provider of providers) {
      let button = document.createElement("toolbarbutton");
      button.setAttribute("class", "toolbarbutton share-provider-button");
      button.setAttribute("type", "radio");
      button.setAttribute("group", "share-providers");
      button.setAttribute("image", provider.iconURL);
      button.setAttribute("tooltiptext", provider.name);
      button.setAttribute("origin", provider.origin);
      button.setAttribute("oncommand", "SocialShare.sharePage(this.getAttribute('origin')); this.checked=true;");
      if (provider == selectedProvider) {
        this.defaultButton = button;
      }
      hbox.appendChild(button);
    }
    if (!this.defaultButton) {
      this.defaultButton = hbox.firstChild
    }
    this.defaultButton.setAttribute("checked", "true");
    this.panel.firstChild.hidden = false;
  },

  get shareButton() {
    return document.getElementById("social-share-button");
  },

  canSharePage: function(aURI) {
    // we do not enable sharing from private sessions
    if (PrivateBrowsingUtils.isWindowPrivate(window))
      return false;

    if (!aURI || !(aURI.schemeIs('http') || aURI.schemeIs('https')))
      return false;
    return true;
  },

  update: function() {
    let shareButton = this.shareButton;
    shareButton.hidden = !SocialUI.enabled ||
                         [p for (p of Social.providers) if (p.shareURL)].length == 0;
    shareButton.disabled = shareButton.hidden || !this.canSharePage(gBrowser.currentURI);

    // also update the relevent command's disabled state so the keyboard
    // shortcut only works when available.
    let cmd = document.getElementById("Social:SharePage");
    cmd.setAttribute("disabled", shareButton.disabled ? "true" : "false");
  },

  onShowing: function() {
    this.shareButton.setAttribute("open", "true");
  },

  onHidden: function() {
    this.shareButton.removeAttribute("open");
    this.iframe.setAttribute("src", "data:text/plain;charset=utf8,");
    this.currentShare = null;
  },

  setErrorMessage: function() {
    let iframe = this.iframe;
    if (!iframe)
      return;

    iframe.removeAttribute("src");
    iframe.webNavigation.loadURI("about:socialerror?mode=compactInfo&origin=" +
                                 encodeURIComponent(iframe.getAttribute("origin")),
                                 null, null, null, null);
    sizeSocialPanelToContent(this.panel, iframe);
  },

  sharePage: function(providerOrigin, graphData) {
    // if providerOrigin is undefined, we use the last-used provider, or the
    // current/default provider.  The provider selection in the share panel
    // will call sharePage with an origin for us to switch to.
    this._createFrame();
    let iframe = this.iframe;
    let provider;
    if (providerOrigin)
      provider = Social._getProviderFromOrigin(providerOrigin);
    else
      provider = this.getSelectedProvider();
    if (!provider || !provider.shareURL)
      return;

    // graphData is an optional param that either defines the full set of data
    // to be shared, or partial data about the current page. It is set by a call
    // in mozSocial API, or via nsContentMenu calls. If it is present, it MUST
    // define at least url. If it is undefined, we're sharing the current url in
    // the browser tab.
    let sharedURI = graphData ? Services.io.newURI(graphData.url, null, null) :
                                gBrowser.currentURI;
    if (!this.canSharePage(sharedURI))
      return;

    // the point of this action type is that we can use existing share
    // endpoints (e.g. oexchange) that do not support additional
    // socialapi functionality.  One tweak is that we shoot an event
    // containing the open graph data.
    let pageData = graphData ? graphData : this.currentShare;
    if (!pageData || sharedURI == gBrowser.currentURI) {
      pageData = OpenGraphBuilder.getData(gBrowser);
      if (graphData) {
        // overwrite data retreived from page with data given to us as a param
        for (let p in graphData) {
          pageData[p] = graphData[p];
        }
      }
    }
    this.currentShare = pageData;

    let shareEndpoint = OpenGraphBuilder.generateEndpointURL(provider.shareURL, pageData);

    this._dynamicResizer = new DynamicResizeWatcher();
    // if we've already loaded this provider/page share endpoint, we don't want
    // to add another load event listener.
    let reload = true;
    let endpointMatch = shareEndpoint == iframe.getAttribute("src");
    let docLoaded = iframe.contentDocument && iframe.contentDocument.readyState == "complete";
    if (endpointMatch && docLoaded) {
      reload = shareEndpoint != iframe.contentDocument.location.spec;
    }
    if (!reload) {
      this._dynamicResizer.start(this.panel, iframe);
      iframe.docShell.isActive = true;
      iframe.docShell.isAppTab = true;
      let evt = iframe.contentDocument.createEvent("CustomEvent");
      evt.initCustomEvent("OpenGraphData", true, true, JSON.stringify(pageData));
      iframe.contentDocument.documentElement.dispatchEvent(evt);
    } else {
      // first time load, wait for load and dispatch after load
      iframe.addEventListener("load", function panelBrowserOnload(e) {
        iframe.removeEventListener("load", panelBrowserOnload, true);
        iframe.docShell.isActive = true;
        iframe.docShell.isAppTab = true;
        setTimeout(function() {
          if (SocialShare._dynamicResizer) { // may go null if hidden quickly
            SocialShare._dynamicResizer.start(iframe.parentNode, iframe);
          }
        }, 0);
        let evt = iframe.contentDocument.createEvent("CustomEvent");
        evt.initCustomEvent("OpenGraphData", true, true, JSON.stringify(pageData));
        iframe.contentDocument.documentElement.dispatchEvent(evt);
      }, true);
    }
    // always ensure that origin belongs to the endpoint
    let uri = Services.io.newURI(shareEndpoint, null, null);
    iframe.setAttribute("origin", provider.origin);
    iframe.setAttribute("src", shareEndpoint);

    let navBar = document.getElementById("nav-bar");
    let anchor = document.getAnonymousElementByAttribute(this.shareButton, "class", "toolbarbutton-icon");
    this.panel.openPopup(anchor, "bottomcenter topright", 0, 0, false, false);
    Social.setErrorListener(iframe, this.setErrorMessage.bind(this));
  }
};

SocialSidebar = {
  // Whether the sidebar can be shown for this window.
  get canShow() {
    if (PrivateBrowsingUtils.isWindowPrivate(window))
      return false;
    return [p for (p of Social.providers) if (p.enabled && p.sidebarURL)].length > 0;
  },

  // Whether the user has toggled the sidebar on (for windows where it can appear)
  get opened() {
    return Services.prefs.getBoolPref("social.sidebar.open") && !document.mozFullScreen;
  },

  setSidebarVisibilityState: function(aEnabled) {
    let sbrowser = document.getElementById("social-sidebar-browser");
    // it's possible we'll be called twice with aEnabled=false so let's
    // just assume we may often be called with the same state.
    if (aEnabled == sbrowser.docShellIsActive)
      return;
    sbrowser.docShellIsActive = aEnabled;
    let evt = sbrowser.contentDocument.createEvent("CustomEvent");
    evt.initCustomEvent(aEnabled ? "socialFrameShow" : "socialFrameHide", true, true, {});
    sbrowser.contentDocument.documentElement.dispatchEvent(evt);
  },

  updateToggleNotifications: function() {
    let command = document.getElementById("Social:ToggleNotifications");
    command.setAttribute("checked", Services.prefs.getBoolPref("social.toast-notifications.enabled"));
    command.setAttribute("hidden", !SocialUI.enabled);
  },

  update: function SocialSidebar_update() {
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

    let sbrowser = document.getElementById("social-sidebar-browser");

    if (hideSidebar) {
      sbrowser.removeEventListener("load", SocialSidebar._loadListener, true);
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
      sbrowser.setAttribute("origin", Social.provider.origin);
      if (Social.provider.errorState == "frameworker-error") {
        SocialSidebar.setSidebarErrorMessage();
        return;
      }

      // Make sure the right sidebar URL is loaded
      if (sbrowser.getAttribute("src") != Social.provider.sidebarURL) {
        // we check readyState right after setting src, we need a new content
        // viewer to ensure we are checking against the correct document.
        sbrowser.docShell.createAboutBlankContentViewer(null);
        Social.setErrorListener(sbrowser, this.setSidebarErrorMessage.bind(this));
        // setting isAppTab causes clicks on untargeted links to open new tabs
        sbrowser.docShell.isAppTab = true;
        sbrowser.setAttribute("src", Social.provider.sidebarURL);
        PopupNotifications.locationChange(sbrowser);
      }

      // if the document has not loaded, delay until it is
      if (sbrowser.contentDocument.readyState != "complete") {
        document.getElementById("social-sidebar-button").setAttribute("loading", "true");
        sbrowser.addEventListener("load", SocialSidebar._loadListener, true);
      } else {
        this.setSidebarVisibilityState(true);
      }
    }
    this._updateCheckedMenuItems(this.opened && this.provider ? this.provider.origin : null);
  },

  _loadListener: function SocialSidebar_loadListener() {
    let sbrowser = document.getElementById("social-sidebar-browser");
    sbrowser.removeEventListener("load", SocialSidebar._loadListener, true);
    document.getElementById("social-sidebar-button").removeAttribute("loading");
    SocialSidebar.setSidebarVisibilityState(true);
  },

  unloadSidebar: function SocialSidebar_unloadSidebar() {
    let sbrowser = document.getElementById("social-sidebar-browser");
    if (!sbrowser.hasAttribute("origin"))
      return;

    sbrowser.stop();
    sbrowser.removeAttribute("origin");
    sbrowser.setAttribute("src", "about:blank");
    // We need to explicitly create a new content viewer because the old one
    // doesn't get destroyed until about:blank has loaded (which does not happen
    // as long as the element is hidden).
    sbrowser.docShell.createAboutBlankContentViewer(null);
    SocialFlyout.unload();
  },

  _unloadTimeoutId: 0,

  setSidebarErrorMessage: function() {
    let sbrowser = document.getElementById("social-sidebar-browser");
    // a frameworker error "trumps" a sidebar error.
    if (Social.provider.errorState == "frameworker-error") {
      sbrowser.setAttribute("src", "about:socialerror?mode=workerFailure");
    } else {
      let url = encodeURIComponent(Social.provider.sidebarURL);
      sbrowser.loadURI("about:socialerror?mode=tryAgain&url=" + url, null, null);
    }
  },

  // provider will move to a sidebar specific member in bug 894806
  get provider() {
    return Social.provider;
  },

  setProvider: function(origin) {
    Social.setProviderByOrigin(origin);
    this._updateHeader();
    this._updateCheckedMenuItems(origin);
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
    this.setProvider(origin);
    Services.prefs.setBoolPref("social.sidebar.open", true);
  },

  hide: function() {
    Services.prefs.setBoolPref("social.sidebar.open", false);
    this._updateCheckedMenuItems();
  },

  populateSidebarMenu: function(event) {
    // Providers are removed from the view->sidebar menu when there is a change
    // in providers, so we only have to populate onshowing if there are no
    // provider menus. We populate this menu so long as there are enabled
    // providers with sidebars.
    let popup = event.target;
    let providerMenuSeps = popup.getElementsByClassName("social-provider-menu");
    if (providerMenuSeps[0].previousSibling.nodeName == "menuseparator")
      SocialSidebar._populateProviderMenu(providerMenuSeps[0]);
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

  _populateProviderMenu: function(providerMenuSep) {
    let menu = providerMenuSep.parentNode;
    // selectable providers are inserted before the provider-menu seperator,
    // remove any menuitems in that area
    while (providerMenuSep.previousSibling.nodeName == "menuitem") {
      menu.removeChild(providerMenuSep.previousSibling);
    }
    // only show a selection in the sidebar header menu if there is more than one
    let providers = [p for (p of Social.providers) if (p.sidebarURL)];
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

// this helper class is used by removable/customizable buttons to handle
// widget creation/destruction

// When a provider is installed we show all their UI so the user will see the
// functionality of what they installed. The user can later customize the UI,
// moving buttons around or off the toolbar.
//
// On startup, we create the button widgets of any enabled provider.
// CustomizableUI handles placement and persistence of placement.
function ToolbarHelper(type, createButtonFn, listener) {
  this._createButton = createButtonFn;
  this._type = type;

  if (listener) {
    CustomizableUI.addListener(listener);
    // remove this listener on window close
    window.addEventListener("unload", () => {
      CustomizableUI.removeListener(listener);
    });
  }
}

ToolbarHelper.prototype = {
  idFromOrigin: function(origin) {
    // this id needs to pass the checks in CustomizableUI, so remove characters
    // that wont pass.
    return this._type + "-" + Services.io.newURI(origin, null, null).hostPort.replace(/[\.:]/g,'-');
  },

  // should be called on disable of a provider
  removeProviderButton: function(origin) {
    CustomizableUI.destroyWidget(this.idFromOrigin(origin));
  },

  clearPalette: function() {
    [this.removeProviderButton(p.origin) for (p of Social.providers)];
  },

  // should be called on enable of a provider
  populatePalette: function() {
    if (!Social.enabled) {
      this.clearPalette();
      return;
    }

    // create any buttons that do not exist yet if they have been persisted
    // as a part of the UI (otherwise they belong in the palette).
    for (let provider of Social.providers) {
      let id = this.idFromOrigin(provider.origin);
      this._createButton(id, provider);
    }
  }
}

let SocialStatusWidgetListener = {
  _getNodeOrigin: function(aWidgetId) {
    // we rely on the button id being the same as the widget.
    let node = document.getElementById(aWidgetId);
    if (!node)
      return null
    if (!node.classList.contains("social-status-button"))
      return null
    return node.getAttribute("origin");
  },
  onWidgetAdded: function(aWidgetId, aArea, aPosition) {
    let origin = this._getNodeOrigin(aWidgetId);
    if (origin)
      SocialStatus.updateButton(origin);
  },
  onWidgetRemoved: function(aWidgetId, aPrevArea) {
    let origin = this._getNodeOrigin(aWidgetId);
    if (!origin)
      return;
    // When a widget is demoted to the palette ('removed'), it's visual
    // style should change.
    SocialStatus.updateButton(origin);
    SocialStatus._removeFrame(origin);
  }
}

SocialStatus = {
  populateToolbarPalette: function() {
    this._toolbarHelper.populatePalette();
  },

  removeProvider: function(origin) {
    this._removeFrame(origin);
    this._toolbarHelper.removeProviderButton(origin);
  },

  reloadProvider: function(origin) {
    let button = document.getElementById(this._toolbarHelper.idFromOrigin(origin));
    if (button && button.getAttribute("open") == "true")
      document.getElementById("social-notification-panel").hidePopup();
    this._removeFrame(origin);
  },

  _removeFrame: function(origin) {
    let notificationFrameId = "social-status-" + origin;
    let frame = document.getElementById(notificationFrameId);
    if (frame) {
      SharedFrame.forgetGroup(frame.id);
      frame.parentNode.removeChild(frame);
    }
  },

  get _toolbarHelper() {
    delete this._toolbarHelper;
    this._toolbarHelper = new ToolbarHelper("social-status-button",
                                            CreateSocialStatusWidget,
                                            SocialStatusWidgetListener);
    return this._toolbarHelper;
  },

  get _dynamicResizer() {
    delete this._dynamicResizer;
    this._dynamicResizer = new DynamicResizeWatcher();
    return this._dynamicResizer;
  },

  // status panels are one-per button per-process, we swap the docshells between
  // windows when necessary
  _attachNotificatonPanel: function(aParent, aButton, provider) {
    aParent.hidden = !SocialUI.enabled;
    let notificationFrameId = "social-status-" + provider.origin;
    let frame = document.getElementById(notificationFrameId);

    // If the button was customized to a new location, we we'll destroy the
    // iframe and start fresh.
    if (frame && frame.parentNode != aParent) {
      SharedFrame.forgetGroup(frame.id);
      frame.parentNode.removeChild(frame);
      frame = null;
    }

    if (!frame) {
      frame = SharedFrame.createFrame(
        notificationFrameId, /* frame name */
        aParent, /* parent */
        {
          "type": "content",
          "mozbrowser": "true",
          "class": "social-panel-frame",
          "id": notificationFrameId,
          "tooltip": "aHTMLTooltip",
          "context": "contentAreaContextMenu",
          "flex": "1",

          // work around bug 793057 - by making the panel roughly the final size
          // we are more likely to have the anchor in the correct position.
          "style": "width: " + PANEL_MIN_WIDTH + "px;",

          "origin": provider.origin,
          "src": provider.statusURL
        }
      );

      if (frame.socialErrorListener)
        frame.socialErrorListener.remove();
      if (frame.docShell) {
        frame.docShell.isActive = false;
        Social.setErrorListener(frame, this.setPanelErrorMessage.bind(this));
      }
    } else {
      frame.setAttribute("origin", provider.origin);
      SharedFrame.updateURL(notificationFrameId, provider.statusURL);
    }
    aButton.setAttribute("notificationFrameId", notificationFrameId);
  },

  updateButton: function(origin) {
    let id = this._toolbarHelper.idFromOrigin(origin);
    let widget = CustomizableUI.getWidget(id);
    if (!widget)
      return;
    let button = widget.forWindow(window).node;
    if (button) {
      // we only grab the first notification, ignore all others
      let place = CustomizableUI.getPlaceForItem(button);
      let provider = Social._getProviderFromOrigin(origin);
      let icons = provider.ambientNotificationIcons;
      let iconNames = Object.keys(icons);
      let notif = icons[iconNames[0]];

      // The image and tooltip need to be updated for both
      // ambient notification and profile changes.
      let iconURL = provider.icon32URL || provider.iconURL;
      let tooltiptext;
      if (!notif || place == "palette") {
        button.style.listStyleImage = "url(" + iconURL + ")";
        button.setAttribute("badge", "");
        button.setAttribute("aria-label", "");
        button.setAttribute("tooltiptext", provider.name);
        return;
      }
      button.style.listStyleImage = "url(" + (notif.iconURL || iconURL) + ")";
      button.setAttribute("tooltiptext", notif.label || provider.name);

      let badge = notif.counter || "";
      button.setAttribute("badge", badge);
      let ariaLabel = notif.label;
      // if there is a badge value, we must use a localizable string to insert it.
      if (badge)
        ariaLabel = gNavigatorBundle.getFormattedString("social.aria.toolbarButtonBadgeText",
                                                        [ariaLabel, badge]);
      button.setAttribute("aria-label", ariaLabel);
    }
  },

  showPopup: function(aToolbarButton) {
    // attach our notification panel if necessary
    let origin = aToolbarButton.getAttribute("origin");
    let provider = Social._getProviderFromOrigin(origin);

    // if we're a slice in the hamburger, use that panel instead
    let widgetGroup = CustomizableUI.getWidget(aToolbarButton.getAttribute("id"));
    let widget = widgetGroup.forWindow(window);
    let panel, showingEvent, hidingEvent;
    let inMenuPanel = widgetGroup.areaType == CustomizableUI.TYPE_MENU_PANEL;
    if (inMenuPanel) {
      panel = document.getElementById("PanelUI-socialapi");
      this._attachNotificatonPanel(panel, aToolbarButton, provider);
      widget.node.setAttribute("closemenu", "none");
      showingEvent = "ViewShowing";
      hidingEvent = "ViewHiding";
    } else {
      panel = document.getElementById("social-notification-panel");
      this._attachNotificatonPanel(panel, aToolbarButton, provider);
      showingEvent = "popupshown";
      hidingEvent = "popuphidden";
    }
    let notificationFrameId = aToolbarButton.getAttribute("notificationFrameId");
    let notificationFrame = document.getElementById(notificationFrameId);

    let wasAlive = SharedFrame.isGroupAlive(notificationFrameId);
    SharedFrame.setOwner(notificationFrameId, notificationFrame);

    // Clear dimensions on all browsers so the panel size will
    // only use the selected browser.
    let frameIter = panel.firstElementChild;
    while (frameIter) {
      frameIter.collapsed = (frameIter != notificationFrame);
      frameIter = frameIter.nextElementSibling;
    }

    function dispatchPanelEvent(name) {
      let evt = notificationFrame.contentDocument.createEvent("CustomEvent");
      evt.initCustomEvent(name, true, true, {});
      notificationFrame.contentDocument.documentElement.dispatchEvent(evt);
    }

    // we only use a dynamic resizer when we're located the toolbar.
    let dynamicResizer = inMenuPanel ? null : this._dynamicResizer;
    panel.addEventListener(hidingEvent, function onpopuphiding() {
      panel.removeEventListener(hidingEvent, onpopuphiding);
      aToolbarButton.removeAttribute("open");
      if (dynamicResizer)
        dynamicResizer.stop();
      notificationFrame.docShell.isActive = false;
      dispatchPanelEvent("socialFrameHide");
    });

    panel.addEventListener(showingEvent, function onpopupshown() {
      panel.removeEventListener(showingEvent, onpopupshown);
      // This attribute is needed on both the button and the
      // containing toolbaritem since the buttons on OS X have
      // moz-appearance:none, while their container gets
      // moz-appearance:toolbarbutton due to the way that toolbar buttons
      // get combined on OS X.
      let initFrameShow = () => {
        notificationFrame.docShell.isActive = true;
        notificationFrame.docShell.isAppTab = true;
        if (dynamicResizer)
          dynamicResizer.start(panel, notificationFrame);
        dispatchPanelEvent("socialFrameShow");
      };
      if (!inMenuPanel)
        aToolbarButton.setAttribute("open", "true");
      if (notificationFrame.contentDocument &&
          notificationFrame.contentDocument.readyState == "complete" && wasAlive) {
        initFrameShow();
      } else {
        // first time load, wait for load and dispatch after load
        notificationFrame.addEventListener("load", function panelBrowserOnload(e) {
          notificationFrame.removeEventListener("load", panelBrowserOnload, true);
          initFrameShow();
        }, true);
      }
    });

    if (inMenuPanel) {
      PanelUI.showSubView("PanelUI-socialapi", widget.node,
                          CustomizableUI.AREA_PANEL);
    } else {
      let anchor = document.getAnonymousElementByAttribute(aToolbarButton, "class", "toolbarbutton-badge-container");
      // Bug 849216 - open the popup in a setTimeout so we avoid the auto-rollup
      // handling from preventing it being opened in some cases.
      setTimeout(function() {
        panel.openPopup(anchor, "bottomcenter topright", 0, 0, false, false);
      }, 0);
    }
  },

  setPanelErrorMessage: function(aNotificationFrame) {
    if (!aNotificationFrame)
      return;

    let src = aNotificationFrame.getAttribute("src");
    aNotificationFrame.removeAttribute("src");
    aNotificationFrame.webNavigation.loadURI("about:socialerror?mode=tryAgainOnly&url=" +
                                             encodeURIComponent(src),
                                             null, null, null, null);
    let panel = aNotificationFrame.parentNode;
    sizeSocialPanelToContent(panel, aNotificationFrame);
  },

};


/**
 * SocialMarks
 *
 * Handles updates to toolbox and signals all buttons to update when necessary.
 */
SocialMarks = {
  update: function() {
    // signal each button to update itself
    let currentButtons = document.querySelectorAll('toolbarbutton[type="socialmark"]');
    for (let elt of currentButtons)
      elt.update();
  },

  updatePanelButtons: function() {
    // querySelectorAll does not work on the menu panel the panel, so we have to
    // do this the hard way.
    let providers = SocialMarks.getProviders();
    let panel =  document.getElementById("PanelUI-popup");
    for (let p of providers) {
      let widgetId = SocialMarks._toolbarHelper.idFromOrigin(p.origin);
      let widget = CustomizableUI.getWidget(widgetId);
      if (!widget)
        continue;
      let node = widget.forWindow(window).node;
      if (node)
        node.update();
    }
  },

  getProviders: function() {
    // only rely on providers that the user has placed in the UI somewhere. This
    // also means that populateToolbarPalette must be called prior to using this
    // method, otherwise you get a big fat zero. For our use case with context
    // menu's, this is ok.
    let tbh = this._toolbarHelper;
    return [p for (p of Social.providers) if (p.markURL &&
                                              document.getElementById(tbh.idFromOrigin(p.origin)))];
  },

  populateContextMenu: function() {
    // only show a selection if enabled and there is more than one
    let providers = this.getProviders();

    // remove all previous entries by class
    let menus = [m for (m of document.getElementsByClassName("context-socialmarks"))];
    [m.parentNode.removeChild(m) for (m of menus)];

    let contextMenus = [
      {
        type: "link",
        id: "context-marklinkMenu",
        label: "social.marklinkMenu.label"
      },
      {
        type: "page",
        id: "context-markpageMenu",
        label: "social.markpageMenu.label"
      }
    ];
    for (let cfg of contextMenus) {
      this._populateContextPopup(cfg, providers);
    }
  },

  MENU_LIMIT: 3, // adjustable for testing
  _populateContextPopup: function(menuInfo, providers) {
    let menu = document.getElementById(menuInfo.id);
    let popup = menu.firstChild;
    for (let provider of providers) {
      // We show up to MENU_LIMIT providers as single menuitems's at the top
      // level of the context menu, if we have more than that, dump them *all*
      // into the menu popup.
      let mi = document.createElement("menuitem");
      mi.setAttribute("oncommand", "gContextMenu.markLink(this.getAttribute('origin'));");
      mi.setAttribute("origin", provider.origin);
      mi.setAttribute("image", provider.iconURL);
      if (providers.length <= this.MENU_LIMIT) {
        // an extra class to make enable/disable easy
        mi.setAttribute("class", "menuitem-iconic context-socialmarks context-mark"+menuInfo.type);
        let menuLabel = gNavigatorBundle.getFormattedString(menuInfo.label, [provider.name]);
        mi.setAttribute("label", menuLabel);
        menu.parentNode.insertBefore(mi, menu);
      } else {
        mi.setAttribute("class", "menuitem-iconic context-socialmarks");
        mi.setAttribute("label", provider.name);
        popup.appendChild(mi);
      }
    }
  },

  populateToolbarPalette: function() {
    this._toolbarHelper.populatePalette();
    this.populateContextMenu();
  },

  removeProvider: function(origin) {
    this._toolbarHelper.removeProviderButton(origin);
  },

  get _toolbarHelper() {
    delete this._toolbarHelper;
    this._toolbarHelper = new ToolbarHelper("social-mark-button", CreateSocialMarkWidget);
    return this._toolbarHelper;
  },

  markLink: function(aOrigin, aUrl) {
    // find the button for this provider, and open it
    let id = this._toolbarHelper.idFromOrigin(aOrigin);
    document.getElementById(id).markLink(aUrl);
  }
};

})();
