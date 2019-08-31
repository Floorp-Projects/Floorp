/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
var { BrowserUtils } = ChromeUtils.import(
  "resource://gre/modules/BrowserUtils.jsm"
);
var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  SpellCheckHelper: "resource://gre/modules/InlineSpellChecker.jsm",
  LoginHelper: "resource://gre/modules/LoginHelper.jsm",
  LoginManagerContextMenu: "resource://gre/modules/LoginManagerContextMenu.jsm",
  WebNavigationFrames: "resource://gre/modules/WebNavigationFrames.jsm",
  ContextualIdentityService:
    "resource://gre/modules/ContextualIdentityService.jsm",
  DevToolsShim: "chrome://devtools-startup/content/DevToolsShim.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
});

XPCOMUtils.defineLazyGetter(this, "ReferrerInfo", () =>
  Components.Constructor(
    "@mozilla.org/referrer-info;1",
    "nsIReferrerInfo",
    "init"
  )
);

var gContextMenuContentData = null;

function openContextMenu(aMessage, aBrowser, aActor) {
  let data = aMessage.data;
  let browser = aBrowser;
  let actor = aActor;
  let spellInfo = data.spellInfo;
  let frameReferrerInfo = data.frameReferrerInfo;
  let linkReferrerInfo = data.linkReferrerInfo;
  let principal = data.principal;
  let storagePrincipal = data.storagePrincipal;

  if (spellInfo) {
    spellInfo.target = browser.messageManager;
  }

  let documentURIObject = makeURI(
    data.docLocation,
    data.charSet,
    makeURI(data.baseURI)
  );

  if (frameReferrerInfo) {
    frameReferrerInfo = E10SUtils.deserializeReferrerInfo(frameReferrerInfo);
  }

  if (linkReferrerInfo) {
    linkReferrerInfo = E10SUtils.deserializeReferrerInfo(linkReferrerInfo);
  }

  // For now, JS Window Actors don't deserialize Principals automatically, so we
  // have to do it ourselves. See bug 1557852.
  if (principal) {
    principal = E10SUtils.deserializePrincipal(principal);
  }
  if (storagePrincipal) {
    storagePrincipal = E10SUtils.deserializePrincipal(storagePrincipal);
  }

  if (data.context.principal) {
    data.context.principal = E10SUtils.deserializePrincipal(
      data.context.principal
    );
  }
  if (data.context.storagePrincipal) {
    data.context.storagePrincipal = E10SUtils.deserializePrincipal(
      data.context.storagePrincipal
    );
  }

  gContextMenuContentData = {
    context: data.context,
    popupNodeSelectors: data.popupNodeSelectors,
    browser,
    actor,
    editFlags: data.editFlags,
    spellInfo,
    principal,
    storagePrincipal,
    customMenuItems: data.customMenuItems,
    documentURIObject,
    docLocation: data.docLocation,
    charSet: data.charSet,
    referrerInfo: E10SUtils.deserializeReferrerInfo(data.referrerInfo),
    frameReferrerInfo,
    linkReferrerInfo,
    contentType: data.contentType,
    contentDisposition: data.contentDisposition,
    frameOuterWindowID: data.frameOuterWindowID,
    selectionInfo: data.selectionInfo,
    disableSetDesktopBackground: data.disableSetDesktopBackground,
    loginFillInfo: data.loginFillInfo,
    parentAllowsMixedContent: data.parentAllowsMixedContent,
    userContextId: data.userContextId,
    webExtContextData: data.webExtContextData,
  };

  let popup = browser.ownerDocument.getElementById("contentAreaContextMenu");
  let context = gContextMenuContentData.context;

  // The event is a CPOW that can't be passed into the native openPopupAtScreen
  // function. Therefore we synthesize a new MouseEvent to propagate the
  // inputSource to the subsequently triggered popupshowing event.
  var newEvent = document.createEvent("MouseEvent");
  newEvent.initNSMouseEvent(
    "contextmenu",
    true,
    true,
    null,
    0,
    context.screenX,
    context.screenY,
    0,
    0,
    false,
    false,
    false,
    false,
    0,
    null,
    0,
    context.mozInputSource
  );
  popup.openPopupAtScreen(newEvent.screenX, newEvent.screenY, true, newEvent);
}

function nsContextMenu(aXulMenu, aIsShift) {
  this.shouldDisplay = true;
  this.initMenu(aXulMenu, aIsShift);
}

// Prototype for nsContextMenu "class."
nsContextMenu.prototype = {
  initMenu: function CM_initMenu(aXulMenu, aIsShift) {
    // Get contextual info.
    this.setContext();

    if (!this.shouldDisplay) {
      return;
    }

    this.hasPageMenu = false;
    this.isContentSelected = !this.selectionInfo.docSelectionIsCollapsed;
    if (!aIsShift) {
      this.hasPageMenu = PageMenuParent.addToPopup(
        gContextMenuContentData.customMenuItems,
        this.browser,
        aXulMenu
      );

      let tab =
        gBrowser && gBrowser.getTabForBrowser
          ? gBrowser.getTabForBrowser(this.browser)
          : undefined;

      let subject = {
        menu: aXulMenu,
        tab,
        timeStamp: this.timeStamp,
        isContentSelected: this.isContentSelected,
        inFrame: this.inFrame,
        isTextSelected: this.isTextSelected,
        onTextInput: this.onTextInput,
        onLink: this.onLink,
        onImage: this.onImage,
        onVideo: this.onVideo,
        onAudio: this.onAudio,
        onCanvas: this.onCanvas,
        onEditable: this.onEditable,
        onSpellcheckable: this.onSpellcheckable,
        onPassword: this.onPassword,
        srcUrl: this.mediaURL,
        frameUrl: gContextMenuContentData
          ? gContextMenuContentData.docLocation
          : undefined,
        pageUrl: this.browser ? this.browser.currentURI.spec : undefined,
        linkText: this.linkTextStr,
        linkUrl: this.linkURL,
        selectionText: this.isTextSelected
          ? this.selectionInfo.fullText
          : undefined,
        frameId: this.frameOuterWindowID,
        webExtBrowserType: this.webExtBrowserType,
        webExtContextData: gContextMenuContentData
          ? gContextMenuContentData.webExtContextData
          : undefined,
      };
      subject.wrappedJSObject = subject;
      Services.obs.notifyObservers(subject, "on-build-contextmenu");
    }

    this.viewFrameSourceElement = document.getElementById(
      "context-viewframesource"
    );
    this.ellipsis = "\u2026";
    try {
      this.ellipsis = Services.prefs.getComplexValue(
        "intl.ellipsis",
        Ci.nsIPrefLocalizedString
      ).data;
    } catch (e) {}

    // Reset after "on-build-contextmenu" notification in case selection was
    // changed during the notification.
    this.isContentSelected = !this.selectionInfo.docSelectionIsCollapsed;
    this.onPlainTextLink = false;

    let bookmarkPage = document.getElementById("context-bookmarkpage");
    if (bookmarkPage) {
      BookmarkingUI.onCurrentPageContextPopupShowing();
    }

    // Initialize (disable/remove) menu items.
    this.initItems();
  },

  setContext() {
    let context = Object.create(null);

    if (gContextMenuContentData) {
      context = gContextMenuContentData.context;
      gContextMenuContentData.context = null;
    }

    this.shouldDisplay = context.shouldDisplay;
    this.timeStamp = context.timeStamp;

    // Assign what's _possibly_ needed from `context` sent by ContextMenuChild.jsm
    // Keep this consistent with the similar code in ContextMenu's _setContext
    this.bgImageURL = context.bgImageURL;
    this.imageDescURL = context.imageDescURL;
    this.imageInfo = context.imageInfo;
    this.mediaURL = context.mediaURL;
    this.webExtBrowserType = context.webExtBrowserType;

    this.canSpellCheck = context.canSpellCheck;
    this.hasBGImage = context.hasBGImage;
    this.hasMultipleBGImages = context.hasMultipleBGImages;
    this.isDesignMode = context.isDesignMode;
    this.inFrame = context.inFrame;
    this.inSrcdocFrame = context.inSrcdocFrame;
    this.inSyntheticDoc = context.inSyntheticDoc;
    this.inTabBrowser = context.inTabBrowser;
    this.inWebExtBrowser = context.inWebExtBrowser;

    this.link = context.link;
    this.linkDownload = context.linkDownload;
    this.linkProtocol = context.linkProtocol;
    this.linkTextStr = context.linkTextStr;
    this.linkURL = context.linkURL;
    this.linkURI = this.getLinkURI(); // can't send; regenerate

    this.onAudio = context.onAudio;
    this.onCanvas = context.onCanvas;
    this.onCompletedImage = context.onCompletedImage;
    this.onCTPPlugin = context.onCTPPlugin;
    this.onDRMMedia = context.onDRMMedia;
    this.onPiPVideo = context.onPiPVideo;
    this.onEditable = context.onEditable;
    this.onImage = context.onImage;
    this.onKeywordField = context.onKeywordField;
    this.onLink = context.onLink;
    this.onLoadedImage = context.onLoadedImage;
    this.onMailtoLink = context.onMailtoLink;
    this.onMozExtLink = context.onMozExtLink;
    this.onNumeric = context.onNumeric;
    this.onPassword = context.onPassword;
    this.onSaveableLink = context.onSaveableLink;
    this.onSpellcheckable = context.onSpellcheckable;
    this.onTextInput = context.onTextInput;
    this.onVideo = context.onVideo;

    this.target = context.target;
    this.targetIdentifier = context.targetIdentifier;

    this.principal = context.principal;
    this.storagePrincipal = context.storagePrincipal;
    this.frameOuterWindowID = context.frameOuterWindowID;

    this.inSyntheticDoc = context.inSyntheticDoc;
    this.inAboutDevtoolsToolbox = context.inAboutDevtoolsToolbox;

    // Everything after this isn't sent directly from ContextMenu
    if (this.target) {
      this.ownerDoc = this.target.ownerDocument;
    }

    this.csp = E10SUtils.deserializeCSP(context.csp);

    // Remember the CSS selectors corresponding to clicked node. gContextMenuContentData
    // can be null if the menu was triggered by tests in which case use an empty array.
    this.targetSelectors = gContextMenuContentData
      ? gContextMenuContentData.popupNodeSelectors
      : [];

    if (gContextMenuContentData) {
      this.browser = gContextMenuContentData.browser;
      this.selectionInfo = gContextMenuContentData.selectionInfo;
      this.actor = gContextMenuContentData.actor;
    } else {
      this.browser = this.ownerDoc.defaultView.docShell.chromeEventHandler;
      this.selectionInfo = BrowserUtils.getSelectionDetails(window);
      this.actor = this.browser.browsingContext.currentWindowGlobal.getActor(
        "ContextMenu"
      );
    }

    const { gBrowser } = this.browser.ownerGlobal;

    this.textSelected = this.selectionInfo.text;
    this.isTextSelected = this.textSelected.length != 0;
    this.webExtBrowserType = this.browser.getAttribute(
      "webextension-view-type"
    );
    this.inWebExtBrowser = !!this.webExtBrowserType;
    this.inTabBrowser =
      gBrowser && gBrowser.getTabForBrowser
        ? !!gBrowser.getTabForBrowser(this.browser)
        : false;

    if (context.shouldInitInlineSpellCheckerUINoChildren) {
      InlineSpellCheckerUI.initFromRemote(
        gContextMenuContentData.spellInfo,
        this.actor.manager
      );
    }

    if (context.shouldInitInlineSpellCheckerUIWithChildren) {
      InlineSpellCheckerUI.initFromRemote(
        gContextMenuContentData.spellInfo,
        this.actor.manager
      );
      let canSpell = InlineSpellCheckerUI.canSpellCheck && this.canSpellCheck;
      this.showItem("spell-check-enabled", canSpell);
      this.showItem("spell-separator", canSpell);
    }
  }, // setContext

  hiding: function CM_hiding() {
    if (this.actor) {
      this.actor.hiding();
    }

    gContextMenuContentData = null;
    InlineSpellCheckerUI.clearSuggestionsFromMenu();
    InlineSpellCheckerUI.clearDictionaryListFromMenu();
    InlineSpellCheckerUI.uninit();
    if (
      Cu.isModuleLoaded("resource://gre/modules/LoginManagerContextMenu.jsm")
    ) {
      LoginManagerContextMenu.clearLoginsFromMenu(document);
    }

    // This handler self-deletes, only run it if it is still there:
    if (this._onPopupHiding) {
      this._onPopupHiding();
    }
  },

  initItems: function CM_initItems() {
    this.initPageMenuSeparator();
    this.initOpenItems();
    this.initNavigationItems();
    this.initViewItems();
    this.initMiscItems();
    this.initSpellingItems();
    this.initSaveItems();
    this.initClipboardItems();
    this.initMediaPlayerItems();
    this.initLeaveDOMFullScreenItems();
    this.initClickToPlayItems();
    this.initPasswordManagerItems();
    this.initSyncItems();
  },

  initPageMenuSeparator: function CM_initPageMenuSeparator() {
    this.showItem("page-menu-separator", this.hasPageMenu);
  },

  initOpenItems: function CM_initOpenItems() {
    var isMailtoInternal = false;
    if (this.onMailtoLink) {
      var mailtoHandler = Cc[
        "@mozilla.org/uriloader/external-protocol-service;1"
      ]
        .getService(Ci.nsIExternalProtocolService)
        .getProtocolHandlerInfo("mailto");
      isMailtoInternal =
        !mailtoHandler.alwaysAskBeforeHandling &&
        mailtoHandler.preferredAction == Ci.nsIHandlerInfo.useHelperApp &&
        mailtoHandler.preferredApplicationHandler instanceof
          Ci.nsIWebHandlerApp;
    }

    if (
      this.isTextSelected &&
      !this.onLink &&
      this.selectionInfo &&
      this.selectionInfo.linkURL
    ) {
      this.linkURL = this.selectionInfo.linkURL;
      try {
        this.linkURI = makeURI(this.linkURL);
      } catch (ex) {}

      this.linkTextStr = this.selectionInfo.linkText;
      this.onPlainTextLink = true;
    }

    var inContainer = false;
    if (gContextMenuContentData.userContextId) {
      inContainer = true;
      var item = document.getElementById("context-openlinkincontainertab");

      item.setAttribute(
        "data-usercontextid",
        gContextMenuContentData.userContextId
      );

      var label = ContextualIdentityService.getUserContextLabel(
        gContextMenuContentData.userContextId
      );
      item.setAttribute(
        "label",
        gBrowserBundle.formatStringFromName("userContextOpenLink.label", [
          label,
        ])
      );
    }

    var shouldShow =
      this.onSaveableLink || isMailtoInternal || this.onPlainTextLink;
    var isWindowPrivate = PrivateBrowsingUtils.isWindowPrivate(window);
    let showContainers =
      Services.prefs.getBoolPref("privacy.userContext.enabled") &&
      ContextualIdentityService.getPublicIdentities().length;
    this.showItem("context-openlink", shouldShow && !isWindowPrivate);
    this.showItem(
      "context-openlinkprivate",
      shouldShow && PrivateBrowsingUtils.enabled
    );
    this.showItem("context-openlinkintab", shouldShow && !inContainer);
    this.showItem("context-openlinkincontainertab", shouldShow && inContainer);
    this.showItem(
      "context-openlinkinusercontext-menu",
      shouldShow && !isWindowPrivate && showContainers
    );
    this.showItem("context-openlinkincurrent", this.onPlainTextLink);
    this.showItem("context-sep-open", shouldShow);
  },

  initNavigationItems: function CM_initNavigationItems() {
    var shouldShow =
      !(
        this.isContentSelected ||
        this.onLink ||
        this.onImage ||
        this.onCanvas ||
        this.onVideo ||
        this.onAudio ||
        this.onTextInput
      ) && this.inTabBrowser;
    this.showItem("context-navigation", shouldShow);
    this.showItem("context-sep-navigation", shouldShow);

    let stopped =
      XULBrowserWindow.stopCommand.getAttribute("disabled") == "true";

    let stopReloadItem = "";
    if (shouldShow || !this.inTabBrowser) {
      stopReloadItem = stopped || !this.inTabBrowser ? "reload" : "stop";
    }

    this.showItem("context-reload", stopReloadItem == "reload");
    this.showItem("context-stop", stopReloadItem == "stop");
  },

  initLeaveDOMFullScreenItems: function CM_initLeaveFullScreenItem() {
    // only show the option if the user is in DOM fullscreen
    var shouldShow = this.target.ownerDocument.fullscreen;
    this.showItem("context-leave-dom-fullscreen", shouldShow);

    // Explicitly show if in DOM fullscreen, but do not hide it has already been shown
    if (shouldShow) {
      this.showItem("context-media-sep-commands", true);
    }
  },

  initSaveItems: function CM_initSaveItems() {
    var shouldShow = !(
      this.onTextInput ||
      this.onLink ||
      this.isContentSelected ||
      this.onImage ||
      this.onCanvas ||
      this.onVideo ||
      this.onAudio
    );
    this.showItem("context-savepage", shouldShow);

    // Save link depends on whether we're in a link, or selected text matches valid URL pattern.
    this.showItem(
      "context-savelink",
      this.onSaveableLink || this.onPlainTextLink
    );

    // Save image depends on having loaded its content, video and audio don't.
    this.showItem("context-saveimage", this.onLoadedImage || this.onCanvas);
    this.showItem("context-savevideo", this.onVideo);
    this.showItem("context-saveaudio", this.onAudio);
    this.showItem("context-video-saveimage", this.onVideo);
    this.setItemAttr("context-savevideo", "disabled", !this.mediaURL);
    this.setItemAttr("context-saveaudio", "disabled", !this.mediaURL);
    // Send media URL (but not for canvas, since it's a big data: URL)
    this.showItem("context-sendimage", this.onImage);
    this.showItem("context-sendvideo", this.onVideo);
    this.showItem("context-sendaudio", this.onAudio);
    let mediaIsBlob = this.mediaURL.startsWith("blob:");
    this.setItemAttr(
      "context-sendvideo",
      "disabled",
      !this.mediaURL || mediaIsBlob
    );
    this.setItemAttr(
      "context-sendaudio",
      "disabled",
      !this.mediaURL || mediaIsBlob
    );
  },

  initViewItems: function CM_initViewItems() {
    // View source is always OK, unless in directory listing.
    this.showItem(
      "context-viewpartialsource-selection",
      !this.inAboutDevtoolsToolbox && this.isContentSelected
    );

    var shouldShow = !(
      this.isContentSelected ||
      this.onImage ||
      this.onCanvas ||
      this.onVideo ||
      this.onAudio ||
      this.onLink ||
      this.onTextInput
    );

    var showInspect =
      this.inTabBrowser &&
      !this.inAboutDevtoolsToolbox &&
      Services.prefs.getBoolPref("devtools.inspector.enabled", true) &&
      !Services.prefs.getBoolPref("devtools.policy.disabled", false);

    var showInspectA11Y =
      showInspect &&
      // Only when accessibility service started.
      Services.appinfo.accessibilityEnabled &&
      this.inTabBrowser &&
      Services.prefs.getBoolPref("devtools.enabled", true) &&
      Services.prefs.getBoolPref("devtools.accessibility.enabled", true) &&
      !Services.prefs.getBoolPref("devtools.policy.disabled", false);

    this.showItem("context-viewsource", shouldShow);
    this.showItem("context-viewinfo", shouldShow);
    // The page info is broken for WebExtension popups, as the browser is
    // destroyed when the popup is closed.
    this.setItemAttr(
      "context-viewinfo",
      "disabled",
      this.webExtBrowserType === "popup"
    );
    this.showItem("inspect-separator", showInspect);
    this.showItem("context-inspect", showInspect);

    this.showItem("context-inspect-a11y", showInspectA11Y);

    this.showItem("context-sep-viewsource", shouldShow);

    // Set as Desktop background depends on whether an image was clicked on,
    // and only works if we have a shell service.
    var haveSetDesktopBackground = false;

    if (
      AppConstants.HAVE_SHELL_SERVICE &&
      Services.policies.isAllowed("setDesktopBackground")
    ) {
      // Only enable Set as Desktop Background if we can get the shell service.
      var shell = getShellService();
      if (shell) {
        haveSetDesktopBackground = shell.canSetDesktopBackground;
      }
    }

    this.showItem(
      "context-setDesktopBackground",
      haveSetDesktopBackground && this.onLoadedImage
    );

    if (haveSetDesktopBackground && this.onLoadedImage) {
      document.getElementById("context-setDesktopBackground").disabled =
        gContextMenuContentData.disableSetDesktopBackground;
    }

    // Reload image depends on an image that's not fully loaded
    this.showItem(
      "context-reloadimage",
      this.onImage && !this.onCompletedImage
    );

    // View image depends on having an image that's not standalone
    // (or is in a frame), or a canvas.
    this.showItem(
      "context-viewimage",
      (this.onImage && (!this.inSyntheticDoc || this.inFrame)) || this.onCanvas
    );

    // View video depends on not having a standalone video.
    this.showItem(
      "context-viewvideo",
      this.onVideo && (!this.inSyntheticDoc || this.inFrame)
    );
    this.setItemAttr("context-viewvideo", "disabled", !this.mediaURL);

    // View background image depends on whether there is one, but don't make
    // background images of a stand-alone media document available.
    this.showItem(
      "context-viewbgimage",
      shouldShow && !this.hasMultipleBGImages && !this.inSyntheticDoc
    );
    this.showItem(
      "context-sep-viewbgimage",
      shouldShow && !this.hasMultipleBGImages && !this.inSyntheticDoc
    );
    document.getElementById("context-viewbgimage").disabled = !this.hasBGImage;

    this.showItem("context-viewimageinfo", this.onImage);
    // The image info popup is broken for WebExtension popups, since the browser
    // is destroyed when the popup is closed.
    this.setItemAttr(
      "context-viewimageinfo",
      "disabled",
      this.webExtBrowserType === "popup"
    );
    this.showItem(
      "context-viewimagedesc",
      this.onImage && this.imageDescURL !== ""
    );
  },

  initMiscItems: function CM_initMiscItems() {
    // Use "Bookmark This Link" if on a link.
    let bookmarkPage = document.getElementById("context-bookmarkpage");
    this.showItem(
      bookmarkPage,
      !(
        this.isContentSelected ||
        this.onTextInput ||
        this.onLink ||
        this.onImage ||
        this.onVideo ||
        this.onAudio ||
        this.onCanvas ||
        this.inWebExtBrowser
      )
    );

    this.showItem(
      "context-bookmarklink",
      (this.onLink && !this.onMailtoLink && !this.onMozExtLink) ||
        this.onPlainTextLink
    );
    this.showItem(
      "context-keywordfield",
      this.onTextInput && this.onKeywordField
    );
    this.showItem("frame", this.inFrame);

    if (this.inFrame) {
      // To make it easier to debug the browser running with out-of-process iframes, we
      // display the process PID of the iframe in the context menu for the subframe.
      let frameOsPid = this.actor.manager.browsingContext.currentWindowGlobal
        .osPid;
      this.setItemAttr("context-frameOsPid", "label", "PID: " + frameOsPid);
    }

    let showSearchSelect =
      !this.inAboutDevtoolsToolbox &&
      (this.isTextSelected || this.onLink) &&
      !this.onImage;
    this.showItem("context-searchselect", showSearchSelect);
    if (showSearchSelect) {
      this.formatSearchContextItem();
    }

    // srcdoc cannot be opened separately due to concerns about web
    // content with about:srcdoc in location bar masquerading as trusted
    // chrome/addon content.
    // No need to also test for this.inFrame as this is checked in the parent
    // submenu.
    this.showItem("context-showonlythisframe", !this.inSrcdocFrame);
    this.showItem("context-openframeintab", !this.inSrcdocFrame);
    this.showItem("context-openframe", !this.inSrcdocFrame);
    this.showItem("context-bookmarkframe", !this.inSrcdocFrame);
    this.showItem("open-frame-sep", !this.inSrcdocFrame);

    this.showItem("frame-sep", this.inFrame && this.isTextSelected);

    // Hide menu entries for images, show otherwise
    if (this.inFrame) {
      if (
        BrowserUtils.mimeTypeIsTextBased(this.target.ownerDocument.contentType)
      ) {
        this.viewFrameSourceElement.removeAttribute("hidden");
      } else {
        this.viewFrameSourceElement.setAttribute("hidden", "true");
      }
    }

    // BiDi UI
    this.showItem("context-sep-bidi", !this.onNumeric && top.gBidiUI);
    this.showItem(
      "context-bidi-text-direction-toggle",
      this.onTextInput && !this.onNumeric && top.gBidiUI
    );
    this.showItem(
      "context-bidi-page-direction-toggle",
      !this.onTextInput && top.gBidiUI
    );
  },

  initSpellingItems() {
    var canSpell =
      InlineSpellCheckerUI.canSpellCheck &&
      !InlineSpellCheckerUI.initialSpellCheckPending &&
      this.canSpellCheck;
    let showDictionaries = canSpell && InlineSpellCheckerUI.enabled;
    var onMisspelling = InlineSpellCheckerUI.overMisspelling;
    var showUndo = canSpell && InlineSpellCheckerUI.canUndo();
    this.showItem("spell-check-enabled", canSpell);
    this.showItem("spell-separator", canSpell);
    document
      .getElementById("spell-check-enabled")
      .setAttribute("checked", canSpell && InlineSpellCheckerUI.enabled);

    this.showItem("spell-add-to-dictionary", onMisspelling);
    this.showItem("spell-undo-add-to-dictionary", showUndo);

    // suggestion list
    this.showItem("spell-suggestions-separator", onMisspelling || showUndo);
    if (onMisspelling) {
      var suggestionsSeparator = document.getElementById(
        "spell-add-to-dictionary"
      );
      var numsug = InlineSpellCheckerUI.addSuggestionsToMenu(
        suggestionsSeparator.parentNode,
        suggestionsSeparator,
        5
      );
      this.showItem("spell-no-suggestions", numsug == 0);
    } else {
      this.showItem("spell-no-suggestions", false);
    }

    // dictionary list
    this.showItem("spell-dictionaries", showDictionaries);
    if (canSpell) {
      var dictMenu = document.getElementById("spell-dictionaries-menu");
      var dictSep = document.getElementById("spell-language-separator");
      let count = InlineSpellCheckerUI.addDictionaryListToMenu(
        dictMenu,
        dictSep
      );
      this.showItem(dictSep, count > 0);
      this.showItem("spell-add-dictionaries-main", false);
    } else if (this.onSpellcheckable) {
      // when there is no spellchecker but we might be able to spellcheck
      // add the add to dictionaries item. This will ensure that people
      // with no dictionaries will be able to download them
      this.showItem("spell-language-separator", showDictionaries);
      this.showItem("spell-add-dictionaries-main", showDictionaries);
    } else {
      this.showItem("spell-add-dictionaries-main", false);
    }
  },

  initClipboardItems() {
    // Copy depends on whether there is selected text.
    // Enabling this context menu item is now done through the global
    // command updating system
    // this.setItemAttr( "context-copy", "disabled", !this.isTextSelected() );
    goUpdateGlobalEditMenuItems();

    this.showItem("context-undo", this.onTextInput);
    this.showItem("context-sep-undo", this.onTextInput);
    this.showItem("context-cut", this.onTextInput);
    this.showItem("context-copy", this.isContentSelected || this.onTextInput);
    this.showItem("context-paste", this.onTextInput);
    this.showItem("context-delete", this.onTextInput);
    this.showItem("context-sep-paste", this.onTextInput);
    this.showItem(
      "context-selectall",
      !(
        this.onLink ||
        this.onImage ||
        this.onVideo ||
        this.onAudio ||
        this.inSyntheticDoc
      ) || this.isDesignMode
    );
    this.showItem(
      "context-sep-selectall",
      !this.inAboutDevtoolsToolbox && this.isContentSelected
    );

    // XXX dr
    // ------
    // nsDocumentViewer.cpp has code to determine whether we're
    // on a link or an image. we really ought to be using that...

    // Copy email link depends on whether we're on an email link.
    this.showItem("context-copyemail", this.onMailtoLink);

    // Copy link location depends on whether we're on a non-mailto link.
    this.showItem("context-copylink", this.onLink && !this.onMailtoLink);
    this.showItem(
      "context-sep-copylink",
      this.onLink && (this.onImage || this.onVideo || this.onAudio)
    );

    // Copy image contents depends on whether we're on an image.
    // Note: the element doesn't exist on all platforms, but showItem() takes
    // care of that by itself.
    this.showItem("context-copyimage-contents", this.onImage);

    // Copy image location depends on whether we're on an image.
    this.showItem("context-copyimage", this.onImage);
    this.showItem("context-copyvideourl", this.onVideo);
    this.showItem("context-copyaudiourl", this.onAudio);
    this.setItemAttr("context-copyvideourl", "disabled", !this.mediaURL);
    this.setItemAttr("context-copyaudiourl", "disabled", !this.mediaURL);
    this.showItem(
      "context-sep-copyimage",
      this.onImage || this.onVideo || this.onAudio
    );
  },

  initMediaPlayerItems() {
    var onMedia = this.onVideo || this.onAudio;
    // Several mutually exclusive items... play/pause, mute/unmute, show/hide
    this.showItem(
      "context-media-play",
      onMedia && (this.target.paused || this.target.ended)
    );
    this.showItem(
      "context-media-pause",
      onMedia && !this.target.paused && !this.target.ended
    );
    this.showItem("context-media-mute", onMedia && !this.target.muted);
    this.showItem("context-media-unmute", onMedia && this.target.muted);
    this.showItem(
      "context-media-playbackrate",
      onMedia && this.target.duration != Number.POSITIVE_INFINITY
    );
    this.showItem("context-media-loop", onMedia);
    this.showItem(
      "context-media-showcontrols",
      onMedia && !this.target.controls
    );
    this.showItem(
      "context-media-hidecontrols",
      this.target.controls &&
        (this.onVideo || (this.onAudio && !this.inSyntheticDoc))
    );
    this.showItem(
      "context-video-fullscreen",
      this.onVideo && !this.target.ownerDocument.fullscreen
    );
    {
      let shouldDisplay =
        Services.prefs.getBoolPref(
          "media.videocontrols.picture-in-picture.enabled"
        ) &&
        this.onVideo &&
        !this.target.ownerDocument.fullscreen;
      this.showItem("context-video-pictureinpicture", shouldDisplay);
    }
    this.showItem("context-media-eme-learnmore", this.onDRMMedia);
    this.showItem("context-media-eme-separator", this.onDRMMedia);

    // Disable them when there isn't a valid media source loaded.
    if (onMedia) {
      this.setItemAttr(
        "context-media-playbackrate-050x",
        "checked",
        this.target.playbackRate == 0.5
      );
      this.setItemAttr(
        "context-media-playbackrate-100x",
        "checked",
        this.target.playbackRate == 1.0
      );
      this.setItemAttr(
        "context-media-playbackrate-125x",
        "checked",
        this.target.playbackRate == 1.25
      );
      this.setItemAttr(
        "context-media-playbackrate-150x",
        "checked",
        this.target.playbackRate == 1.5
      );
      this.setItemAttr(
        "context-media-playbackrate-200x",
        "checked",
        this.target.playbackRate == 2.0
      );
      this.setItemAttr("context-media-loop", "checked", this.target.loop);
      var hasError =
        this.target.error != null ||
        this.target.networkState == this.target.NETWORK_NO_SOURCE;
      this.setItemAttr("context-media-play", "disabled", hasError);
      this.setItemAttr("context-media-pause", "disabled", hasError);
      this.setItemAttr("context-media-mute", "disabled", hasError);
      this.setItemAttr("context-media-unmute", "disabled", hasError);
      this.setItemAttr("context-media-playbackrate", "disabled", hasError);
      this.setItemAttr("context-media-playbackrate-050x", "disabled", hasError);
      this.setItemAttr("context-media-playbackrate-100x", "disabled", hasError);
      this.setItemAttr("context-media-playbackrate-125x", "disabled", hasError);
      this.setItemAttr("context-media-playbackrate-150x", "disabled", hasError);
      this.setItemAttr("context-media-playbackrate-200x", "disabled", hasError);
      this.setItemAttr("context-media-showcontrols", "disabled", hasError);
      this.setItemAttr("context-media-hidecontrols", "disabled", hasError);
      if (this.onVideo) {
        let canSaveSnapshot =
          !this.onDRMMedia &&
          this.target.readyState >= this.target.HAVE_CURRENT_DATA;
        this.setItemAttr(
          "context-video-saveimage",
          "disabled",
          !canSaveSnapshot
        );
        this.setItemAttr("context-video-fullscreen", "disabled", hasError);
        this.setItemAttr(
          "context-video-pictureinpicture",
          "checked",
          this.onPiPVideo
        );
        this.setItemAttr(
          "context-video-pictureinpicture",
          "disabled",
          !this.onPiPVideo && hasError
        );
      }
    }
    this.showItem("context-media-sep-commands", onMedia);
  },

  initClickToPlayItems() {
    this.showItem("context-ctp-play", this.onCTPPlugin);
    this.showItem("context-ctp-hide", this.onCTPPlugin);
    this.showItem("context-sep-ctp", this.onCTPPlugin);
  },

  initPasswordManagerItems() {
    let loginFillInfo =
      gContextMenuContentData && gContextMenuContentData.loginFillInfo;
    let documentURI = gContextMenuContentData.documentURIObject;

    // If we could not find a password field we
    // don't want to show the form fill option.
    let showFill =
      loginFillInfo &&
      loginFillInfo.passwordField.found &&
      !documentURI.schemeIs("about");

    // Disable the fill option if the user hasn't unlocked with their master password
    // or if the password field or target field are disabled.
    // XXX: Bug 1529025 to respect signon.rememberSignons
    let disableFill =
      !loginFillInfo ||
      !Services.logins ||
      !Services.logins.isLoggedIn ||
      loginFillInfo.passwordField.disabled ||
      (!this.onPassword && loginFillInfo.usernameField.disabled);

    this.showItem("fill-login-separator", showFill);
    this.showItem("fill-login", showFill);
    this.setItemAttr("fill-login", "disabled", disableFill);

    // Set the correct label for the fill menu
    let fillMenu = document.getElementById("fill-login");
    if (this.onPassword) {
      fillMenu.setAttribute("label", fillMenu.getAttribute("label-password"));
      fillMenu.setAttribute(
        "accesskey",
        fillMenu.getAttribute("accesskey-password")
      );
    } else {
      fillMenu.setAttribute("label", fillMenu.getAttribute("label-login"));
      fillMenu.setAttribute(
        "accesskey",
        fillMenu.getAttribute("accesskey-login")
      );
    }

    if (!showFill || disableFill) {
      return;
    }

    let formOrigin = LoginHelper.getLoginOrigin(documentURI.spec);
    let fragment = LoginManagerContextMenu.addLoginsToMenu(
      this.targetIdentifier,
      this.browser,
      formOrigin
    );
    let isGeneratedPasswordEnabled =
      LoginHelper.generationAvailable && LoginHelper.generationEnabled;
    let canFillGeneratedPassword =
      this.onPassword &&
      isGeneratedPasswordEnabled &&
      Services.logins.getLoginSavingEnabled(formOrigin);

    this.showItem("fill-login-no-logins", !fragment);
    this.showItem("fill-login-generated-password", canFillGeneratedPassword);
    this.showItem("generated-password-separator", canFillGeneratedPassword);

    this.setItemAttr(
      "fill-login-generated-password",
      "disabled",
      PrivateBrowsingUtils.isWindowPrivate(window)
    );

    if (!fragment) {
      return;
    }
    let popup = document.getElementById("fill-login-popup");
    let insertBeforeElement = document.getElementById("fill-login-no-logins");
    popup.insertBefore(fragment, insertBeforeElement);
  },

  initSyncItems() {
    gSync.updateContentContextMenu(this);
  },

  openPasswordManager() {
    LoginHelper.openPasswordManager(window, {
      filterString: gContextMenuContentData.documentURIObject.host,
      entryPoint: "contextmenu",
    });
  },

  fillGeneratedPassword() {
    LoginManagerContextMenu.fillGeneratedPassword(
      this.targetIdentifier,
      gContextMenuContentData.documentURIObject,
      this.browser
    );
  },

  inspectNode() {
    return DevToolsShim.inspectNode(gBrowser.selectedTab, this.targetSelectors);
  },

  inspectA11Y() {
    return DevToolsShim.inspectA11Y(gBrowser.selectedTab, this.targetSelectors);
  },

  _openLinkInParameters(extra) {
    let params = {
      charset: gContextMenuContentData.charSet,
      originPrincipal: this.principal,
      originStoragePrincipal: this.storagePrincipal,
      triggeringPrincipal: this.principal,
      csp: this.csp,
      frameOuterWindowID: gContextMenuContentData.frameOuterWindowID,
    };
    for (let p in extra) {
      params[p] = extra[p];
    }

    let referrerInfo = this.onLink
      ? gContextMenuContentData.linkReferrerInfo
      : gContextMenuContentData.referrerInfo;
    // If we want to change userContextId, we must be sure that we don't
    // propagate the referrer.
    if (
      ("userContextId" in params &&
        params.userContextId != gContextMenuContentData.userContextId) ||
      this.onPlainTextLink
    ) {
      referrerInfo = new ReferrerInfo(
        referrerInfo.referrerPolicy,
        false,
        referrerInfo.originalReferrer
      );
    }

    params.referrerInfo = referrerInfo;
    return params;
  },

  // Open linked-to URL in a new window.
  openLink() {
    openLinkIn(this.linkURL, "window", this._openLinkInParameters());
  },

  // Open linked-to URL in a new private window.
  openLinkInPrivateWindow() {
    openLinkIn(
      this.linkURL,
      "window",
      this._openLinkInParameters({ private: true })
    );
  },

  // Open linked-to URL in a new tab.
  openLinkInTab(event) {
    let referrerURI = gContextMenuContentData.documentURIObject;

    // if its parent allows mixed content and the referring URI passes
    // a same origin check with the target URI, we can preserve the users
    // decision of disabling MCB on a page for it's child tabs.
    let persistAllowMixedContentInChildTab = false;

    if (gContextMenuContentData.parentAllowsMixedContent) {
      const sm = Services.scriptSecurityManager;
      try {
        let targetURI = this.linkURI;
        let isPrivateWin =
          this.browser.contentPrincipal.originAttributes.privateBrowsingId > 0;
        sm.checkSameOriginURI(referrerURI, targetURI, false, isPrivateWin);
        persistAllowMixedContentInChildTab = true;
      } catch (e) {}
    }

    let params = {
      allowMixedContent: persistAllowMixedContentInChildTab,
      userContextId: parseInt(event.target.getAttribute("data-usercontextid")),
    };

    openLinkIn(this.linkURL, "tab", this._openLinkInParameters(params));
  },

  // open URL in current tab
  openLinkInCurrent() {
    openLinkIn(this.linkURL, "current", this._openLinkInParameters());
  },

  // Open frame in a new tab.
  openFrameInTab() {
    openLinkIn(gContextMenuContentData.docLocation, "tab", {
      charset: gContextMenuContentData.charSet,
      triggeringPrincipal: this.browser.contentPrincipal,
      csp: this.browser.csp,
      referrerInfo: gContextMenuContentData.frameReferrerInfo,
    });
  },

  // Reload clicked-in frame.
  reloadFrame(aEvent) {
    let forceReload = aEvent.shiftKey;
    this.actor.reloadFrame(this.targetIdentifier, forceReload);
  },

  // Open clicked-in frame in its own window.
  openFrame() {
    openLinkIn(gContextMenuContentData.docLocation, "window", {
      charset: gContextMenuContentData.charSet,
      triggeringPrincipal: this.browser.contentPrincipal,
      csp: this.browser.csp,
      referrerInfo: gContextMenuContentData.frameReferrerInfo,
    });
  },

  // Open clicked-in frame in the same window.
  showOnlyThisFrame() {
    urlSecurityCheck(
      gContextMenuContentData.docLocation,
      this.browser.contentPrincipal,
      Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT
    );
    openWebLinkIn(gContextMenuContentData.docLocation, "current", {
      referrerInfo: gContextMenuContentData.frameReferrerInfo,
      triggeringPrincipal: this.browser.contentPrincipal,
    });
  },

  // View Partial Source
  viewPartialSource() {
    let { browser } = this;
    let openSelectionFn = function() {
      let tabBrowser = gBrowser;
      const inNewWindow = !Services.prefs.getBoolPref("view_source.tab");
      // In the case of popups, we need to find a non-popup browser window.
      // We might also not have a tabBrowser reference (if this isn't in a
      // a tabbrowser scope) or might have a fake/stub tabbrowser reference
      // (in the sidebar). Deal with those cases:
      if (!tabBrowser || !tabBrowser.loadOneTab || !window.toolbar.visible) {
        // This returns only non-popup browser windows by default.
        let browserWindow = BrowserWindowTracker.getTopWindow();
        tabBrowser = browserWindow.gBrowser;
      }
      let relatedToCurrent = gBrowser && gBrowser.selectedBrowser == browser;
      let tab = tabBrowser.loadOneTab("about:blank", {
        relatedToCurrent,
        inBackground: inNewWindow,
        skipAnimation: inNewWindow,
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });
      const viewSourceBrowser = tabBrowser.getBrowserForTab(tab);
      if (inNewWindow) {
        tabBrowser.hideTab(tab);
        tabBrowser.replaceTabsWithWindow(tab);
      }
      return viewSourceBrowser;
    };

    top.gViewSourceUtils.viewPartialSourceInBrowser(browser, openSelectionFn);
  },

  // Open new "view source" window with the frame's URL.
  viewFrameSource() {
    BrowserViewSourceOfDocument({
      browser: this.browser,
      URL: gContextMenuContentData.docLocation,
      outerWindowID: this.frameOuterWindowID,
    });
  },

  viewInfo() {
    BrowserPageInfo(
      gContextMenuContentData.docLocation,
      null,
      null,
      null,
      this.browser
    );
  },

  viewImageInfo() {
    BrowserPageInfo(
      gContextMenuContentData.docLocation,
      "mediaTab",
      this.imageInfo,
      null,
      this.browser
    );
  },

  viewImageDesc(e) {
    urlSecurityCheck(
      this.imageDescURL,
      this.principal,
      Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT
    );
    openUILink(this.imageDescURL, e, {
      referrerInfo: gContextMenuContentData.referrerInfo,
      triggeringPrincipal: this.principal,
      csp: this.csp,
    });
  },

  viewFrameInfo() {
    BrowserPageInfo(
      gContextMenuContentData.docLocation,
      null,
      null,
      this.frameOuterWindowID,
      this.browser
    );
  },

  reloadImage() {
    urlSecurityCheck(
      this.mediaURL,
      this.principal,
      Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT
    );
    this.actor.reloadImage(this.targetIdentifier);
  },

  _canvasToBlobURL(targetIdentifier) {
    return this.actor.canvasToBlobURL(targetIdentifier);
  },

  // Change current window to the URL of the image, video, or audio.
  viewMedia(e) {
    let referrerInfo = gContextMenuContentData.referrerInfo;
    let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
    if (this.onCanvas) {
      this._canvasToBlobURL(this.targetIdentifier).then(function(blobURL) {
        openUILink(blobURL, e, {
          referrerInfo,
          triggeringPrincipal: systemPrincipal,
        });
      }, Cu.reportError);
    } else {
      urlSecurityCheck(
        this.mediaURL,
        this.principal,
        Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT
      );
      openUILink(this.mediaURL, e, {
        referrerInfo,
        forceAllowDataURI: true,
        triggeringPrincipal: this.principal,
        csp: this.csp,
      });
    }
  },

  saveVideoFrameAsImage() {
    let isPrivate = PrivateBrowsingUtils.isBrowserPrivate(this.browser);

    let name = "";
    if (this.mediaURL) {
      try {
        let uri = makeURI(this.mediaURL);
        let url = uri.QueryInterface(Ci.nsIURL);
        if (url.fileBaseName) {
          name = decodeURI(url.fileBaseName) + ".jpg";
        }
      } catch (e) {}
    }
    if (!name) {
      name = "snapshot.jpg";
    }

    // Cache this because we fetch the data async
    let referrerInfo = gContextMenuContentData.referrerInfo;

    this.actor.saveVideoFrameAsImage(this.targetIdentifier).then(dataURL => {
      // FIXME can we switch this to a blob URL?
      saveImageURL(
        dataURL,
        name,
        "SaveImageTitle",
        true, // bypass cache
        false, // don't skip prompt for where to save
        referrerInfo, // referrer info
        null, // document
        null, // content type
        null, // content disposition
        isPrivate,
        this.principal
      );
    });
  },

  leaveDOMFullScreen() {
    document.exitFullscreen();
  },

  // Change current window to the URL of the background image.
  viewBGImage(e) {
    urlSecurityCheck(
      this.bgImageURL,
      this.principal,
      Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT
    );

    openUILink(this.bgImageURL, e, {
      referrerInfo: gContextMenuContentData.referrerInfo,
      triggeringPrincipal: this.principal,
      csp: this.csp,
    });
  },

  setDesktopBackground() {
    if (!Services.policies.isAllowed("setDesktopBackground")) {
      return;
    }

    this.actor
      .setAsDesktopBackground(this.targetIdentifier)
      .then(({ failed, dataURL, imageName }) => {
        if (failed) {
          return;
        }

        let image = document.createElementNS(
          "http://www.w3.org/1999/xhtml",
          "img"
        );
        image.src = dataURL;

        // Confirm since it's annoying if you hit this accidentally.
        const kDesktopBackgroundURL =
          "chrome://browser/content/setDesktopBackground.xul";

        if (AppConstants.platform == "macosx") {
          // On Mac, the Set Desktop Background window is not modal.
          // Don't open more than one Set Desktop Background window.
          let dbWin = Services.wm.getMostRecentWindow(
            "Shell:SetDesktopBackground"
          );
          if (dbWin) {
            dbWin.gSetBackground.init(image, imageName);
            dbWin.focus();
          } else {
            openDialog(
              kDesktopBackgroundURL,
              "",
              "centerscreen,chrome,dialog=no,dependent,resizable=no",
              image,
              imageName
            );
          }
        } else {
          // On non-Mac platforms, the Set Wallpaper dialog is modal.
          openDialog(
            kDesktopBackgroundURL,
            "",
            "centerscreen,chrome,dialog,modal,dependent",
            image,
            imageName
          );
        }
      });
  },

  // Save URL of clicked-on frame.
  saveFrame() {
    saveBrowser(this.browser, false, this.frameOuterWindowID);
  },

  // Helper function to wait for appropriate MIME-type headers and
  // then prompt the user with a file picker
  saveHelper(
    linkURL,
    linkText,
    dialogTitle,
    bypassCache,
    doc,
    referrerInfo,
    windowID,
    linkDownload,
    isContentWindowPrivate
  ) {
    // canonical def in nsURILoader.h
    const NS_ERROR_SAVE_LINK_AS_TIMEOUT = 0x805d0020;

    // an object to proxy the data through to
    // nsIExternalHelperAppService.doContent, which will wait for the
    // appropriate MIME-type headers and then prompt the user with a
    // file picker
    function saveAsListener(principal) {
      this._triggeringPrincipal = principal;
    }
    saveAsListener.prototype = {
      extListener: null,

      onStartRequest: function saveLinkAs_onStartRequest(aRequest) {
        // if the timer fired, the error status will have been caused by that,
        // and we'll be restarting in onStopRequest, so no reason to notify
        // the user
        if (aRequest.status == NS_ERROR_SAVE_LINK_AS_TIMEOUT) {
          return;
        }

        timer.cancel();

        // some other error occured; notify the user...
        if (!Components.isSuccessCode(aRequest.status)) {
          try {
            const bundle = Services.strings.createBundle(
              "chrome://mozapps/locale/downloads/downloads.properties"
            );

            const title = bundle.GetStringFromName("downloadErrorAlertTitle");
            const msg = bundle.GetStringFromName("downloadErrorGeneric");

            let window = Services.wm.getOuterWindowWithId(windowID);
            Services.prompt.alert(window, title, msg);
          } catch (ex) {}
          return;
        }

        let extHelperAppSvc = Cc[
          "@mozilla.org/uriloader/external-helper-app-service;1"
        ].getService(Ci.nsIExternalHelperAppService);
        let channel = aRequest.QueryInterface(Ci.nsIChannel);
        this.extListener = extHelperAppSvc.doContent(
          channel.contentType,
          aRequest,
          null,
          true,
          window
        );
        this.extListener.onStartRequest(aRequest);
      },

      onStopRequest: function saveLinkAs_onStopRequest(aRequest, aStatusCode) {
        if (aStatusCode == NS_ERROR_SAVE_LINK_AS_TIMEOUT) {
          // do it the old fashioned way, which will pick the best filename
          // it can without waiting.
          saveURL(
            linkURL,
            linkText,
            dialogTitle,
            bypassCache,
            false,
            referrerInfo,
            doc,
            isContentWindowPrivate,
            this._triggeringPrincipal
          );
        }
        if (this.extListener) {
          this.extListener.onStopRequest(aRequest, aStatusCode);
        }
      },

      onDataAvailable: function saveLinkAs_onDataAvailable(
        aRequest,
        aInputStream,
        aOffset,
        aCount
      ) {
        this.extListener.onDataAvailable(
          aRequest,
          aInputStream,
          aOffset,
          aCount
        );
      },
    };

    function callbacks() {}
    callbacks.prototype = {
      getInterface: function sLA_callbacks_getInterface(aIID) {
        if (aIID.equals(Ci.nsIAuthPrompt) || aIID.equals(Ci.nsIAuthPrompt2)) {
          // If the channel demands authentication prompt, we must cancel it
          // because the save-as-timer would expire and cancel the channel
          // before we get credentials from user.  Both authentication dialog
          // and save as dialog would appear on the screen as we fall back to
          // the old fashioned way after the timeout.
          timer.cancel();
          channel.cancel(NS_ERROR_SAVE_LINK_AS_TIMEOUT);
        }
        throw Cr.NS_ERROR_NO_INTERFACE;
      },
    };

    // if it we don't have the headers after a short time, the user
    // won't have received any feedback from their click.  that's bad.  so
    // we give up waiting for the filename.
    function timerCallback() {}
    timerCallback.prototype = {
      notify: function sLA_timer_notify(aTimer) {
        channel.cancel(NS_ERROR_SAVE_LINK_AS_TIMEOUT);
      },
    };

    // setting up a new channel for 'right click - save link as ...'
    var channel = NetUtil.newChannel({
      uri: makeURI(linkURL),
      loadingPrincipal: this.principal,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_SAVEAS_DOWNLOAD,
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS,
    });

    if (linkDownload) {
      channel.contentDispositionFilename = linkDownload;
    }
    if (channel instanceof Ci.nsIPrivateBrowsingChannel) {
      let docIsPrivate = PrivateBrowsingUtils.isBrowserPrivate(this.browser);
      channel.setPrivate(docIsPrivate);
    }
    channel.notificationCallbacks = new callbacks();

    let flags = Ci.nsIChannel.LOAD_CALL_CONTENT_SNIFFERS;

    if (bypassCache) {
      flags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
    }

    if (channel instanceof Ci.nsICachingChannel) {
      flags |= Ci.nsICachingChannel.LOAD_BYPASS_LOCAL_CACHE_IF_BUSY;
    }

    channel.loadFlags |= flags;

    if (channel instanceof Ci.nsIHttpChannel) {
      channel.referrerInfo = referrerInfo;
      if (channel instanceof Ci.nsIHttpChannelInternal) {
        channel.forceAllowThirdPartyCookie = true;
      }
    }

    // fallback to the old way if we don't see the headers quickly
    var timeToWait = Services.prefs.getIntPref(
      "browser.download.saveLinkAsFilenameTimeout"
    );
    var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(
      new timerCallback(),
      timeToWait,
      timer.TYPE_ONE_SHOT
    );

    // kick off the channel with our proxy object as the listener
    channel.asyncOpen(new saveAsListener(this.principal));
  },

  // Save URL of clicked-on link.
  saveLink() {
    let referrerInfo = this.onLink
      ? gContextMenuContentData.linkReferrerInfo
      : gContextMenuContentData.referrerInfo;

    let isContentWindowPrivate = this.ownerDoc.isPrivate;
    this.saveHelper(
      this.linkURL,
      this.linkTextStr,
      null,
      true,
      this.ownerDoc,
      referrerInfo,
      this.frameOuterWindowID,
      this.linkDownload,
      isContentWindowPrivate
    );
  },

  // Backwards-compatibility wrapper
  saveImage() {
    if (this.onCanvas || this.onImage) {
      this.saveMedia();
    }
  },

  // Save URL of the clicked upon image, video, or audio.
  saveMedia() {
    let doc = this.ownerDoc;
    let isContentWindowPrivate = this.ownerDoc.isPrivate;
    let referrerInfo = gContextMenuContentData.referrerInfo;
    let isPrivate = PrivateBrowsingUtils.isBrowserPrivate(this.browser);
    if (this.onCanvas) {
      // Bypass cache, since it's a data: URL.
      this._canvasToBlobURL(this.targetIdentifier).then(function(blobURL) {
        saveImageURL(
          blobURL,
          "canvas.png",
          "SaveImageTitle",
          true,
          false,
          referrerInfo,
          null,
          null,
          null,
          isPrivate,
          document.nodePrincipal /* system, because blob: */
        );
      }, Cu.reportError);
    } else if (this.onImage) {
      urlSecurityCheck(this.mediaURL, this.principal);
      saveImageURL(
        this.mediaURL,
        null,
        "SaveImageTitle",
        false,
        false,
        referrerInfo,
        null,
        gContextMenuContentData.contentType,
        gContextMenuContentData.contentDisposition,
        isPrivate,
        this.principal
      );
    } else if (this.onVideo || this.onAudio) {
      var dialogTitle = this.onVideo ? "SaveVideoTitle" : "SaveAudioTitle";
      this.saveHelper(
        this.mediaURL,
        null,
        dialogTitle,
        false,
        doc,
        referrerInfo,
        this.frameOuterWindowID,
        "",
        isContentWindowPrivate
      );
    }
  },

  // Backwards-compatibility wrapper
  sendImage() {
    if (this.onCanvas || this.onImage) {
      this.sendMedia();
    }
  },

  sendMedia() {
    MailIntegration.sendMessage(this.mediaURL, "");
  },

  playPlugin() {
    this.actor.pluginCommand("play", this.targetIdentifier);
  },

  hidePlugin() {
    this.actor.pluginCommand("hide", this.targetIdentifier);
  },

  // Generate email address and put it on clipboard.
  copyEmail() {
    // Copy the comma-separated list of email addresses only.
    // There are other ways of embedding email addresses in a mailto:
    // link, but such complex parsing is beyond us.
    var url = this.linkURL;
    var qmark = url.indexOf("?");
    var addresses;

    // 7 == length of "mailto:"
    addresses = qmark > 7 ? url.substring(7, qmark) : url.substr(7);

    // Let's try to unescape it using a character set
    // in case the address is not ASCII.
    try {
      addresses = Services.textToSubURI.unEscapeURIForUI(
        gContextMenuContentData.charSet,
        addresses
      );
    } catch (ex) {
      // Do nothing.
    }

    var clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(
      Ci.nsIClipboardHelper
    );
    clipboard.copyString(addresses);
  },

  copyLink() {
    // If we're in a view source tab, remove the view-source: prefix
    let linkURL = this.linkURL.replace(/^view-source:/, "");
    var clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(
      Ci.nsIClipboardHelper
    );
    clipboard.copyString(linkURL);
  },

  addKeywordForSearchField() {
    this.actor.getSearchFieldBookmarkData(this.targetIdentifier).then(data => {
      let title = gNavigatorBundle.getFormattedString(
        "addKeywordTitleAutoFill",
        [data.title]
      );
      PlacesUIUtils.showBookmarkDialog(
        {
          action: "add",
          type: "bookmark",
          uri: makeURI(data.spec),
          title,
          keyword: "",
          postData: data.postData,
          charSet: data.charset,
          hiddenRows: ["location", "tags"],
        },
        window
      );
    });
  },

  /**
   * Utilities
   */

  /**
   * Show/hide one item (specified via name or the item element itself).
   * If the element is not found, then this function finishes silently.
   *
   * @param {Element|String} aItemOrId The item element or the name of the element
   *                                   to show.
   * @param {Boolean} aShow Set to true to show the item, false to hide it.
   */
  showItem(aItemOrId, aShow) {
    var item =
      aItemOrId.constructor == String
        ? document.getElementById(aItemOrId)
        : aItemOrId;
    if (item) {
      item.hidden = !aShow;
    }
  },

  // Set given attribute of specified context-menu item.  If the
  // value is null, then it removes the attribute (which works
  // nicely for the disabled attribute).
  setItemAttr(aID, aAttr, aVal) {
    var elem = document.getElementById(aID);
    if (elem) {
      if (aVal == null) {
        // null indicates attr should be removed.
        elem.removeAttribute(aAttr);
      } else {
        // Set attr=val.
        elem.setAttribute(aAttr, aVal);
      }
    }
  },

  // Temporary workaround for DOM api not yet implemented by XUL nodes.
  cloneNode(aItem) {
    // Create another element like the one we're cloning.
    var node = document.createElement(aItem.tagName);

    // Copy attributes from argument item to the new one.
    var attrs = aItem.attributes;
    for (var i = 0; i < attrs.length; i++) {
      var attr = attrs.item(i);
      node.setAttribute(attr.nodeName, attr.nodeValue);
    }

    // Voila!
    return node;
  },

  getLinkURI() {
    try {
      return makeURI(this.linkURL);
    } catch (ex) {
      // e.g. empty URL string
    }

    return null;
  },

  // Kept for addon compat
  linkText() {
    return this.linkTextStr;
  },

  // Determines whether or not the separator with the specified ID should be
  // shown or not by determining if there are any non-hidden items between it
  // and the previous separator.
  shouldShowSeparator(aSeparatorID) {
    var separator = document.getElementById(aSeparatorID);
    if (separator) {
      var sibling = separator.previousSibling;
      while (sibling && sibling.localName != "menuseparator") {
        if (!sibling.hidden) {
          return true;
        }
        sibling = sibling.previousSibling;
      }
    }
    return false;
  },

  addDictionaries() {
    var uri = formatURL("browser.dictionaries.download.url", true);

    var locale = "-";
    try {
      locale = Services.prefs.getComplexValue(
        "intl.accept_languages",
        Ci.nsIPrefLocalizedString
      ).data;
    } catch (e) {}

    var version = "-";
    try {
      version = Services.appinfo.version;
    } catch (e) {}

    uri = uri.replace(/%LOCALE%/, escape(locale)).replace(/%VERSION%/, version);

    var newWindowPref = Services.prefs.getIntPref(
      "browser.link.open_newwindow"
    );
    var where = newWindowPref == 3 ? "tab" : "window";

    openTrustedLinkIn(uri, where);
  },

  bookmarkThisPage: function CM_bookmarkThisPage() {
    window.top.PlacesCommandHook.bookmarkPage().catch(Cu.reportError);
  },

  bookmarkLink: function CM_bookmarkLink() {
    window.top.PlacesCommandHook.bookmarkLink(
      this.linkURL,
      this.linkTextStr
    ).catch(Cu.reportError);
  },

  addBookmarkForFrame: function CM_addBookmarkForFrame() {
    let uri = gContextMenuContentData.documentURIObject;

    this.actor.getFrameTitle(this.targetIdentifier).then(title => {
      window.top.PlacesCommandHook.bookmarkLink(uri.spec, title).catch(
        Cu.reportError
      );
    });
  },

  savePageAs: function CM_savePageAs() {
    saveBrowser(this.browser);
  },

  printFrame: function CM_printFrame() {
    PrintUtils.printWindow(this.frameOuterWindowID, this.browser);
  },

  switchPageDirection: function CM_switchPageDirection() {
    gBrowser.selectedBrowser.sendMessageToActor(
      "SwitchDocumentDirection",
      {},
      "SwitchDocumentDirection",
      true
    );
  },

  mediaCommand: function CM_mediaCommand(command, data) {
    this.actor.mediaCommand(this.targetIdentifier, command, data);
  },

  copyMediaLocation() {
    var clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(
      Ci.nsIClipboardHelper
    );
    clipboard.copyString(this.mediaURL);
  },

  drmLearnMore(aEvent) {
    let drmInfoURL =
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "drm-content";
    let dest = whereToOpenLink(aEvent);
    // Don't ever want this to open in the same tab as it'll unload the
    // DRM'd video, which is going to be a bad idea in most cases.
    if (dest == "current") {
      dest = "tab";
    }
    openTrustedLinkIn(drmInfoURL, dest);
  },

  get imageURL() {
    if (this.onImage) {
      return this.mediaURL;
    }
    return "";
  },

  // Formats the 'Search <engine> for "<selection or link text>"' context menu.
  formatSearchContextItem() {
    var menuItem = document.getElementById("context-searchselect");
    let selectedText = this.isTextSelected
      ? this.textSelected
      : this.linkTextStr;

    // Store searchTerms in context menu item so we know what to search onclick
    menuItem.searchTerms = selectedText;
    menuItem.principal = this.principal;
    menuItem.csp = this.csp;

    // Copied to alert.js' prefillAlertInfo().
    // If the JS character after our truncation point is a trail surrogate,
    // include it in the truncated string to avoid splitting a surrogate pair.
    if (selectedText.length > 15) {
      let truncLength = 15;
      let truncChar = selectedText[15].charCodeAt(0);
      if (truncChar >= 0xdc00 && truncChar <= 0xdfff) {
        truncLength++;
      }
      selectedText = selectedText.substr(0, truncLength) + this.ellipsis;
    }

    // format "Search <engine> for <selection>" string to show in menu
    const docIsPrivate = PrivateBrowsingUtils.isBrowserPrivate(this.browser);
    let engineName =
      Services.search[docIsPrivate ? "defaultPrivateEngine" : "defaultEngine"]
        .name;
    menuItem.usePrivate = docIsPrivate;
    var menuLabel = gNavigatorBundle.getFormattedString("contextMenuSearch", [
      engineName,
      selectedText,
    ]);
    menuItem.label = menuLabel;
    menuItem.accessKey = gNavigatorBundle.getString(
      "contextMenuSearch.accesskey"
    );
  },

  createContainerMenu(aEvent) {
    let createMenuOptions = {
      isContextMenu: true,
      excludeUserContextId: gContextMenuContentData.userContextId,
    };
    return createUserContextMenu(aEvent, createMenuOptions);
  },

  doCustomCommand(generatedItemId, handlingUserInput) {
    this.actor.doCustomCommand(generatedItemId, handlingUserInput);
  },
};
