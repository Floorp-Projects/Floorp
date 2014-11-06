// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// the "exported" symbols
let SocialUI,
    SocialFlyout,
    SocialMarks,
    SocialShare,
    SocialSidebar,
    SocialStatus;

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

    Services.obs.addObserver(this, "social:ambient-notification-changed", false);
    Services.obs.addObserver(this, "social:profile-changed", false);
    Services.obs.addObserver(this, "social:frameworker-error", false);
    Services.obs.addObserver(this, "social:providers-changed", false);
    Services.obs.addObserver(this, "social:provider-reload", false);
    Services.obs.addObserver(this, "social:provider-enabled", false);
    Services.obs.addObserver(this, "social:provider-disabled", false);

    Services.prefs.addObserver("social.toast-notifications.enabled", this, false);

    gBrowser.addEventListener("ActivateSocialFeature", this._activationEventHandler.bind(this), true, true);
    CustomizableUI.addListener(this);

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
    Services.obs.removeObserver(this, "social:profile-changed");
    Services.obs.removeObserver(this, "social:frameworker-error");
    Services.obs.removeObserver(this, "social:providers-changed");
    Services.obs.removeObserver(this, "social:provider-reload");
    Services.obs.removeObserver(this, "social:provider-enabled");
    Services.obs.removeObserver(this, "social:provider-disabled");

    Services.prefs.removeObserver("social.toast-notifications.enabled", this);
    CustomizableUI.removeListener(this);

    document.getElementById("viewSidebarMenu").removeEventListener("popupshowing", SocialSidebar.populateSidebarMenu, true);
    document.getElementById("social-statusarea-popup").removeEventListener("popupshowing", SocialSidebar.populateSidebarMenu, true);

    this._initialized = false;
  },

  observe: function SocialUI_observe(subject, topic, data) {
    switch (topic) {
      case "social:provider-enabled":
        SocialMarks.populateToolbarPalette();
        SocialStatus.populateToolbarPalette();
        break;
      case "social:provider-disabled":
        SocialMarks.removeProvider(data);
        SocialStatus.removeProvider(data);
        SocialSidebar.disableProvider(data);
        break;
      case "social:provider-reload":
        SocialStatus.reloadProvider(data);
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
        SocialStatus.updateButton(data);
        break;
      case "social:profile-changed":
        // make sure anything that happens here only affects the provider for
        // which the profile is changing, and that anything we call actually
        // needs to change based on profile data.
        SocialStatus.updateButton(data);
        break;
      case "social:frameworker-error":
        if (this.enabled && SocialSidebar.provider && SocialSidebar.provider.origin == data) {
          SocialSidebar.setSidebarErrorMessage();
        }
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
    SocialStatus.populateToolbarPalette();
    SocialMarks.populateToolbarPalette();
  },

  // This handles "ActivateSocialFeature" events fired against content documents
  // in this window.  If this activation happens from within Firefox, such as
  // about:home or the share panel, we bypass the enable prompt. Any website
  // activation, such as from the activations directory or a providers website
  // will still get the prompt.
  _activationEventHandler: function SocialUI_activationHandler(e, options={}) {
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

    // The share panel iframe will not match "content" so it passes a bypass
    // flag
    if (!options.bypassContentCheck && targetDoc.defaultView != content)
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
          if (!widget.areaType) {
            CustomizableUI.addWidgetToArea("social-share-button", CustomizableUI.AREA_NAVBAR);
            // ensure correct state
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
    return Social.providers.length > 0;
  },

  canShareOrMarkPage: function(aURI) {
    // Bug 898706 we do not enable social in private sessions since frameworker
    // would be shared between private and non-private windows
    if (PrivateBrowsingUtils.isWindowPrivate(window))
      return false;

    return (aURI && (aURI.schemeIs('http') || aURI.schemeIs('https')));
  },

  onCustomizeEnd: function(aWindow) {
    if (aWindow != window)
      return;
    // customization mode gets buttons out of sync with command updating, fix
    // the disabled state
    let canShare = this.canShareOrMarkPage(gBrowser.currentURI);
    let shareButton = SocialShare.shareButton;
    if (shareButton) {
      if (canShare) {
        shareButton.removeAttribute("disabled")
      } else {
        shareButton.setAttribute("disabled", "true")
      }
    }
    // update the disabled state of the button based on the command
    for (let node of SocialMarks.nodes) {
      if (canShare) {
        node.removeAttribute("disabled")
      } else {
        node.setAttribute("disabled", "true")
      }
    }
  },

  // called on tab/urlbar/location changes and after customization. Update
  // anything that is tab specific.
  updateState: function() {
    if (location == "about:customizing")
      return;
    goSetCommandEnabled("Social:PageShareOrMark", this.canShareOrMarkPage(gBrowser.currentURI));
    if (!SocialUI.enabled)
      return;
    // larger update that may change button icons
    SocialMarks.update();
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
    iframe.setAttribute("origin", SocialSidebar.provider.origin);
    panel.appendChild(iframe);
  },

  setFlyoutErrorMessage: function SF_setFlyoutErrorMessage() {
    this.iframe.removeAttribute("src");
    this.iframe.webNavigation.loadURI("about:socialerror?mode=compactInfo&origin=" +
                                 encodeURIComponent(this.iframe.getAttribute("origin")),
                                 null, null, null, null);
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
    if (!SocialSidebar.provider)
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

  _activationHandler: function(event) {
    if (!Services.prefs.getBoolPref("social.share.activationPanelEnabled"))
      return;
    SocialUI._activationEventHandler(event, { bypassContentCheck: true, bypassInstallPanel: true });
  },

  uninit: function () {
    if (this.iframe) {
      this.iframe.removeEventListener("ActivateSocialFeature", this._activationHandler, true, true);
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
    panel.appendChild(iframe);
    this.iframe.addEventListener("ActivateSocialFeature", this._activationHandler, true, true);
    this.populateProviderMenu();
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
    let providers = [p for (p of Social.providers) if (p.shareURL)];
    let hbox = document.getElementById("social-share-provider-buttons");
    // remove everything before the add-share-provider button (which should also
    // be lastChild if any share providers were added)
    let addButton = document.getElementById("add-share-provider");
    while (hbox.firstChild != addButton) {
      hbox.removeChild(hbox.firstChild);
    }
    let selectedProvider = this.getSelectedProvider();
    for (let provider of providers) {
      let button = document.createElement("toolbarbutton");
      button.setAttribute("class", "toolbarbutton share-provider-button");
      button.setAttribute("type", "radio");
      button.setAttribute("group", "share-providers");
      button.setAttribute("image", provider.iconURL);
      button.setAttribute("tooltip", "share-button-tooltip");
      button.setAttribute("origin", provider.origin);
      button.setAttribute("oncommand", "SocialShare.sharePage(this.getAttribute('origin'));");
      if (provider == selectedProvider) {
        this.defaultButton = button;
      }
      hbox.insertBefore(button, addButton);
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
    this.anchor.setAttribute("open", "true");
    this.iframe.addEventListener("click", this._onclick, true);
  },

  onHidden: function() {
    this.anchor.removeAttribute("open");
    this.iframe.removeEventListener("click", this._onclick, true);
    this.iframe.setAttribute("src", "data:text/plain;charset=utf8,");
    // make sure that the frame is unloaded after it is hidden
    this.iframe.docShell.createAboutBlankContentViewer(null);
    this.currentShare = null;
    // share panel use is over, purge any history
    if (this.iframe.sessionHistory) {
      let purge = this.iframe.sessionHistory.count;
      if (purge > 0)
        this.iframe.sessionHistory.PurgeHistory(purge);
    }
  },

  setErrorMessage: function() {
    let iframe = this.iframe;
    if (!iframe)
      return;

    let url;
    let origin = iframe.getAttribute("origin");
    if (!origin) {
      // directory site is down
      url = "about:socialerror?mode=tryAgainOnly&directory=1&url=" + encodeURIComponent(iframe.getAttribute("src"));
    } else {
      url = "about:socialerror?mode=compactInfo&origin=" + encodeURIComponent(origin);
    }
    iframe.webNavigation.loadURI(url, null, null, null, null);
    sizeSocialPanelToContent(this.panel, iframe);
  },

  sharePage: function(providerOrigin, graphData, target) {
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
    if (!SocialUI.canShareOrMarkPage(sharedURI))
      return;

    // the point of this action type is that we can use existing share
    // endpoints (e.g. oexchange) that do not support additional
    // socialapi functionality.  One tweak is that we shoot an event
    // containing the open graph data.
    if (!pageData || sharedURI == gBrowser.currentURI) {
      pageData = OpenGraphBuilder.getData(gBrowser);
      if (graphData) {
        // overwrite data retreived from page with data given to us as a param
        for (let p in graphData) {
          pageData[p] = graphData[p];
        }
      }
    }
    // if this is a share of a selected item, get any microdata
    if (!pageData.microdata && target) {
      pageData.microdata = OpenGraphBuilder.getMicrodata(gBrowser, target);
    }
    this.currentShare = pageData;

    let provider;
    if (providerOrigin)
      provider = Social._getProviderFromOrigin(providerOrigin);
    else
      provider = this.getSelectedProvider();
    if (!provider || !provider.shareURL) {
      this.showDirectory();
      return;
    }
    // check the menu button
    let hbox = document.getElementById("social-share-provider-buttons");
    let btn = hbox.querySelector("[origin='" + provider.origin + "']");
    if (btn)
      btn.checked = true;

    let shareEndpoint = OpenGraphBuilder.generateEndpointURL(provider.shareURL, pageData);

    let size = provider.getPageSize("share");
    if (size) {
      this._dynamicResizer.stop();
    }

    // if we've already loaded this provider/page share endpoint, we don't want
    // to add another load event listener.
    let reload = true;
    let endpointMatch = shareEndpoint == iframe.getAttribute("src");
    let docLoaded = iframe.contentDocument && iframe.contentDocument.readyState == "complete";
    if (endpointMatch && docLoaded) {
      reload = shareEndpoint != iframe.contentDocument.location.spec;
    }
    if (!reload) {
      if (!size)
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
        // to support standard share endpoints mimick window.open by setting
        // window.opener, some share endpoints rely on w.opener to know they
        // should close the window when done.
        iframe.contentWindow.opener = iframe.contentWindow;
        setTimeout(function() {
          if (size) {
            let panel = SocialShare.panel;
            let {width, height} = size;
            width += panel.boxObject.width - iframe.boxObject.width;
            height += panel.boxObject.height - iframe.boxObject.height;
            panel.sizeTo(width, height);
          } else {
            SocialShare._dynamicResizer.start(iframe.parentNode, iframe);
          }
        }, 0);
        let evt = iframe.contentDocument.createEvent("CustomEvent");
        evt.initCustomEvent("OpenGraphData", true, true, JSON.stringify(pageData));
        iframe.contentDocument.documentElement.dispatchEvent(evt);
      }, true);
    }
    // if the user switched between share providers we do not want that history
    // available.
    if (iframe.sessionHistory) {
      let purge = iframe.sessionHistory.count;
      if (purge > 0)
        iframe.sessionHistory.PurgeHistory(purge);
    }

    // always ensure that origin belongs to the endpoint
    let uri = Services.io.newURI(shareEndpoint, null, null);
    iframe.setAttribute("origin", provider.origin);
    iframe.setAttribute("src", shareEndpoint);
    this._openPanel();
  },

  showDirectory: function() {
    this._createFrame();
    let iframe = this.iframe;
    iframe.removeAttribute("origin");
    iframe.addEventListener("load", function panelBrowserOnload(e) {
      iframe.removeEventListener("load", panelBrowserOnload, true);
      hookWindowCloseForPanelClose(iframe.contentWindow);
      SocialShare._dynamicResizer.start(iframe.parentNode, iframe);

      iframe.addEventListener("unload", function panelBrowserOnload(e) {
        iframe.removeEventListener("unload", panelBrowserOnload, true);
        SocialShare._dynamicResizer.stop();
      }, true);
    }, true);
    iframe.setAttribute("src", "about:providerdirectory");
    this._openPanel();
  },

  _openPanel: function() {
    let anchor = document.getAnonymousElementByAttribute(this.anchor, "class", "toolbarbutton-icon");
    this.panel.openPopup(anchor, "bottomcenter topright", 0, 0, false, false);
    Social.setErrorListener(this.iframe, this.setErrorMessage.bind(this));
    Services.telemetry.getHistogramById("SOCIAL_TOOLBAR_BUTTONS").add(0);
  }
};

SocialSidebar = {
  _openStartTime: 0,

  // Whether the sidebar can be shown for this window.
  get canShow() {
    if (!SocialUI.enabled || document.mozFullScreen)
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
      document.getElementById("social-sidebar-browser").setAttribute("origin", data.origin);
      if (!data.hidden)
        this.show(data.origin);
    } else if (Services.prefs.prefHasUserValue("social.sidebar.provider")) {
      // no window state, use the global state if it is available
      this.show(Services.prefs.getCharPref("social.sidebar.provider"));
    }
  },

  saveWindowState: function() {
    let broadcaster = document.getElementById("socialSidebarBroadcaster");
    let sidebarOrigin = document.getElementById("social-sidebar-browser").getAttribute("origin");
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
      sbrowser.setAttribute("origin", this.provider.origin);
      if (this.provider.errorState == "frameworker-error") {
        SocialSidebar.setSidebarErrorMessage();
        return;
      }

      // Make sure the right sidebar URL is loaded
      if (sbrowser.getAttribute("src") != this.provider.sidebarURL) {
        // we check readyState right after setting src, we need a new content
        // viewer to ensure we are checking against the correct document.
        sbrowser.docShell.createAboutBlankContentViewer(null);
        Social.setErrorListener(sbrowser, this.setSidebarErrorMessage.bind(this));
        // setting isAppTab causes clicks on untargeted links to open new tabs
        sbrowser.docShell.isAppTab = true;
        sbrowser.setAttribute("src", this.provider.sidebarURL);
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

  _onclick: function() {
    Services.telemetry.getHistogramById("SOCIAL_PANEL_CLICKS").add(3);
  },

  _loadListener: function SocialSidebar_loadListener() {
    let sbrowser = document.getElementById("social-sidebar-browser");
    sbrowser.removeEventListener("load", SocialSidebar._loadListener, true);
    document.getElementById("social-sidebar-button").removeAttribute("loading");
    SocialSidebar.setSidebarVisibilityState(true);
    sbrowser.addEventListener("click", SocialSidebar._onclick, true);
  },

  unloadSidebar: function SocialSidebar_unloadSidebar() {
    let sbrowser = document.getElementById("social-sidebar-browser");
    if (!sbrowser.hasAttribute("origin"))
      return;

    sbrowser.removeEventListener("click", SocialSidebar._onclick, true);
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
    let origin = sbrowser.getAttribute("origin");
    if (origin) {
      origin = "&origin=" + encodeURIComponent(origin);
    }
    if (this.provider.errorState == "frameworker-error") {
      sbrowser.setAttribute("src", "about:socialerror?mode=workerFailure" + origin);
    } else {
      let url = encodeURIComponent(this.provider.sidebarURL);
      sbrowser.loadURI("about:socialerror?mode=tryAgain&url=" + url + origin, null, null);
    }
  },

  _provider: null,
  ensureProvider: function() {
    if (this._provider)
      return;
    // origin for sidebar is persisted, so get the previously selected sidebar
    // first, otherwise fallback to the first provider in the list
    let sbrowser = document.getElementById("social-sidebar-browser");
    let origin = sbrowser.getAttribute("origin");
    let providers = [p for (p of Social.providers) if (p.sidebarURL)];
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

    for (let provider of Social.providers)
      this.updateButton(provider.origin);
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

  updateButton: function(origin) {
    let id = this._toolbarHelper.idFromOrigin(origin);
    let widget = CustomizableUI.getWidget(id);
    if (!widget)
      return;
    let button = widget.forWindow(window).node;
    if (button) {
      // we only grab the first notification, ignore all others
      let provider = Social._getProviderFromOrigin(origin);
      let icons = provider.ambientNotificationIcons;
      let iconNames = Object.keys(icons);
      let notif = icons[iconNames[0]];

      // The image and tooltip need to be updated for both
      // ambient notification and profile changes.
      let iconURL = provider.icon32URL || provider.iconURL;
      let tooltiptext;
      if (!notif || !widget.areaType) {
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

  _onclose: function() {
    let notificationFrameId = "social-status-" + origin;
    let frame = document.getElementById(notificationFrameId);
    frame.removeEventListener("close", this._onclose, true);
    frame.removeEventListener("click", this._onclick, true);
  },

  _onclick: function() {
    Services.telemetry.getHistogramById("SOCIAL_PANEL_CLICKS").add(1);
  },

  showPopup: function(aToolbarButton) {
    // attach our notification panel if necessary
    let origin = aToolbarButton.getAttribute("origin");
    let provider = Social._getProviderFromOrigin(origin);

    PanelFrame.showPopup(window, aToolbarButton, "social", origin,
                         provider.statusURL, provider.getPageSize("status"),
                         (frame) => {
                          frame.addEventListener("close", this._onclose, true);
                          frame.addEventListener("click", this._onclick, true);
                        });
    Services.telemetry.getHistogramById("SOCIAL_TOOLBAR_BUTTONS").add(1);
  },

  setPanelErrorMessage: function(aNotificationFrame) {
    if (!aNotificationFrame)
      return;

    let src = aNotificationFrame.getAttribute("src");
    aNotificationFrame.removeAttribute("src");
    let origin = aNotificationFrame.getAttribute("origin");
    aNotificationFrame.webNavigation.loadURI("about:socialerror?mode=tryAgainOnly&url=" +
                                            encodeURIComponent(src) + "&origin=" +
                                            encodeURIComponent(origin),
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
  get nodes() {
    let providers = [p for (p of Social.providers) if (p.markURL)];
    for (let p of providers) {
      let widgetId = SocialMarks._toolbarHelper.idFromOrigin(p.origin);
      let widget = CustomizableUI.getWidget(widgetId);
      if (!widget)
        continue;
      let node = widget.forWindow(window).node;
      if (node)
        yield node;
    }
  },
  update: function() {
    // querySelectorAll does not work on the menu panel, so we have to do this
    // the hard way.
    for (let node of this.nodes) {
      // xbl binding is not complete on startup when buttons are not in toolbar,
      // verify update is available
      if (node.update) {
        node.update();
      }
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
    this.update();
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

  markLink: function(aOrigin, aUrl, aTarget) {
    // find the button for this provider, and open it
    let id = this._toolbarHelper.idFromOrigin(aOrigin);
    document.getElementById(id).markLink(aUrl, aTarget);
  }
};

})();
