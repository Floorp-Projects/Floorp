// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The minimum sizes for the auto-resize panel code.
const PANEL_MIN_HEIGHT = 100;
const PANEL_MIN_WIDTH = 330;

XPCOMUtils.defineLazyModuleGetter(this, "SharedFrame",
  "resource:///modules/SharedFrame.jsm");

let SocialUI = {
  // Called on delayed startup to initialize the UI
  init: function SocialUI_init() {
    Services.obs.addObserver(this, "social:pref-changed", false);
    Services.obs.addObserver(this, "social:ambient-notification-changed", false);
    Services.obs.addObserver(this, "social:profile-changed", false);
    Services.obs.addObserver(this, "social:recommend-info-changed", false);
    Services.obs.addObserver(this, "social:frameworker-error", false);
    Services.obs.addObserver(this, "social:provider-set", false);

    Services.prefs.addObserver("social.sidebar.open", this, false);
    Services.prefs.addObserver("social.toast-notifications.enabled", this, false);

    gBrowser.addEventListener("ActivateSocialFeature", this._activationEventHandler, true, true);

    // Called when we enter DOM full-screen mode.
    window.addEventListener("mozfullscreenchange", function () {
      SocialSidebar.update();
      SocialChatBar.update();
    });

    Social.init(this._providerReady.bind(this));
  },

  // Called on window unload
  uninit: function SocialUI_uninit() {
    Services.obs.removeObserver(this, "social:pref-changed");
    Services.obs.removeObserver(this, "social:ambient-notification-changed");
    Services.obs.removeObserver(this, "social:profile-changed");
    Services.obs.removeObserver(this, "social:recommend-info-changed");
    Services.obs.removeObserver(this, "social:frameworker-error");
    Services.obs.removeObserver(this, "social:provider-set");

    Services.prefs.removeObserver("social.sidebar.open", this);
    Services.prefs.removeObserver("social.toast-notifications.enabled", this);
  },

  // Called once, after window load, once Social.jsm's provider has been set.
  _providerReady: function SocialUI_providerReady() {
    this._updateActiveUI();
    this._updateMenuItems();

    SocialChatBar.update();
    SocialShareButton.init();
    SocialMenu.populate();
    SocialToolbar.init();
    SocialSidebar.init();
  },

  // Social.provider has changed, update any state that depends on it.
  // Note: this method is not called when Social.provider is first set, during
  // the first window load.
  _updateProvider: function () {
    // XXX audit for handling nullness of social.provider
    this._updateActiveUI();
    this._updateMenuItems();

    SocialChatBar.update();
    SocialShareButton.updateProvider();
    SocialMenu.populate();
    SocialToolbar.updateProvider();
    SocialSidebar.update();
  },

  // The entire feature is being turned on/off.
  _updateEnabledState: function () {
    this._updateActiveUI();
    SocialChatBar.update();
    SocialSidebar.update();
    SocialShareButton.updateButtonHiddenState();
    SocialMenu.populate();
    SocialToolbar.updateButtonHiddenState();
    SocialToolbar.populateProviderMenus();
  },

  _matchesCurrentProvider: function (origin) {
    return Social.provider && Social.provider.origin == origin;
  },

  observe: function SocialUI_observe(subject, topic, data) {
    // Exceptions here sometimes don't get reported properly, report them
    // manually :(
    try {
      switch (topic) {
        case "social:provider-set":
          this._updateProvider();
          break;
        case "social:pref-changed":
          this._updateEnabledState();
          break;

        // Provider-specific notifications
        case "social:ambient-notification-changed":
          if (this._matchesCurrentProvider(data)) {
            SocialToolbar.updateButton();
            SocialMenu.populate();
          }
          break;
        case "social:profile-changed":
          if (this._matchesCurrentProvider(data)) {
            SocialToolbar.updateProfile();
            SocialShareButton.updateProfileInfo();
            SocialChatBar.update();
          }
          break;
        case "social:recommend-info-changed":
          if (this._matchesCurrentProvider(data)) {
            SocialShareButton.updateShareState();
          }
          break;
        case "social:frameworker-error":
          if (Social.provider && Social.provider.origin == data) {
            SocialSidebar.setSidebarErrorMessage("frameworker-error");
          }
          break;

        case "nsPref:changed":
          if (data == "social.sidebar.open") {
            SocialSidebar.update();
          }
          break;
      }
    } catch (e) {
      Components.utils.reportError(e + "\n" + e.stack);
      throw e;
    }
  },

  nonBrowserWindowInit: function SocialUI_nonBrowserInit() {
    // Disable the social menu item in non-browser windows
    document.getElementById("menu_socialAmbientMenu").hidden = true;
  },

  // Miscellaneous helpers
  showProfile: function SocialUI_showProfile() {
    if (Social.haveLoggedInUser())
      openUILinkIn(Social.provider.profile.profileURL, "tab");
    else {
      // XXX Bug 789585 will implement an API for provider-specified login pages.
      openUILinkIn(Social.provider.origin, "tab");
    }
  },

  _updateActiveUI: function SocialUI_updateActiveUI() {
    let broadcaster = document.getElementById("socialActiveBroadcaster");
    broadcaster.hidden = !Social.active;

    if (!Social.provider)
      return;

    let toggleCommand = document.getElementById("Social:Toggle");
    // We only need to update the command itself - all our menu items use it.
    let label = gNavigatorBundle.getFormattedString(Social.provider.enabled ?
                                                      "social.turnOff.label" :
                                                      "social.turnOn.label",
                                                    [Social.provider.name]);
    let accesskey = gNavigatorBundle.getString(Social.provider.enabled ?
                                                 "social.turnOff.accesskey" :
                                                 "social.turnOn.accesskey");
    toggleCommand.setAttribute("label", label);
    toggleCommand.setAttribute("accesskey", accesskey);
    toggleCommand.setAttribute("hidden", Social.active ? "false" : "true");
  },

  _updateMenuItems: function () {
    if (!Social.provider)
      return;

    // The View->Sidebar and Menubar->Tools menu.
    for (let id of ["menu_socialSidebar", "menu_socialAmbientMenu"])
      document.getElementById(id).setAttribute("label", Social.provider.name);
  },

  // This handles "ActivateSocialFeature" events fired against content documents
  // in this window.
  _activationEventHandler: function SocialUI_activationHandler(e) {
    let targetDoc = e.target;

    // Event must be fired against the document
    if (!(targetDoc instanceof HTMLDocument))
      return;

    // Ignore events fired in background tabs
    if (targetDoc.defaultView.top != content)
      return;

    // Check that the associated document's origin is in our whitelist
    let providerOrigin = targetDoc.nodePrincipal.origin;
    let whitelist = Services.prefs.getCharPref("social.activation.whitelist");
    if (whitelist.split(",").indexOf(providerOrigin) == -1)
      return;

    // If the last event was received < 1s ago, ignore this one
    let now = Date.now();
    if (now - Social.lastEventReceived < 1000)
      return;
    Social.lastEventReceived = now;

    // Keep track of the old provider in case of undo
    let oldOrigin = Social.provider ? Social.provider.origin : "";

    // Enable the social functionality, and indicate that it was activated
    let provider = Social.activateFromOrigin(providerOrigin);

    // Provider to activate may not have been found
    if (!provider)
      return;

    // Show a warning, allow undoing the activation
    let description = document.getElementById("social-activation-message");
    let brandShortName = document.getElementById("bundle_brand").getString("brandShortName");
    let message = gNavigatorBundle.getFormattedString("social.activated.description",
                                                      [provider.name, brandShortName]);
    description.value = message;

    let notificationPanel = SocialUI.notificationPanel;
    // Set the origin being activated and the previously active one, to allow undo
    notificationPanel.setAttribute("origin", provider.origin);
    notificationPanel.setAttribute("oldorigin", oldOrigin);

    // Show the panel
    notificationPanel.hidden = false;
    setTimeout(function () {
      notificationPanel.openPopup(SocialToolbar.button, "bottomcenter topright");
    }, 0);
  },

  undoActivation: function SocialUI_undoActivation() {
    let origin = this.notificationPanel.getAttribute("origin");
    let oldOrigin = this.notificationPanel.getAttribute("oldorigin");
    Social.deactivateFromOrigin(origin, oldOrigin);
    this.notificationPanel.hidePopup();
  },

  get notificationPanel() {
    return document.getElementById("socialActivatedNotification");
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
      containerParent.hidePopup();
    }
  },

  disableWithConfirmation: function SocialUI_disableWithConfirmation() {
    let brandShortName = document.getElementById("bundle_brand").getString("brandShortName");
    let dialogTitle = gNavigatorBundle.getFormattedString("social.remove.confirmationOK",
                                                          [Social.provider.name]);
    let text = gNavigatorBundle.getFormattedString("social.remove.confirmationLabel",
                                                   [Social.provider.name, brandShortName]);
    let okButtonText = dialogTitle;

    let ps = Services.prompt;
    let flags = ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_0 +
                ps.BUTTON_TITLE_CANCEL * ps.BUTTON_POS_1 +
                ps.BUTTON_POS_0_DEFAULT;

    let confirmationIndex = ps.confirmEx(null, dialogTitle, text, flags,
                                         okButtonText, null, null, null, {});
    if (confirmationIndex == 0) {
      Social.deactivateFromOrigin(Social.provider.origin);
    }
  }
}

let SocialChatBar = {
  get chatbar() {
    return document.getElementById("pinnedchats");
  },
  // Whether the chatbar is available for this window.  Note that in full-screen
  // mode chats are available, but not shown.
  get isAvailable() {
    if (!Social.haveLoggedInUser())
      return false;
    let docElem = document.documentElement;
    let chromeless = docElem.getAttribute("chromehidden").indexOf("extrachrome") >= 0;
    return Social.uiVisible && !chromeless;
  },
  // Does this chatbar have any chats (whether minimized, collapsed or normal)
  get hasChats() {
    return !!this.chatbar.firstElementChild;
  },
  openChat: function(aProvider, aURL, aCallback, aMode) {
    if (this.isAvailable)
      this.chatbar.openChat(aProvider, aURL, aCallback, aMode);
  },
  update: function() {
    let command = document.getElementById("Social:FocusChat");
    if (!this.isAvailable) {
      this.chatbar.removeAll();
      command.hidden = true;
    } else {
      this.chatbar.hidden = command.hidden = document.mozFullScreen;
    }
    command.setAttribute("disabled", command.hidden ? "true" : "false");
  },
  focus: function SocialChatBar_focus() {
    this.chatbar.focus();
  }
}

function sizeSocialPanelToContent(panel, iframe) {
  // FIXME: bug 764787: Maybe we can use nsIDOMWindowUtils.getRootBounds() here?
  let doc = iframe.contentDocument;
  if (!doc || !doc.body) {
    return;
  }
  let body = doc.body;
  // offsetHeight/Width don't include margins, so account for that.
  let cs = doc.defaultView.getComputedStyle(body);
  let computedHeight = parseInt(cs.marginTop) + body.offsetHeight + parseInt(cs.marginBottom);
  let height = Math.max(computedHeight, PANEL_MIN_HEIGHT);
  let computedWidth = parseInt(cs.marginLeft) + body.offsetWidth + parseInt(cs.marginRight);
  let width = Math.max(computedWidth, PANEL_MIN_WIDTH);
  let wDiff = width - iframe.getBoundingClientRect().width;
  // A panel resize will move the right margin - if that is where the anchor
  // arrow is, the arrow will be mis-aligned from the anchor.  So we move the
  // popup to compensate for that.  See bug 799014.
  if (wDiff !== 0 && panel.getAttribute("side") == "right") {
    let box = panel.boxObject;
    panel.moveTo(box.screenX - wDiff, box.screenY);
  }
  iframe.style.height = height + "px";
  iframe.style.width = width + "px";
}

function DynamicResizeWatcher() {
  this._mutationObserver = null;
}

DynamicResizeWatcher.prototype = {
  start: function DynamicResizeWatcher_start(panel, iframe) {
    this.stop(); // just in case...
    let doc = iframe.contentDocument;
    this._mutationObserver = new iframe.contentWindow.MutationObserver(function(mutations) {
      sizeSocialPanelToContent(panel, iframe);
    });
    // Observe anything that causes the size to change.
    let config = {attributes: true, characterData: true, childList: true, subtree: true};
    this._mutationObserver.observe(doc, config);
    // and since this may be setup after the load event has fired we do an
    // initial resize now.
    sizeSocialPanelToContent(panel, iframe);
  },
  stop: function DynamicResizeWatcher_stop() {
    if (this._mutationObserver) {
      try {
        this._mutationObserver.disconnect();
      } catch (ex) {
        // may get "TypeError: can't access dead object" which seems strange,
        // but doesn't seem to indicate a real problem, so ignore it...
      }
      this._mutationObserver = null;
    }
  }
}

let SocialFlyout = {
  get panel() {
    return document.getElementById("social-flyout-panel");
  },

  dispatchPanelEvent: function(name) {
    let doc = this.panel.firstChild.contentDocument;
    let evt = doc.createEvent("CustomEvent");
    evt.initCustomEvent(name, true, true, {});
    doc.documentElement.dispatchEvent(evt);
  },

  _createFrame: function() {
    let panel = this.panel;
    if (!Social.provider || panel.firstChild)
      return;
    // create and initialize the panel for this window
    let iframe = document.createElement("iframe");
    iframe.setAttribute("type", "content");
    iframe.setAttribute("class", "social-panel-frame");
    iframe.setAttribute("flex", "1");
    iframe.setAttribute("origin", Social.provider.origin);
    panel.appendChild(iframe);
  },

  setUpProgressListener: function SF_setUpProgressListener() {
    if (!this._progressListenerSet) {
      this._progressListenerSet = true;
      // Force a layout flush by calling .clientTop so
      // that the docShell of this frame is created
      this.panel.firstChild.clientTop;
      this.panel.firstChild.docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                    .getInterface(Ci.nsIWebProgress)
                                    .addProgressListener(new SocialErrorListener("flyout"),
                                                         Ci.nsIWebProgress.NOTIFY_STATE_REQUEST |
                                                         Ci.nsIWebProgress.NOTIFY_LOCATION);
    }
  },

  setFlyoutErrorMessage: function SF_setFlyoutErrorMessage() {
    let iframe = this.panel.firstChild;
    if (!iframe)
      return;

    iframe.removeAttribute("src");
    iframe.webNavigation.loadURI("about:socialerror?mode=compactInfo", null, null, null, null);
    sizeSocialPanelToContent(this.panel, iframe);
  },

  unload: function() {
    let panel = this.panel;
    panel.hidePopup();
    if (!panel.firstChild)
      return
    panel.removeChild(panel.firstChild);
  },

  onShown: function(aEvent) {
    let panel = this.panel;
    let iframe = panel.firstChild;
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
    this.panel.firstChild.docShell.isActive = false;
    this.dispatchPanelEvent("socialFrameHide");
  },

  open: function(aURL, yOffset, aCallback) {
    // Hide any other social panels that may be open.
    document.getElementById("social-notification-panel").hidePopup();

    if (!Social.provider)
      return;
    let panel = this.panel;
    if (!panel.firstChild)
      this._createFrame();
    panel.hidden = false;
    let iframe = panel.firstChild;

    let src = iframe.getAttribute("src");
    if (src != aURL) {
      iframe.addEventListener("load", function documentLoaded() {
        iframe.removeEventListener("load", documentLoaded, true);
        if (aCallback) {
          try {
            aCallback(iframe.contentWindow);
          } catch(e) {
            Cu.reportError(e);
          }
        }
      }, true);
      iframe.setAttribute("src", aURL);
    }
    else if (aCallback) {
      try {
        aCallback(iframe.contentWindow);
      } catch(e) {
        Cu.reportError(e);
      }
    }

    sizeSocialPanelToContent(panel, iframe);
    let anchor = document.getElementById("social-sidebar-browser");
    if (panel.state == "open") {
      // this is painful - there is no way to say "move to a new anchor offset",
      // only "move to new screen pos".  So we remember the last yOffset,
      // calculate the adjustment needed to the new yOffset, then calc the
      // screen Y position.
      let yAdjust = yOffset - this.yOffset;
      let box = panel.boxObject;
      panel.moveTo(box.screenX, box.screenY + yAdjust);
    } else {
      panel.openPopup(anchor, "start_before", 0, yOffset, false, false);
      this.setUpProgressListener();
    }
    this.yOffset = yOffset;
  }
}

let SocialShareButton = {
  // Called once, after window load, when the Social.provider object is initialized
  init: function SSB_init() {
    this.updateProvider();
  },

  // Called when the Social.provider changes
  updateProvider: function () {
    this.updateButtonHiddenState();
    if (!Social.provider)
      return;
    this.updateProfileInfo();
  },

  // Called when the provider's profile info changes (or when the provider
  // changes, via updateProvider)
  updateProfileInfo: function SSB_updateProfileInfo() {
    let profileRow = document.getElementById("unsharePopupHeader");
    let profile = Social.provider.profile;
    if (profile && profile.displayName) {
      profileRow.hidden = false;
      let portrait = document.getElementById("socialUserPortrait");
      portrait.setAttribute("src", profile.portrait || "chrome://global/skin/icons/information-32.png");
      let displayName = document.getElementById("socialUserDisplayName");
      displayName.setAttribute("label", profile.displayName);
    } else {
      profileRow.hidden = true;
      this.updateButtonHiddenState();
    }
  },

  get shareButton() {
    return document.getElementById("share-button");
  },
  get unsharePopup() {
    return document.getElementById("unsharePopup");
  },

  dismissUnsharePopup: function SSB_dismissUnsharePopup() {
    this.unsharePopup.hidePopup();
  },

  canSharePage: function SSB_canSharePage(aURI) {
    // We only allow sharing of http or https
    return aURI && (aURI.schemeIs('http') || aURI.schemeIs('https'));
  },

  updateButtonHiddenState: function SSB_updateButtonHiddenState() {
    let shareButton = this.shareButton;
    if (shareButton)
      shareButton.hidden = !Social.uiVisible || Social.provider.recommendInfo == null ||
                           !Social.haveLoggedInUser() ||
                           !this.canSharePage(gBrowser.currentURI);

    // also update the relevent command's disabled state so the keyboard
    // shortcut only works when available.
    let cmd = document.getElementById("Social:SharePage");
    cmd.setAttribute("disabled", shareButton.hidden ? "true" : "false");
  },

  onClick: function SSB_onClick(aEvent) {
    if (aEvent.button != 0)
      return;

    // Don't bubble to the textbox, to avoid unwanted selection of the address.
    aEvent.stopPropagation();

    this.sharePage();
  },

  panelShown: function SSB_panelShown(aEvent) {
    function updateElement(id, attrs) {
      let el = document.getElementById(id);
      Object.keys(attrs).forEach(function(attr) {
        el.setAttribute(attr, attrs[attr]);
      });
    }
    let continueSharingButton = document.getElementById("unsharePopupContinueSharingButton");
    continueSharingButton.focus();
    let recommendInfo = Social.provider.recommendInfo;
    updateElement("unsharePopupContinueSharingButton",
                  {label: recommendInfo.messages.unshareCancelLabel,
                   accesskey: recommendInfo.messages.unshareCancelAccessKey});
    updateElement("unsharePopupStopSharingButton",
                  {label: recommendInfo.messages.unshareConfirmLabel,
                  accesskey: recommendInfo.messages.unshareConfirmAccessKey});
    updateElement("socialUserPortrait",
                  {"aria-label": recommendInfo.messages.portraitLabel});
    updateElement("socialUserRecommendedText",
                  {value: recommendInfo.messages.unshareLabel});
  },

  sharePage: function SSB_sharePage() {
    this.unsharePopup.hidden = false;

    let uri = gBrowser.currentURI;
    if (!Social.isPageShared(uri)) {
      Social.sharePage(uri);
      this.updateShareState();
    } else {
      this.unsharePopup.openPopup(this.shareButton, "bottomcenter topright");
    }
  },

  unsharePage: function SSB_unsharePage() {
    Social.unsharePage(gBrowser.currentURI);
    this.updateShareState();
    this.dismissUnsharePopup();
  },

  updateShareState: function SSB_updateShareState() {
    this.updateButtonHiddenState();

    let shareButton = this.shareButton;
    let currentPageShared = shareButton && !shareButton.hidden && Social.isPageShared(gBrowser.currentURI);

    let recommendInfo = Social.provider ? Social.provider.recommendInfo : null;
    // Provide a11y-friendly notification of share.
    let status = document.getElementById("share-button-status");
    if (status) {
      // XXX - this should also be capable of reflecting that the page was
      // unshared (ie, it needs to manage three-states: (1) nothing done, (2)
      // shared, (3) shared then unshared)
      // Note that we *do* have an appropriate string from the provider for
      // this (recommendInfo.messages.unsharedLabel) but currently lack a way of
      // tracking this state)
      let statusString = currentPageShared && recommendInfo ?
                           recommendInfo.messages.sharedLabel : "";
      status.setAttribute("value", statusString);
    }

    // Update the share button, if present
    if (!shareButton || shareButton.hidden)
      return;

    let imageURL;
    if (currentPageShared) {
      shareButton.setAttribute("shared", "true");
      shareButton.setAttribute("tooltiptext", recommendInfo.messages.unshareTooltip);
      imageURL = recommendInfo.images.unshare;
    } else {
      shareButton.removeAttribute("shared");
      shareButton.setAttribute("tooltiptext", recommendInfo.messages.shareTooltip);
      imageURL = recommendInfo.images.share;
    }
    shareButton.src = imageURL;
  }
};

var SocialMenu = {
  populate: function SocialMenu_populate() {
    let submenu = document.getElementById("menu_social-statusarea-popup");
    let ambientMenuItems = submenu.getElementsByClassName("ambient-menuitem");
    while (ambientMenuItems.length)
      submenu.removeChild(ambientMenuItems.item(0));

    let separator = document.getElementById("socialAmbientMenuSeparator");
    separator.hidden = true;
    if (!Social.uiVisible)
      return;

    let provider = Social.provider;
    let iconNames = Object.keys(provider.ambientNotificationIcons);
    for (let name of iconNames) {
      let icon = provider.ambientNotificationIcons[name];
      if (!icon.label || !icon.menuURL)
        continue;
      separator.hidden = false;
      let menuitem = document.createElement("menuitem");
      menuitem.setAttribute("label", icon.label);
      menuitem.classList.add("ambient-menuitem");
      menuitem.addEventListener("command", function() {
        openUILinkIn(icon.menuURL, "tab");
      }, false);
      submenu.insertBefore(menuitem, separator);
    }
  }
};

// XXX Need to audit that this is being initialized correctly
var SocialToolbar = {
  // Called once, after window load, when the Social.provider object is
  // initialized.
  init: function SocialToolbar_init() {
    let brandShortName = document.getElementById("bundle_brand").getString("brandShortName");
    let label = gNavigatorBundle.getFormattedString("social.remove.label",
                                                    [brandShortName]);
    let accesskey = gNavigatorBundle.getString("social.remove.accesskey");

    let removeCommand = document.getElementById("Social:Remove");
    removeCommand.setAttribute("label", label);
    removeCommand.setAttribute("accesskey", accesskey);

    this.updateProvider();
    this._dynamicResizer = new DynamicResizeWatcher();
  },

  // Called when the Social.provider changes
  updateProvider: function () {
    if (!Social.provider)
      return;
    this.button.style.listStyleImage = "url(" + Social.provider.iconURL + ")";
    this.button.setAttribute("label", Social.provider.name);
    this.button.setAttribute("tooltiptext", Social.provider.name);
    this.updateButton();
    this.updateProfile();
    this.populateProviderMenus();
  },

  get button() {
    return document.getElementById("social-provider-button");
  },

  // Note: this doesn't actually handle hiding the toolbar button,
  // socialActiveBroadcaster is responsible for that.
  updateButtonHiddenState: function SocialToolbar_updateButtonHiddenState() {
    let tbi = document.getElementById("social-toolbar-item");
    let socialEnabled = Social.uiVisible;
    for (let className of ["social-statusarea-separator", "social-statusarea-user"]) {
      for (let element of document.getElementsByClassName(className))
        element.hidden = !socialEnabled;
    }
    let toggleNotificationsCommand = document.getElementById("Social:ToggleNotifications");
    toggleNotificationsCommand.setAttribute("hidden", !socialEnabled);

    if (!Social.haveLoggedInUser() || !socialEnabled) {
      let parent = document.getElementById("social-notification-panel");
      while (parent.hasChildNodes()) {
        let frame = parent.firstChild;
        SharedFrame.forgetGroup(frame.id);
        parent.removeChild(frame);
      }

      while (tbi.lastChild != tbi.firstChild)
        tbi.removeChild(tbi.lastChild);
    }
  },

  updateProfile: function SocialToolbar_updateProfile() {
    // Profile may not have been initialized yet, since it depends on a worker
    // response. In that case we'll be called again when it's available, via
    // social:profile-changed
    let profile = Social.provider.profile || {};
    let userPortrait = profile.portrait || "chrome://global/skin/icons/information-32.png";

    let userDetailsBroadcaster = document.getElementById("socialBroadcaster_userDetails");
    let loggedInStatusValue = profile.userName ||
                              userDetailsBroadcaster.getAttribute("notLoggedInLabel");

    // "image" and "label" are used by Mac's native menus that do not render the menuitem's children
    // elements. "src" and "value" are used by the image/label children on the other platforms.
    userDetailsBroadcaster.setAttribute("src", userPortrait);
    userDetailsBroadcaster.setAttribute("image", userPortrait);

    userDetailsBroadcaster.setAttribute("value", loggedInStatusValue);
    userDetailsBroadcaster.setAttribute("label", loggedInStatusValue);
  },

  // XXX doesn't this need to be called for profile changes, given its use of provider.profile?
  updateButton: function SocialToolbar_updateButton() {
    this.updateButtonHiddenState();
    let provider = Social.provider;
    let icons = provider.ambientNotificationIcons;
    let iconNames = Object.keys(icons);
    let panel = document.getElementById("social-notification-panel");
    panel.hidden = false;

    let command = document.getElementById("Social:ToggleNotifications");
    command.setAttribute("checked", Services.prefs.getBoolPref("social.toast-notifications.enabled"));

    const CACHE_PREF_NAME = "social.cached.ambientNotificationIcons";
    // provider.profile == undefined means no response yet from the provider
    // to tell us whether the user is logged in or not.
    if (!provider.enabled ||
        (!Social.haveLoggedInUser() && provider.profile !== undefined)) {
      // Either no enabled provider, or there is a provider and it has
      // responded with a profile and the user isn't loggedin.  The icons
      // etc have already been removed by updateButtonHiddenState, so we want
      // to nuke any cached icons we have and get out of here!
      Services.prefs.clearUserPref(CACHE_PREF_NAME);
      return;
    }
    if (Social.provider.profile === undefined) {
      // provider has not told us about the login state yet - see if we have
      // a cached version for this provider.
      let cached;
      try {
        cached = JSON.parse(Services.prefs.getComplexValue(CACHE_PREF_NAME,
                                                           Ci.nsISupportsString).data);
      } catch (ex) {}
      if (cached && cached.provider == Social.provider.origin && cached.data) {
        icons = cached.data;
        iconNames = Object.keys(icons);
        // delete the counter data as it is almost certainly stale.
        for each(let name in iconNames) {
          icons[name].counter = '';
        }
      }
    } else {
      // We have a logged in user - save the current set of icons back to the
      // "cache" so we can use them next startup.
      let str = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
      str.data = JSON.stringify({provider: Social.provider.origin, data: icons});
      Services.prefs.setComplexValue(CACHE_PREF_NAME,
                                     Ci.nsISupportsString,
                                     str);
    }

    let toolbarButtons = document.createDocumentFragment();

    let createdFrames = [];

    for each(let name in iconNames) {
      let icon = icons[name];

      let notificationFrameId = "social-status-" + icon.name;
      let notificationFrame = document.getElementById(notificationFrameId);

      if (!notificationFrame) {
        notificationFrame = SharedFrame.createFrame(
          notificationFrameId, /* frame name */
          panel, /* parent */
          {
            "type": "content",
            "mozbrowser": "true",
            "class": "social-panel-frame",
            "id": notificationFrameId,

            // work around bug 793057 - by making the panel roughly the final size
            // we are more likely to have the anchor in the correct position.
            "style": "width: " + PANEL_MIN_WIDTH + "px;",

            "origin": provider.origin,
            "src": icon.contentPanel
          }
        );

        createdFrames.push(notificationFrame);
      } else {
        notificationFrame.setAttribute("origin", provider.origin);
        SharedFrame.updateURL(notificationFrameId, icon.contentPanel);
      }

      let toolbarButtonContainerId = "social-notification-container-" + icon.name;
      let toolbarButtonId = "social-notification-icon-" + icon.name;
      let toolbarButtonContainer = document.getElementById(toolbarButtonContainerId);
      let toolbarButton = document.getElementById(toolbarButtonId);
      if (!toolbarButtonContainer) {
        // The container is used to fix an issue with position:absolute on
        // generated content not being constrained to the bounding box of a
        // parent toolbarbutton that has position:relative.
        toolbarButtonContainer = document.createElement("toolbaritem");
        toolbarButtonContainer.classList.add("social-notification-container");
        toolbarButtonContainer.setAttribute("id", toolbarButtonContainerId);

        toolbarButton = document.createElement("toolbarbutton");
        toolbarButton.classList.add("toolbarbutton-1");
        toolbarButton.setAttribute("id", toolbarButtonId);
        toolbarButton.setAttribute("notificationFrameId", notificationFrameId);
        toolbarButton.addEventListener("mousedown", function (event) {
          if (event.button == 0)
            SocialToolbar.showAmbientPopup(toolbarButton);
        });

        toolbarButtonContainer.appendChild(toolbarButton);
        toolbarButtons.appendChild(toolbarButtonContainer);
      }

      toolbarButton.style.listStyleImage = "url(" + icon.iconURL + ")";
      toolbarButton.setAttribute("label", icon.label);
      toolbarButton.setAttribute("tooltiptext", icon.label);

      let badge = icon.counter || "";
      if (toolbarButton.getAttribute("badge") != badge)
        toolbarButton.setAttribute("badge", badge);
    }
    let socialToolbarItem = document.getElementById("social-toolbar-item");
    socialToolbarItem.appendChild(toolbarButtons);

    for (let frame of createdFrames) {
      if (frame.docShell) {
        frame.docShell.isActive = false;
        frame.docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIWebProgress)
                      .addProgressListener(new SocialErrorListener("notification-panel"),
                                           Ci.nsIWebProgress.NOTIFY_STATE_REQUEST |
                                           Ci.nsIWebProgress.NOTIFY_LOCATION);
      }
    }
  },

  showAmbientPopup: function SocialToolbar_showAmbientPopup(aToolbarButton) {
    // Hide any other social panels that may be open.
    SocialFlyout.panel.hidePopup();

    let panel = document.getElementById("social-notification-panel");
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

    let dynamicResizer = this._dynamicResizer;
    panel.addEventListener("popuphidden", function onpopuphiding() {
      panel.removeEventListener("popuphidden", onpopuphiding);
      aToolbarButton.removeAttribute("open");
      aToolbarButton.parentNode.removeAttribute("open");
      dynamicResizer.stop();
      notificationFrame.docShell.isActive = false;
      dispatchPanelEvent("socialFrameHide");
    });

    panel.addEventListener("popupshown", function onpopupshown() {
      panel.removeEventListener("popupshown", onpopupshown);
      // This attribute is needed on both the button and the
      // containing toolbaritem since the buttons on OS X have
      // moz-appearance:none, while their container gets
      // moz-appearance:toolbarbutton due to the way that toolbar buttons
      // get combined on OS X.
      aToolbarButton.setAttribute("open", "true");
      aToolbarButton.parentNode.setAttribute("open", "true");
      notificationFrame.docShell.isActive = true;
      notificationFrame.docShell.isAppTab = true;
      if (notificationFrame.contentDocument.readyState == "complete" && wasAlive) {
        dynamicResizer.start(panel, notificationFrame);
        dispatchPanelEvent("socialFrameShow");
      } else {
        // first time load, wait for load and dispatch after load
        notificationFrame.addEventListener("load", function panelBrowserOnload(e) {
          notificationFrame.removeEventListener("load", panelBrowserOnload, true);
          dynamicResizer.start(panel, notificationFrame);
          setTimeout(function() {
            dispatchPanelEvent("socialFrameShow");
          }, 0);
        }, true);
      }
    });

    let navBar = document.getElementById("nav-bar");
    let anchor = navBar.getAttribute("mode") == "text" ?
                   document.getAnonymousElementByAttribute(aToolbarButton, "class", "toolbarbutton-text") :
                   document.getAnonymousElementByAttribute(aToolbarButton, "class", "toolbarbutton-icon");
    panel.openPopup(anchor, "bottomcenter topright", 0, 0, false, false);
  },

  setPanelErrorMessage: function SocialToolbar_setPanelErrorMessage(aNotificationFrame) {
    if (!aNotificationFrame)
      return;

    let src = aNotificationFrame.getAttribute("src");
    aNotificationFrame.removeAttribute("src");
    aNotificationFrame.webNavigation.loadURI("about:socialerror?mode=tryAgainOnly&url=" +
                                             encodeURIComponent(src), null, null, null, null);
    let panel = aNotificationFrame.parentNode;
    sizeSocialPanelToContent(panel, aNotificationFrame);
  },

  populateProviderMenus: function SocialToolbar_renderProviderMenus() {
    let providerMenuSeps = document.getElementsByClassName("social-provider-menu");
    let activeProviders = [p for (p of Social.providers) if (p.active)];
    for (let providerMenuSep of providerMenuSeps)
      this._populateProviderMenu(providerMenuSep, activeProviders);
  },

  _populateProviderMenu: function SocialToolbar_renderProviderMenu(providerMenuSep, providers) {
    let menu = providerMenuSep.parentNode;
    // selectable providers are inserted before the provider-menu seperator,
    // remove any menuitems in that area
    while (providerMenuSep.previousSibling.nodeName == "menuitem") {
      menu.removeChild(providerMenuSep.previousSibling);
    }
    // only show a selection if there is more than one
    if (!Social.enabled || providers.length < 2) {
      providerMenuSep.hidden = true;
      return;
    }
    for (let provider of providers) {
      let menuitem = document.createElement("menuitem");
      menuitem.className = "menuitem-iconic social-provider-menuitem";
      menuitem.setAttribute("image", provider.iconURL);
      menuitem.setAttribute("label", provider.name);
      menuitem.setAttribute("origin", provider.origin);
      if (provider == Social.provider) {
        menuitem.setAttribute("checked", "true");
      } else {
        menuitem.setAttribute("oncommand", "Social.setProviderByOrigin(this.getAttribute('origin'));");
      }
      menu.insertBefore(menuitem, providerMenuSep);
    }
    providerMenuSep.hidden = false;
  }
}

var SocialSidebar = {
  // Called once, after window load, when the Social.provider object is initialized
  init: function SocialSidebar_init() {
    let sbrowser = document.getElementById("social-sidebar-browser");
    this.errorListener = new SocialErrorListener("sidebar");
    this.configureSidebarDocShell(sbrowser.docShell);
    this.update();
  },

  configureSidebarDocShell: function SocialSidebar_configureDocShell(aDocShell) {
    // setting isAppTab causes clicks on untargeted links to open new tabs
    aDocShell.isAppTab = true;
    aDocShell.QueryInterface(Ci.nsIWebProgress)
             .addProgressListener(SocialSidebar.errorListener,
                                  Ci.nsIWebProgress.NOTIFY_STATE_REQUEST |
                                  Ci.nsIWebProgress.NOTIFY_LOCATION);
  },

  // Whether the sidebar can be shown for this window.
  get canShow() {
    return Social.uiVisible && Social.provider.sidebarURL && !this.chromeless;
  },

  // Whether this is a "chromeless window" (e.g. popup window). We don't show
  // the sidebar in these windows.
  get chromeless() {
    let docElem = document.documentElement;
    return docElem.getAttribute('disablechrome') ||
           docElem.getAttribute('chromehidden').contains("toolbar");
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

  update: function SocialSidebar_update() {
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
        SocialSidebar.setSidebarErrorMessage("frameworker-error");
        return;
      }

      // Make sure the right sidebar URL is loaded
      if (sbrowser.getAttribute("src") != Social.provider.sidebarURL) {
        sbrowser.setAttribute("src", Social.provider.sidebarURL);
        sbrowser.addEventListener("load", SocialSidebar._loadListener, true);
      } else {
        this.setSidebarVisibilityState(true);
      }
    }
  },

  _loadListener: function SocialSidebar_loadListener() {
    let sbrowser = document.getElementById("social-sidebar-browser");
    sbrowser.removeEventListener("load", SocialSidebar._loadListener, true);
    SocialSidebar.setSidebarVisibilityState(true);
  },

  unloadSidebar: function SocialSidebar_unloadSidebar() {
    let sbrowser = document.getElementById("social-sidebar-browser");
    if (!sbrowser.hasAttribute("origin"))
      return;

    sbrowser.stop();
    sbrowser.removeAttribute("origin");
    sbrowser.setAttribute("src", "about:blank");
    SocialFlyout.unload();
  },

  _unloadTimeoutId: 0,

  setSidebarErrorMessage: function(aType) {
    let sbrowser = document.getElementById("social-sidebar-browser");
    switch (aType) {
      case "sidebar-error":
        let url = encodeURIComponent(Social.provider.sidebarURL);
        sbrowser.loadURI("about:socialerror?mode=tryAgain&url=" + url, null, null);
        break;

      case "frameworker-error":
        sbrowser.setAttribute("src", "about:socialerror?mode=workerFailure");
        break;
    }
  }
}

// Error handling class used to listen for network errors in the social frames
// and replace them with a social-specific error page
function SocialErrorListener(aType) {
  this.type = aType;
}

SocialErrorListener.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsISupports]),

  onStateChange: function SPL_onStateChange(aWebProgress, aRequest, aState, aStatus) {
    let failure = false;
    if ((aState & Ci.nsIWebProgressListener.STATE_STOP)) {
      if (aRequest instanceof Ci.nsIHttpChannel) {
        try {
          // Change the frame to an error page on 4xx (client errors)
          // and 5xx (server errors)
          failure = aRequest.responseStatus >= 400 &&
                    aRequest.responseStatus < 600;
        } catch (e) {}
      }
    }

    // Calling cancel() will raise some OnStateChange notifications by itself,
    // so avoid doing that more than once
    if (failure && aStatus != Components.results.NS_BINDING_ABORTED) {
      aRequest.cancel(Components.results.NS_BINDING_ABORTED);
      this.setErrorMessage(aWebProgress);
    }
  },

  onLocationChange: function SPL_onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    let failure = aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE;
    if (failure && Social.provider.errorState != "frameworker-error") {
      aRequest.cancel(Components.results.NS_BINDING_ABORTED);
      window.setTimeout(function(self) {
        self.setErrorMessage(aWebProgress);
      }, 0, this);
    }
  },

  onProgressChange: function SPL_onProgressChange() {},
  onStatusChange: function SPL_onStatusChange() {},
  onSecurityChange: function SPL_onSecurityChange() {},

  setErrorMessage: function(aWebProgress) {
    switch (this.type) {
      case "flyout":
        SocialFlyout.setFlyoutErrorMessage();
        break;

      case "sidebar":
        // a frameworker error "trumps" a sidebar error.
        let reason = Social.provider.errorState || "sidebar-error";
        SocialSidebar.setSidebarErrorMessage(reason);
        break;

      case "notification-panel":
        let frame = aWebProgress.QueryInterface(Ci.nsIDocShell)
                                .chromeEventHandler;
        SocialToolbar.setPanelErrorMessage(frame);
        break;
    }
  }
};
