// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The minimum sizes for the auto-resize panel code.
const PANEL_MIN_HEIGHT = 300;
const PANEL_MIN_WIDTH = 330;

let SocialUI = {
  // Called on delayed startup to initialize UI
  init: function SocialUI_init() {
    Services.obs.addObserver(this, "social:pref-changed", false);
    Services.obs.addObserver(this, "social:ambient-notification-changed", false);
    Services.obs.addObserver(this, "social:profile-changed", false);

    Services.prefs.addObserver("social.sidebar.open", this, false);
    Services.prefs.addObserver("social.toast-notifications.enabled", this, false);

    gBrowser.addEventListener("ActivateSocialFeature", this._activationEventHandler, true, true);

    // Called when we enter DOM full-screen mode.
    window.addEventListener("mozfullscreenchange", function () SocialSidebar.updateSidebar());

    Social.init(this._providerReady.bind(this));
  },

  // Called on window unload
  uninit: function SocialUI_uninit() {
    Services.obs.removeObserver(this, "social:pref-changed");
    Services.obs.removeObserver(this, "social:ambient-notification-changed");
    Services.obs.removeObserver(this, "social:profile-changed");

    Services.prefs.removeObserver("social.sidebar.open", this);
    Services.prefs.removeObserver("social.toast-notifications.enabled", this);
  },

  showProfile: function SocialUI_showProfile() {
    if (this.haveLoggedInUser())
      openUILinkIn(Social.provider.profile.profileURL, "tab");
    else {
      // XXX Bug 789585 will implement an API for provider-specified login pages.
      openUILinkIn(Social.provider.origin, "tab");
    }
  },

  observe: function SocialUI_observe(subject, topic, data) {
    switch (topic) {
      case "social:pref-changed":
        // Exceptions here sometimes don't get reported properly, report them
        // manually :(
        try {
          this.updateToggleCommand();
          SocialShareButton.updateButtonHiddenState();
          SocialToolbar.updateButtonHiddenState();
          SocialSidebar.updateSidebar();
          SocialChatBar.update();
          SocialFlyout.unload();
        } catch (e) {
          Components.utils.reportError(e);
          throw e;
        }
        break;
      case "social:ambient-notification-changed":
        SocialToolbar.updateButton();
        break;
      case "social:profile-changed":
        SocialToolbar.updateProfile();
        SocialShareButton.updateProfileInfo();
        SocialChatBar.update();
        break;
      case "nsPref:changed":
        SocialSidebar.updateSidebar();
        SocialToolbar.updateButton();
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
    let brandShortName = document.getElementById("bundle_brand").getString("brandShortName");
    let label = gNavigatorBundle.getFormattedString("social.toggle.label",
                                                    [Social.provider.name,
                                                     brandShortName]);
    let accesskey = gNavigatorBundle.getString("social.toggle.accesskey");
    toggleCommand.setAttribute("label", label);
    toggleCommand.setAttribute("accesskey", accesskey);

    SocialToolbar.init();
    SocialShareButton.init();
    SocialSidebar.init();
  },

  updateToggleCommand: function SocialUI_updateToggleCommand() {
    let toggleCommand = this.toggleCommand;
    toggleCommand.setAttribute("checked", Services.prefs.getBoolPref("social.enabled"));

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
    let message = gNavigatorBundle.getFormattedString("social.activated.description",
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
  },

  haveLoggedInUser: function SocialUI_haveLoggedInUser() {
    return !!(Social.provider && Social.provider.profile && Social.provider.profile.userName);
  }
}

let SocialChatBar = {
  get chatbar() {
    return document.getElementById("pinnedchats");
  },
  // Whether the chats can be shown for this window.
  get canShow() {
    if (!SocialUI.haveLoggedInUser())
      return false;
    let docElem = document.documentElement;
    let chromeless = docElem.getAttribute("disablechrome") ||
                     docElem.getAttribute("chromehidden").indexOf("extrachrome") >= 0;
    return Social.uiVisible && !chromeless && !document.mozFullScreen;
  },
  openChat: function(aProvider, aURL, aCallback, aMode) {
    if (this.canShow)
      this.chatbar.openChat(aProvider, aURL, aCallback, aMode);
  },
  update: function() {
    if (!this.canShow)
      this.chatbar.removeAll();
  }
}

function sizeSocialPanelToContent(iframe) {
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
  iframe.style.height = height + "px";
  let computedWidth = parseInt(cs.marginLeft) + body.offsetWidth + parseInt(cs.marginRight);
  let width = Math.max(computedWidth, PANEL_MIN_WIDTH);
  iframe.style.width = width + "px";
}

function DynamicResizeWatcher() {
  this._mutationObserver = null;
}

DynamicResizeWatcher.prototype = {
  start: function DynamicResizeWatcher_start(iframe) {
    this.stop(); // just in case...
    let doc = iframe.contentDocument;
    this._mutationObserver = new iframe.contentWindow.MutationObserver(function(mutations) {
      sizeSocialPanelToContent(iframe);
    });
    // Observe anything that causes the size to change.
    let config = {attributes: true, characterData: true, childList: true, subtree: true};
    this._mutationObserver.observe(doc, config);
    // and since this may be setup after the load event has fired we do an
    // initial resize now.
    sizeSocialPanelToContent(iframe);
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
    sizeSocialPanelToContent(iframe);
  },

  unload: function() {
    let panel = this.panel;
    panel.hidePopup();
    if (!panel.firstChild)
      return
    panel.removeChild(panel.firstChild);
  },

  onShown: function(aEvent) {
    let iframe = this.panel.firstChild;
    this._dynamicResizer = new DynamicResizeWatcher();
    iframe.docShell.isActive = true;
    iframe.docShell.isAppTab = true;
    if (iframe.contentDocument.readyState == "complete") {
      this._dynamicResizer.start(iframe);
      this.dispatchPanelEvent("socialFrameShow");
    } else {
      // first time load, wait for load and dispatch after load
      iframe.addEventListener("load", function panelBrowserOnload(e) {
        iframe.removeEventListener("load", panelBrowserOnload, true);
        setTimeout(function() {
          SocialFlyout._dynamicResizer.start(iframe);
          SocialFlyout.dispatchPanelEvent("socialFrameShow");
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

    sizeSocialPanelToContent(iframe);
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
  // promptImages and promptMessages being null means we are yet to get the
  // message back from the provider with the images and icons (or that we got
  // the response but determined it was invalid.)
  promptImages: null,
  promptMessages: null,

  // Called once, after window load, when the Social.provider object is initialized
  init: function SSB_init() {
    this.updateButtonHiddenState();
    this.updateProfileInfo();
  },

  updateProfileInfo: function SSB_updateProfileInfo() {
    let profileRow = document.getElementById("unsharePopupHeader");
    let profile = Social.provider.profile;
    this.promptImages = null;
    this.promptMessages = null;
    if (profile && profile.displayName) {
      profileRow.hidden = false;
      let portrait = document.getElementById("socialUserPortrait");
      portrait.setAttribute("src", profile.portrait || "chrome://global/skin/icons/information-32.png");
      let displayName = document.getElementById("socialUserDisplayName");
      displayName.setAttribute("label", profile.displayName);
    } else {
      profileRow.hidden = true;
      this.updateButtonHiddenState();
      return;
    }
    // XXX - this shouldn't be done as part of updateProfileInfo, but instead
    // whenever we notice the provider has changed - but the concept of
    // "provider changed" will only exist once bug 774520 lands. 
    // get the recommend-prompt info.
    let port = Social.provider.getWorkerPort();
    if (port) {
      port.onmessage = function(evt) {
        if (evt.data.topic == "social.user-recommend-prompt-response") {
          port.close();
          this.acceptRecommendInfo(evt.data.data);
          this.updateButtonHiddenState();
          this.updateShareState();
        }
      }.bind(this);
      port.postMessage({topic: "social.user-recommend-prompt"});
    }
  },

  acceptRecommendInfo: function SSB_acceptRecommendInfo(data) {
    // Accept *and validate* the user-recommend-prompt-response message.
    let promptImages = {};
    let promptMessages = {};
    function reportError(reason) {
      Cu.reportError("Invalid recommend data from provider: " + reason + ": sharing is disabled for this provider");
      return false;
    }
    if (!data ||
        !data.images || typeof data.images != "object" ||
        !data.messages || typeof data.messages != "object") {
      return reportError("data is missing valid 'images' or 'messages' elements");
    }
    for (let sub of ["share", "unshare"]) {
      let url = data.images[sub];
      if (!url || typeof url != "string" || url.length == 0) {
        return reportError('images["' + sub + '"] is missing or not a non-empty string');
      }
      // resolve potentially relative URLs then check the scheme is acceptable.
      url = Services.io.newURI(Social.provider.origin, null, null).resolve(url);
      let uri = Services.io.newURI(url, null, null);
      if (!uri.schemeIs("http") && !uri.schemeIs("https") && !uri.schemeIs("data")) {
        return reportError('images["' + sub + '"] does not have a valid scheme');
      }
      promptImages[sub] = url;
    }
    for (let sub of ["shareTooltip", "unshareTooltip",
                     "sharedLabel", "unsharedLabel", "unshareLabel",
                     "portraitLabel", 
                     "unshareConfirmLabel", "unshareConfirmAccessKey",
                     "unshareCancelLabel", "unshareCancelAccessKey"]) {
      if (typeof data.messages[sub] != "string" || data.messages[sub].length == 0) {
        return reportError('messages["' + sub + '"] is not a valid string');
      }
      promptMessages[sub] = data.messages[sub];
    }
    this.promptImages = promptImages;
    this.promptMessages = promptMessages;
    return true;
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

  updateButtonHiddenState: function SSB_updateButtonHiddenState() {
    let shareButton = this.shareButton;
    if (shareButton)
      shareButton.hidden = !Social.uiVisible || this.promptImages == null ||
                           !SocialUI.haveLoggedInUser();
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
    updateElement("unsharePopupContinueSharingButton",
                  {label: this.promptMessages.unshareCancelLabel,
                   accesskey: this.promptMessages.unshareCancelAccessKey});
    updateElement("unsharePopupStopSharingButton",
                  {label: this.promptMessages.unshareConfirmLabel,
                  accesskey: this.promptMessages.unshareConfirmAccessKey});
    updateElement("socialUserPortrait",
                  {"aria-label": this.promptMessages.portraitLabel});
    updateElement("socialUserRecommendedText",
                  {value: this.promptMessages.unshareLabel});
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
    let currentPageShared = Social.isPageShared(gBrowser.currentURI);

    // Provide a11y-friendly notification of share.
    let status = document.getElementById("share-button-status");
    if (status) {
      // XXX - this should also be capable of reflecting that the page was
      // unshared (ie, it needs to manage three-states: (1) nothing done, (2)
      // shared, (3) shared then unshared)
      // Note that we *do* have an appropriate string from the provider for
      // this (promptMessages['unsharedLabel'] but currently lack a way of
      // tracking this state)
      let statusString = currentPageShared ?
                           this.promptMessages['sharedLabel'] : "";
      status.setAttribute("value", statusString);
    }

    // Update the share button, if present
    let shareButton = this.shareButton;
    if (!shareButton || shareButton.hidden)
      return;

    let imageURL;
    if (currentPageShared) {
      shareButton.setAttribute("shared", "true");
      shareButton.setAttribute("tooltiptext", this.promptMessages['unshareTooltip']);
      imageURL = this.promptImages["unshare"]
    } else {
      shareButton.removeAttribute("shared");
      shareButton.setAttribute("tooltiptext", this.promptMessages['shareTooltip']);
      imageURL = this.promptImages["share"]
    }
    shareButton.style.backgroundImage = 'url("' + encodeURI(imageURL) + '")';
  }
};

var SocialToolbar = {
  // Called once, after window load, when the Social.provider object is initialized
  init: function SocialToolbar_init() {
    this.button.setAttribute("image", Social.provider.iconURL);
    this.updateButton();
    this.updateProfile();
    this._dynamicResizer = new DynamicResizeWatcher();
  },

  get button() {
    return document.getElementById("social-provider-button");
  },

  updateButtonHiddenState: function SocialToolbar_updateButtonHiddenState() {
    let tbi = document.getElementById("social-toolbar-item");
    tbi.hidden = !Social.uiVisible;
    if (!SocialUI.haveLoggedInUser()) {
      let parent = document.getElementById("social-notification-box");
      while (parent.hasChildNodes())
        parent.removeChild(parent.firstChild);

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
    let icons = provider.ambientNotificationIcons;
    let iconNames = Object.keys(icons);
    let iconBox = document.getElementById("social-toolbar-item");
    let notifBox = document.getElementById("social-notification-box");
    let panel = document.getElementById("social-notification-panel");
    panel.hidden = false;

    let command = document.getElementById("Social:ToggleNotifications");
    command.setAttribute("checked", Services.prefs.getBoolPref("social.toast-notifications.enabled"));

    const CACHE_PREF_NAME = "social.cached.notificationIcons";
    // provider.profile == undefined means no response yet from the provider
    // to tell us whether the user is logged in or not.
    if (!SocialUI.haveLoggedInUser() && provider.profile !== undefined) {
      // The provider has responded with a profile and the user isn't logged
      // in.  The icons etc have already been removed by
      // updateButtonHiddenState, so we want to nuke any cached icons we
      // have and get out of here!
      Services.prefs.clearUserPref(CACHE_PREF_NAME);
      return;
    }
    if (Social.provider.profile === undefined) {
      // provider has not told us about the login state yet - see if we have
      // a cached version for this provider.
      let cached;
      try {
        cached = JSON.parse(Services.prefs.getCharPref(CACHE_PREF_NAME));
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
      Services.prefs.setCharPref(CACHE_PREF_NAME,
                                 JSON.stringify({provider: Social.provider.origin,
                                                 data: icons}));
    }

    let notificationFrames = document.createDocumentFragment();
    let iconContainers = document.createDocumentFragment();

    let createdFrames = [];

    for each(let name in iconNames) {
      let icon = icons[name];

      let notificationFrameId = "social-status-" + icon.name;
      let notificationFrame = document.getElementById(notificationFrameId);
      if (!notificationFrame) {
        notificationFrame = document.createElement("iframe");
        notificationFrame.setAttribute("type", "content");
        notificationFrame.setAttribute("class", "social-panel-frame");
        notificationFrame.setAttribute("id", notificationFrameId);
        notificationFrame.setAttribute("mozbrowser", "true");
        // work around bug 793057 - by making the panel roughly the final size
        // we are more likely to have the anchor in the correct position.
        notificationFrame.style.width = PANEL_MIN_WIDTH + "px";

        createdFrames.push(notificationFrame);
        notificationFrames.appendChild(notificationFrame);
      }
      notificationFrame.setAttribute("origin", provider.origin);
      if (notificationFrame.getAttribute("src") != icon.contentPanel)
        notificationFrame.setAttribute("src", icon.contentPanel);

      let iconId = "social-notification-icon-" + icon.name;
      let imageId = iconId + "-image";
      let labelId = iconId + "-label";
      let stackId = iconId + "-stack";
      let stack = document.getElementById(stackId);
      let image, label;
      if (stack) {
        image = document.getElementById(imageId);
        label = document.getElementById(labelId);
      } else {
        let box = document.createElement("box");
        box.classList.add("toolbarbutton-1");
        box.setAttribute("id", iconId);
        box.addEventListener("mousedown", function (e) {
          if (e.button == 0)
            SocialToolbar.showAmbientPopup(box);
        }, false);
        box.setAttribute("notificationFrameId", notificationFrameId);
        stack = document.createElement("stack");
        stack.setAttribute("id", stackId);
        stack.classList.add("social-notification-icon-stack");
        stack.classList.add("toolbarbutton-icon");
        image = document.createElement("image");
        image.setAttribute("id", imageId);
        image.classList.add("social-notification-icon-image");
        image = stack.appendChild(image);
        label = document.createElement("label");
        label.setAttribute("id", labelId);
        label.classList.add("social-notification-icon-label");
        let hbox = document.createElement("hbox");
        hbox.classList.add("social-notification-icon-hbox");
        hbox.setAttribute("align", "start");
        hbox.setAttribute("pack", "end");
        label = hbox.appendChild(label);
        stack.appendChild(hbox);
        box.appendChild(stack);
        iconContainers.appendChild(box);
      }

      let labelValue = icon.counter || "";
      // Only update the value attribute if it has changed to reduce layout changes.
      if (!label.hasAttribute("value") || label.getAttribute("value") != labelValue)
        label.setAttribute("value", labelValue);

      if (image.getAttribute("src") != icon.iconURL)
        image.setAttribute("src", icon.iconURL);
    }
    notifBox.appendChild(notificationFrames);
    iconBox.appendChild(iconContainers);

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

  showAmbientPopup: function SocialToolbar_showAmbientPopup(aToolbarButtonBox) {
    let panel = document.getElementById("social-notification-panel");
    let notifBox = document.getElementById("social-notification-box");
    let notificationFrameId = aToolbarButtonBox.getAttribute("notificationFrameId");
    let notificationFrame = document.getElementById(notificationFrameId);

    // Clear dimensions on all browsers so the panel size will
    // only use the selected browser.
    let frameIter = notifBox.firstElementChild;
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
      aToolbarButtonBox.removeAttribute("open");
      dynamicResizer.stop();
      notificationFrame.docShell.isActive = false;
      dispatchPanelEvent("socialFrameHide");
    });

    panel.addEventListener("popupshown", function onpopupshown() {
      panel.removeEventListener("popupshown", onpopupshown);
      aToolbarButtonBox.setAttribute("open", "true");
      notificationFrame.docShell.isActive = true;
      notificationFrame.docShell.isAppTab = true;
      if (notificationFrame.contentDocument.readyState == "complete") {
        dynamicResizer.start(notificationFrame);
        dispatchPanelEvent("socialFrameShow");
      } else {
        // first time load, wait for load and dispatch after load
        notificationFrame.addEventListener("load", function panelBrowserOnload(e) {
          notificationFrame.removeEventListener("load", panelBrowserOnload, true);
          dynamicResizer.start(notificationFrame);
          setTimeout(function() {
            dispatchPanelEvent("socialFrameShow");
          }, 0);
        }, true);
      }
    });

    let imageId = aToolbarButtonBox.getAttribute("id") + "-image";
    let toolbarButtonImage = document.getElementById(imageId);
    panel.openPopup(toolbarButtonImage, "bottomcenter topleft", 0, 0, false, false);
  },

  setPanelErrorMessage: function SocialToolbar_setPanelErrorMessage(aNotificationFrame) {
    if (!aNotificationFrame)
      return;

    let src = aNotificationFrame.getAttribute("src");
    aNotificationFrame.removeAttribute("src");
    aNotificationFrame.webNavigation.loadURI("about:socialerror?mode=tryAgainOnly&url=" +
                                             encodeURIComponent(src), null, null, null, null);
    sizeSocialPanelToContent(aNotificationFrame);
  }
}

var SocialSidebar = {
  // Called once, after window load, when the Social.provider object is initialized
  init: function SocialSidebar_init() {
    let sbrowser = document.getElementById("social-sidebar-browser");
    // setting isAppTab causes clicks on untargeted links to open new tabs
    sbrowser.docShell.isAppTab = true;
    sbrowser.webProgress.addProgressListener(new SocialErrorListener("sidebar"),
                                             Ci.nsIWebProgress.NOTIFY_STATE_REQUEST |
                                             Ci.nsIWebProgress.NOTIFY_LOCATION);
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
           docElem.getAttribute('chromehidden').contains("toolbar");
  },

  // Whether the user has toggled the sidebar on (for windows where it can appear)
  get opened() {
    return Services.prefs.getBoolPref("social.sidebar.open") && !document.mozFullScreen;
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
    let hideSidebar = !this.canShow || !this.opened;
    let broadcaster = document.getElementById("socialSidebarBroadcaster");
    broadcaster.hidden = hideSidebar;
    command.setAttribute("checked", !hideSidebar);

    let sbrowser = document.getElementById("social-sidebar-browser");
    sbrowser.docShell.isActive = !hideSidebar;
    if (hideSidebar) {
      this.dispatchEvent("socialFrameHide");
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
            SocialSidebar.dispatchEvent("socialFrameShow");
          }, 0);
        });
      } else {
        this.dispatchEvent("socialFrameShow");
      }
    }
  },

  setSidebarErrorMessage: function() {
    let sbrowser = document.getElementById("social-sidebar-browser");
    let url = encodeURIComponent(Social.provider.sidebarURL);
    sbrowser.loadURI("about:socialerror?mode=tryAgain&url=" + url, null, null);
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
    if (failure) {
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
        SocialSidebar.setSidebarErrorMessage();
        break;

      case "notification-panel":
        let frame = aWebProgress.QueryInterface(Ci.nsIDocShell)
                                .chromeEventHandler;
        SocialToolbar.setPanelErrorMessage(frame);
        break;
    }
  }
};
