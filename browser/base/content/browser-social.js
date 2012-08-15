// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let SocialUI = {
  // Called on delayed startup to initialize UI
  init: function SocialUI_init() {
    Services.obs.addObserver(this, "social:pref-changed", false);
    Services.obs.addObserver(this, "social:ambient-notification-changed", false);
    Services.obs.addObserver(this, "social:profile-changed", false);

    Services.prefs.addObserver("social.sidebar.open", this, false);

    gBrowser.addEventListener("ActivateSocialFeature", this._activationEventHandler, true, true);

    Social.init(this._providerReady.bind(this));
  },

  // Called on window unload
  uninit: function SocialUI_uninit() {
    Services.obs.removeObserver(this, "social:pref-changed");
    Services.obs.removeObserver(this, "social:ambient-notification-changed");
    Services.obs.removeObserver(this, "social:profile-changed");

    Services.prefs.removeObserver("social.sidebar.open", this);
  },

  showProfile: function SocialUI_showProfile() {
    if (Social.provider)
      openUILink(Social.provider.profile.profileURL);
  },

  observe: function SocialUI_observe(subject, topic, data) {
    switch (topic) {
      case "social:pref-changed":
        this.updateToggleCommand();
        SocialShareButton.updateButtonHiddenState();
        SocialToolbar.updateButtonHiddenState();
        SocialSidebar.updateSidebar();
        break;
      case "social:ambient-notification-changed":
        SocialToolbar.updateButton();
        break;
      case "social:profile-changed":
        SocialToolbar.updateProfile();
        SocialShareButton.updateProfileInfo();
        break;
      case "nsPref:changed":
        SocialSidebar.updateSidebar();
    }
  },

  get toggleCommand() {
    return document.getElementById("Social:Toggle");
  },

  // Called once Social.jsm's provider has been set
  _providerReady: function SocialUI_providerReady() {
    // If we couldn't find a provider, nothing to do here.
    if (!Social.provider)
      return;

    this.updateToggleCommand();

    let toggleCommand = this.toggleCommand;
    let label = gNavigatorBundle.getFormattedString("social.enable.label",
                                                    [Social.provider.name]);
    let accesskey = gNavigatorBundle.getString("social.enable.accesskey");
    toggleCommand.setAttribute("label", label);
    toggleCommand.setAttribute("accesskey", accesskey);

    SocialToolbar.init();
    SocialShareButton.init();
    SocialSidebar.init();
  },

  updateToggleCommand: function SocialUI_updateToggleCommand() {
    let toggleCommand = this.toggleCommand;
    toggleCommand.setAttribute("checked", Social.enabled);

    // FIXME: bug 772808: menu items don't inherit the "hidden" state properly,
    // need to update them manually.
    // This should just be: toggleCommand.hidden = !Social.active;
    for (let id of ["appmenu_socialToggle", "menu_socialToggle"]) {
      let el = document.getElementById(id);
      if (!el)
        continue;

      if (Social.active)
        el.removeAttribute("hidden");
      else
        el.setAttribute("hidden", "true");
    }
  },

  // This handles "ActivateSocialFeature" events fired against content documents
  // in this window.
  _activationEventHandler: function SocialUI_activationHandler(e) {
    // Nothing to do if Social is already active, or we don't have a provider
    // to enable yet.
    if (Social.active || !Social.provider)
      return;

    let targetDoc = e.target;

    // Event must be fired against the document
    if (!(targetDoc instanceof HTMLDocument))
      return;

    // Ignore events fired in background tabs
    if (targetDoc.defaultView.top != content)
      return;

    // Check that the associated document's origin is in our whitelist
    let prePath = targetDoc.documentURIObject.prePath;
    let whitelist = Services.prefs.getCharPref("social.activation.whitelist");
    if (whitelist.split(",").indexOf(prePath) == -1)
      return;

    // If the last event was received < 1s ago, ignore this one
    let now = Date.now();
    if (now - Social.lastEventReceived < 1000)
      return;
    Social.lastEventReceived = now;

    // Enable the social functionality, and indicate that it was activated
    Social.active = true;

    // Show a warning, allow undoing the activation
    let description = document.getElementById("social-activation-message");
    let brandShortName = document.getElementById("bundle_brand").getString("brandShortName");
    let message = gNavigatorBundle.getFormattedString("social.activated.message",
                                                      [Social.provider.name, brandShortName]);
    description.value = message;

    SocialUI.notificationPanel.hidden = false;

    setTimeout(function () {
      SocialUI.notificationPanel.openPopup(SocialToolbar.button, "bottomcenter topright");
    }.bind(this), 0);
  },

  get notificationPanel() {
    return document.getElementById("socialActivatedNotification")
  },

  undoActivation: function SocialUI_undoActivation() {
    Social.active = false;
    this.notificationPanel.hidePopup();
  }
}

let SocialShareButton = {
  // Called once, after window load, when the Social.provider object is initialized
  init: function SSB_init() {
    this.updateButtonHiddenState();
    this.updateProfileInfo();
  },

  updateProfileInfo: function SSB_updateProfileInfo() {
    let profileRow = document.getElementById("editSharePopupHeader");
    let profile = Social.provider.profile;
    if (profile && profile.displayName) {
      profileRow.hidden = false;
      let portrait = document.getElementById("socialUserPortrait");
      portrait.setAttribute("src", profile.portrait || "chrome://browser/skin/social/social.png");
      let displayName = document.getElementById("socialUserDisplayName");
      displayName.setAttribute("label", profile.displayName);
    } else {
      profileRow.hidden = true;
    }
  },

  get shareButton() {
    return document.getElementById("share-button");
  },
  get sharePopup() {
    return document.getElementById("editSharePopup");
  },

  dismissSharePopup: function SSB_dismissSharePopup() {
    this.sharePopup.hidePopup();
  },

  updateButtonHiddenState: function SSB_updateButtonHiddenState() {
    let shareButton = this.shareButton;
    if (shareButton)
      shareButton.hidden = !Social.uiVisible;
  },

  onClick: function SSB_onClick(aEvent) {
    if (aEvent.button != 0)
      return;

    // Don't bubble to the textbox, to avoid unwanted selection of the address.
    aEvent.stopPropagation();

    this.sharePage();
  },

  panelShown: function SSB_panelShown(aEvent) {
    let sharePopupOkButton = document.getElementById("editSharePopupOkButton");
    if (sharePopupOkButton)
      sharePopupOkButton.focus();
  },

  sharePage: function SSB_sharePage() {
    this.sharePopup.hidden = false;

    let uri = gBrowser.currentURI;
    if (!Social.isPageShared(uri)) {
      Social.sharePage(uri);
      this.updateShareState();
    } else {
      this.sharePopup.openPopup(this.shareButton, "bottomcenter topright");
    }
  },

  unsharePage: function SSB_unsharePage() {
    Social.unsharePage(gBrowser.currentURI);
    this.updateShareState();
    this.dismissSharePopup();
  },

  updateShareState: function SSB_updateShareState() {
    let currentPageShared = Social.isPageShared(gBrowser.currentURI);

    // Provide a11y-friendly notification of share.
    let status = document.getElementById("share-button-status");
    if (status) {
      let statusString = currentPageShared ?
                           gNavigatorBundle.getString("social.pageShared.label") : "";
      status.setAttribute("value", statusString);
    }

    // Update the share button, if present
    let shareButton = this.shareButton;
    if (!shareButton)
      return;

    if (currentPageShared) {
      shareButton.setAttribute("shared", "true");
      shareButton.setAttribute("tooltiptext", gNavigatorBundle.getString("social.shareButton.sharedtooltip"));
    } else {
      shareButton.removeAttribute("shared");
      shareButton.setAttribute("tooltiptext", gNavigatorBundle.getString("social.shareButton.tooltip"));
    }
  }
};

var SocialToolbar = {
  // Called once, after window load, when the Social.provider object is initialized
  init: function SocialToolbar_init() {
    document.getElementById("social-provider-image").setAttribute("image", Social.provider.iconURL);

    let notifBrowser = document.getElementById("social-notification-browser");
    notifBrowser.docShell.isAppTab = true;

    let removeItem = document.getElementById("social-remove-menuitem");
    let brandShortName = document.getElementById("bundle_brand").getString("brandShortName");
    let label = gNavigatorBundle.getFormattedString("social.remove.label",
                                                    [brandShortName]);
    let accesskey = gNavigatorBundle.getString("social.remove.accesskey");
    removeItem.setAttribute("label", label);
    removeItem.setAttribute("accesskey", accesskey);

    let statusAreaPopup = document.getElementById("social-statusarea-popup");
    statusAreaPopup.addEventListener("popupshowing", function(e) {
      this.button.setAttribute("open", "true");
    }.bind(this));
    statusAreaPopup.addEventListener("popuphidden", function(e) {
      this.button.removeAttribute("open");
    }.bind(this));

    this.updateButton();
    this.updateProfile();
  },

  get button() {
    return document.getElementById("social-toolbar-button");
  },

  updateButtonHiddenState: function SocialToolbar_updateButtonHiddenState() {
    this.button.hidden = !Social.uiVisible;
  },

  updateProfile: function SocialToolbar_updateProfile() {
    // Profile may not have been initialized yet, since it depends on a worker
    // response. In that case we'll be called again when it's available, via
    // social:profile-changed
    let profile = Social.provider.profile || {};
    let userPortrait = profile.portrait || "chrome://browser/skin/social/social.png";
    document.getElementById("social-statusarea-user-portrait").setAttribute("src", userPortrait);

    let notLoggedInLabel = document.getElementById("social-statusarea-notloggedin");
    let userNameBtn = document.getElementById("social-statusarea-username");
    if (profile.userName) {
      notLoggedInLabel.hidden = true;
      userNameBtn.hidden = false;
      userNameBtn.label = profile.userName;
    } else {
      notLoggedInLabel.hidden = false;
      userNameBtn.hidden = true;
    }
  },

  updateButton: function SocialToolbar_updateButton() {
    this.updateButtonHiddenState();

    let provider = Social.provider;
    // if there are no ambient icons, we collapse them in the following loop
    let iconNames = Object.keys(provider.ambientNotificationIcons);
    let iconBox = document.getElementById("social-status-iconbox");
    for (var i = 0; i < iconBox.childNodes.length; i++) {
      let iconContainer = iconBox.childNodes[i];
      if (i > iconNames.length - 1) {
        iconContainer.collapsed = true;
        continue;
      }

      iconContainer.collapsed = false;
      let icon = provider.ambientNotificationIcons[iconNames[i]];
      let iconImage = iconContainer.firstChild;
      let iconCounter = iconImage.nextSibling;

      iconImage.setAttribute("contentPanel", icon.contentPanel);
      iconImage.setAttribute("src", icon.iconURL);

      if (iconCounter.firstChild)
        iconCounter.removeChild(iconCounter.firstChild);

      if (icon.counter) {
        iconCounter.appendChild(document.createTextNode(icon.counter));
        iconCounter.collapsed = false;
      } else {
        iconCounter.collapsed = true;
      }
    }
  },

  showAmbientPopup: function SocialToolbar_showAmbientPopup(iconContainer) {
    let iconImage = iconContainer.firstChild;
    let panel = document.getElementById("social-notification-panel");
    let notifBrowser = document.getElementById("social-notification-browser");

    panel.hidden = false;

    function sizePanelToContent() {
      // FIXME: bug 764787: Maybe we can use nsIDOMWindowUtils.getRootBounds() here?
      // Need to handle dynamic sizing
      let doc = notifBrowser.contentDocument;
      // "notif" is an implementation detail that we should get rid of
      // eventually
      let body = doc.getElementById("notif") || (doc.body && doc.body.firstChild);
      if (!body)
        return;
      let h = body.scrollHeight > 0 ? body.scrollHeight : 300;
      notifBrowser.style.width = body.scrollWidth + "px";
      notifBrowser.style.height = h + "px";
    }

    notifBrowser.addEventListener("DOMContentLoaded", function onload() {
      notifBrowser.removeEventListener("DOMContentLoaded", onload);
      sizePanelToContent();
    });

    panel.addEventListener("popuphiding", function onpopuphiding() {
      panel.removeEventListener("popuphiding", onpopuphiding);
      // unload the panel
      SocialToolbar.button.removeAttribute("open");
      notifBrowser.setAttribute("src", "about:blank");
    });

    notifBrowser.setAttribute("origin", Social.provider.origin);
    notifBrowser.setAttribute("src", iconImage.getAttribute("contentPanel"));
    this.button.setAttribute("open", "true");
    panel.openPopup(iconImage, "bottomcenter topleft", 0, 0, false, false);
  }
}

var SocialSidebar = {
  // Called once, after window load, when the Social.provider object is initialized
  init: function SocialSidebar_init() {
    let sbrowser = document.getElementById("social-sidebar-browser");
    // setting isAppTab causes clicks on untargeted links to open new tabs
    sbrowser.docShell.isAppTab = true;
  
    this.updateSidebar();
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
           docElem.getAttribute('chromehidden').indexOf("extrachrome") >= 0;
  },

  // Whether the user has toggled the sidebar on (for windows where it can appear)
  get enabled() {
    return Services.prefs.getBoolPref("social.sidebar.open");
  },

  dispatchEvent: function(aType, aDetail) {
    let sbrowser = document.getElementById("social-sidebar-browser");
    let evt = sbrowser.contentDocument.createEvent("CustomEvent");
    evt.initCustomEvent(aType, true, true, aDetail ? aDetail : {});
    sbrowser.contentDocument.documentElement.dispatchEvent(evt);
  },

  updateSidebar: function SocialSidebar_updateSidebar() {
    // Hide the toggle menu item if the sidebar cannot appear
    let command = document.getElementById("Social:ToggleSidebar");
    command.hidden = !this.canShow;

    // Hide the sidebar if it cannot appear, or has been toggled off.
    // Also set the command "checked" state accordingly.
    let hideSidebar = !this.canShow || !this.enabled;
    let broadcaster = document.getElementById("socialSidebarBroadcaster");
    broadcaster.hidden = hideSidebar;
    command.setAttribute("checked", !hideSidebar);

    let sbrowser = document.getElementById("social-sidebar-browser");
    if (hideSidebar) {
      this.dispatchEvent("sidebarhide");
      // If we're disabled, unload the sidebar content
      if (!this.canShow) {
        sbrowser.removeAttribute("origin");
        sbrowser.setAttribute("src", "about:blank");
      }
    } else {
      // Make sure the right sidebar URL is loaded
      if (sbrowser.getAttribute("origin") != Social.provider.origin) {
        sbrowser.setAttribute("origin", Social.provider.origin);
        sbrowser.setAttribute("src", Social.provider.sidebarURL);
        sbrowser.addEventListener("load", function sidebarOnShow() {
          sbrowser.removeEventListener("load", sidebarOnShow);
          // let load finish, then fire our event
          setTimeout(function () {
            SocialSidebar.dispatchEvent("sidebarshow");
          }, 0);
        });
      } else {
        this.dispatchEvent("sidebarshow");
      }
    }
  }
}
