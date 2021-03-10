/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PASSWORD_FIELDNAME_HINTS = ["current-password", "new-password"];

function openContextMenu(aMessage, aBrowser, aActor) {
  if (BrowserHandler.kiosk) {
    // Don't display context menus in kiosk mode
    return;
  }
  let data = aMessage.data;
  let browser = aBrowser;
  let actor = aActor;
  let spellInfo = data.spellInfo;
  let frameReferrerInfo = data.frameReferrerInfo;
  let linkReferrerInfo = data.linkReferrerInfo;
  let principal = data.principal;
  let storagePrincipal = data.storagePrincipal;

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

  nsContextMenu.contentData = {
    context: data.context,
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
    frameID: data.frameID,
    frameOuterWindowID: data.frameID,
    frameBrowsingContext: BrowsingContext.get(data.frameBrowsingContextID),
    selectionInfo: data.selectionInfo,
    disableSetDesktopBackground: data.disableSetDesktopBackground,
    loginFillInfo: data.loginFillInfo,
    parentAllowsMixedContent: data.parentAllowsMixedContent,
    userContextId: data.userContextId,
    webExtContextData: data.webExtContextData,
    cookieJarSettings: E10SUtils.deserializeCookieJarSettings(
      data.cookieJarSettings
    ),
  };

  let popup = browser.ownerDocument.getElementById("contentAreaContextMenu");
  let context = nsContextMenu.contentData.context;

  // We don't have access to the original event here, as that happened in
  // another process. Therefore we synthesize a new MouseEvent to propagate the
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

class nsContextMenu {
  constructor(aXulMenu, aIsShift) {
    // Get contextual info.
    this.setContext();

    if (!this.shouldDisplay) {
      return;
    }

    this.hasPageMenu = false;
    this.isContentSelected = !this.selectionInfo.docSelectionIsCollapsed;
    if (!aIsShift) {
      this.hasPageMenu = PageMenuParent.addToPopup(
        this.contentData.customMenuItems,
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
        frameUrl: this.contentData ? this.contentData.docLocation : undefined,
        pageUrl: this.browser ? this.browser.currentURI.spec : undefined,
        linkText: this.linkTextStr,
        linkUrl: this.linkURL,
        selectionText: this.isTextSelected
          ? this.selectionInfo.fullText
          : undefined,
        frameId: this.frameID,
        webExtBrowserType: this.webExtBrowserType,
        webExtContextData: this.contentData
          ? this.contentData.webExtContextData
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

    // Initialize (disable/remove) menu items.
    this.initItems(aXulMenu);
  }

  setContext() {
    let context = Object.create(null);

    if (nsContextMenu.contentData) {
      this.contentData = nsContextMenu.contentData;
      context = this.contentData.context;
      nsContextMenu.contentData = null;
    }

    this.shouldDisplay = context.shouldDisplay;
    this.timeStamp = context.timeStamp;

    // Assign what's _possibly_ needed from `context` sent by ContextMenuChild.jsm
    // Keep this consistent with the similar code in ContextMenu's _setContext
    this.imageDescURL = context.imageDescURL;
    this.imageInfo = context.imageInfo;
    this.mediaURL = context.mediaURL || context.bgImageURL;
    this.webExtBrowserType = context.webExtBrowserType;

    this.canSpellCheck = context.canSpellCheck;
    this.hasBGImage = context.hasBGImage;
    this.hasMultipleBGImages = context.hasMultipleBGImages;
    this.isDesignMode = context.isDesignMode;
    this.inFrame = context.inFrame;
    this.inPDFViewer = context.inPDFViewer;
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
    this.frameID = context.frameID;
    this.frameOuterWindowID = context.frameOuterWindowID;
    this.frameBrowsingContext = BrowsingContext.get(
      context.frameBrowsingContextID
    );

    this.inSyntheticDoc = context.inSyntheticDoc;
    this.inAboutDevtoolsToolbox = context.inAboutDevtoolsToolbox;

    // Everything after this isn't sent directly from ContextMenu
    if (this.target) {
      this.ownerDoc = this.target.ownerDocument;
    }

    this.csp = E10SUtils.deserializeCSP(context.csp);

    if (this.contentData) {
      this.browser = this.contentData.browser;
      this.selectionInfo = this.contentData.selectionInfo;
      this.actor = this.contentData.actor;
    } else {
      const { SelectionUtils } = ChromeUtils.import(
        "resource://gre/modules/SelectionUtils.jsm"
      );

      this.browser = this.ownerDoc.defaultView.docShell.chromeEventHandler;
      this.selectionInfo = SelectionUtils.getSelectionDetails(window);
      this.actor = this.browser.browsingContext.currentWindowGlobal.getActor(
        "ContextMenu"
      );
    }

    const { gBrowser } = this.browser.ownerGlobal;

    this.textSelected = this.selectionInfo.text;
    this.isTextSelected = !!this.textSelected.length;
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
        this.contentData.spellInfo,
        this.actor.manager
      );
    }

    if (context.shouldInitInlineSpellCheckerUIWithChildren) {
      InlineSpellCheckerUI.initFromRemote(
        this.contentData.spellInfo,
        this.actor.manager
      );
      let canSpell = InlineSpellCheckerUI.canSpellCheck && this.canSpellCheck;
      this.showItem("spell-check-enabled", canSpell);
    }
  } // setContext

  hiding(aXulMenu) {
    if (this.actor) {
      this.actor.hiding();
    }

    aXulMenu.showHideSeparators = null;

    this.contentData = null;
    InlineSpellCheckerUI.clearSuggestionsFromMenu();
    InlineSpellCheckerUI.clearDictionaryListFromMenu();
    InlineSpellCheckerUI.uninit();
    if (
      Cu.isModuleLoaded("resource://gre/modules/LoginManagerContextMenu.jsm")
    ) {
      nsContextMenu.LoginManagerContextMenu.clearLoginsFromMenu(document);
    }

    // This handler self-deletes, only run it if it is still there:
    if (this._onPopupHiding) {
      this._onPopupHiding();
    }
  }

  initItems(aXulMenu) {
    this.initPageMenuSeparator();
    this.initOpenItems();
    this.initNavigationItems();
    this.initViewItems();
    this.initImageItems();
    this.initMiscItems();
    this.initSpellingItems();
    this.initSaveItems();
    this.initClipboardItems();
    this.initMediaPlayerItems();
    this.initLeaveDOMFullScreenItems();
    this.initClickToPlayItems();
    this.initPasswordManagerItems();
    this.initSyncItems();
    this.initViewSourceItems();
    this.initScreenshotItem();

    this.showHideSeparators(aXulMenu);
    if (!aXulMenu.showHideSeparators) {
      // Set the showHideSeparators function on the menu itself so that
      // the extension code (ext-menus.js) can call it after modifying
      // the menus.
      aXulMenu.showHideSeparators = () => {
        this.showHideSeparators(aXulMenu);
      };
    }
  }

  initPageMenuSeparator() {
    this.showItem("page-menu-separator", this.hasPageMenu);
  }

  initOpenItems() {
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
    if (this.contentData.userContextId) {
      inContainer = true;
      var item = document.getElementById("context-openlinkincontainertab");

      item.setAttribute("data-usercontextid", this.contentData.userContextId);

      var label = ContextualIdentityService.getUserContextLabel(
        this.contentData.userContextId
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
  }

  initNavigationItems() {
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

    let stopped =
      XULBrowserWindow.stopCommand.getAttribute("disabled") == "true";

    let stopReloadItem = "";
    if (shouldShow || !this.inTabBrowser) {
      stopReloadItem = stopped || !this.inTabBrowser ? "reload" : "stop";
    }

    this.showItem("context-reload", stopReloadItem == "reload");
    this.showItem("context-stop", stopReloadItem == "stop");

    function initBackForwardMenuItemTooltip(menuItemId, l10nId, shortcutId) {
      let shortcut = document.getElementById(shortcutId);
      if (shortcut) {
        shortcut = ShortcutUtils.prettifyShortcut(shortcut);
      } else {
        // Sidebar doesn't have navigation buttons or shortcuts, but we still
        // want to format the menu item tooltip to remove "$shortcut" string.
        shortcut = "";
      }
      let menuItem = document.getElementById(menuItemId);
      document.l10n.setAttributes(menuItem, l10nId, { shortcut });
    }

    initBackForwardMenuItemTooltip(
      "context-back",
      "main-context-menu-back-2",
      "goBackKb"
    );

    initBackForwardMenuItemTooltip(
      "context-forward",
      "main-context-menu-forward-2",
      "goForwardKb"
    );
  }

  initLeaveDOMFullScreenItems() {
    // only show the option if the user is in DOM fullscreen
    var shouldShow = this.target.ownerDocument.fullscreen;
    this.showItem("context-leave-dom-fullscreen", shouldShow);
  }

  initSaveItems() {
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
    if (
      (this.onSaveableLink || this.onPlainTextLink) &&
      Services.policies.status === Services.policies.ACTIVE
    ) {
      this.setItemAttr(
        "context-savelink",
        "disabled",
        !WebsiteFilter.isAllowed(this.linkURL)
      );
    }

    // Save video and audio don't rely on whether it has loaded or not.
    this.showItem("context-savevideo", this.onVideo);
    this.showItem("context-saveaudio", this.onAudio);
    this.showItem("context-video-saveimage", this.onVideo);
    this.setItemAttr("context-savevideo", "disabled", !this.mediaURL);
    this.setItemAttr("context-saveaudio", "disabled", !this.mediaURL);
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
  }

  initImageItems() {
    // Reload image depends on an image that's not fully loaded
    this.showItem(
      "context-reloadimage",
      this.onImage && !this.onCompletedImage
    );

    // View image depends on having an image that's not standalone
    // (or is in a frame), or a canvas. If this isn't an image, check
    // if there is a background image.
    let showViewImage =
      (this.onImage && (!this.inSyntheticDoc || this.inFrame)) || this.onCanvas;
    let showBGImage =
      this.hasBGImage &&
      !this.hasMultipleBGImages &&
      !this.inSyntheticDoc &&
      !this.inPDFViewer &&
      !this.isContentSelected &&
      !this.onImage &&
      !this.onCanvas &&
      !this.onVideo &&
      !this.onAudio &&
      !this.onLink &&
      !this.onTextInput;
    this.showItem("context-viewimage", showViewImage || showBGImage);

    // Save image depends on having loaded its content.
    this.showItem("context-saveimage", this.onLoadedImage || this.onCanvas);

    // Copy image contents depends on whether we're on an image.
    // Note: the element doesn't exist on all platforms, but showItem() takes
    // care of that by itself.
    this.showItem("context-copyimage-contents", this.onImage);

    // Copy image location depends on whether we're on an image.
    this.showItem("context-copyimage", this.onImage || showBGImage);

    // Send media URL (but not for canvas, since it's a big data: URL)
    this.showItem("context-sendimage", this.onImage || showBGImage);

    // Open the link to more details about the image. Does not apply to
    // background images.
    this.showItem(
      "context-viewimagedesc",
      this.onImage && this.imageDescURL !== ""
    );

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
      document.getElementById(
        "context-setDesktopBackground"
      ).disabled = this.contentData.disableSetDesktopBackground;
    }
  }

  initViewItems() {
    // View source is always OK, unless in directory listing.
    this.showItem(
      "context-viewpartialsource-selection",
      !this.inAboutDevtoolsToolbox &&
        this.isContentSelected &&
        this.selectionInfo.isDocumentLevelSelection
    );

    this.showItem(
      "context-print-selection",
      !this.inAboutDevtoolsToolbox &&
        this.isContentSelected &&
        this.selectionInfo.isDocumentLevelSelection
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
      Services.prefs.getBoolPref("devtools.accessibility.enabled", false) &&
      Services.prefs.getBoolPref("devtools.enabled", true) &&
      (Services.prefs.getBoolPref("devtools.everOpened", false) ||
        // Note: this is a legacy usecase, we will remove it in bug 1695257,
        // once existing users have had time to set devtools.everOpened
        // through normal use, and we've passed an ESR cycle (91).
        nsContextMenu.DevToolsShim.isDevToolsUser());

    this.showItem("context-viewsource", shouldShow);
    this.showItem("context-inspect", showInspect);

    this.showItem("context-inspect-a11y", showInspectA11Y);

    // View video depends on not having a standalone video.
    this.showItem(
      "context-viewvideo",
      this.onVideo && (!this.inSyntheticDoc || this.inFrame)
    );
    this.setItemAttr("context-viewvideo", "disabled", !this.mediaURL);
  }

  initMiscItems() {
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
    this.showItem("context-keywordfield", this.shouldShowAddKeyword());
    this.showItem("frame", this.inFrame);

    if (this.inFrame) {
      // To make it easier to debug the browser running with out-of-process iframes, we
      // display the process PID of the iframe in the context menu for the subframe.
      let frameOsPid = this.actor.manager.browsingContext.currentWindowGlobal
        .osPid;
      this.setItemAttr("context-frameOsPid", "label", "PID: " + frameOsPid);
    }

    this.showAndFormatSearchContextItem();

    // srcdoc cannot be opened separately due to concerns about web
    // content with about:srcdoc in location bar masquerading as trusted
    // chrome/addon content.
    // No need to also test for this.inFrame as this is checked in the parent
    // submenu.
    this.showItem("context-showonlythisframe", !this.inSrcdocFrame);
    this.showItem("context-openframeintab", !this.inSrcdocFrame);
    this.showItem("context-openframe", !this.inSrcdocFrame);
    this.showItem("context-bookmarkframe", !this.inSrcdocFrame);

    // Hide menu entries for images, show otherwise
    if (this.inFrame) {
      this.viewFrameSourceElement.hidden = !BrowserUtils.mimeTypeIsTextBased(
        this.target.ownerDocument.contentType
      );
    }

    // BiDi UI
    this.showItem(
      "context-bidi-text-direction-toggle",
      this.onTextInput && !this.onNumeric && top.gBidiUI
    );
    this.showItem(
      "context-bidi-page-direction-toggle",
      !this.onTextInput && top.gBidiUI
    );
  }

  initSpellingItems() {
    var canSpell =
      InlineSpellCheckerUI.canSpellCheck &&
      !InlineSpellCheckerUI.initialSpellCheckPending &&
      this.canSpellCheck;
    let showDictionaries = canSpell && InlineSpellCheckerUI.enabled;
    var onMisspelling = InlineSpellCheckerUI.overMisspelling;
    var showUndo = canSpell && InlineSpellCheckerUI.canUndo();
    this.showItem("spell-check-enabled", canSpell);
    document
      .getElementById("spell-check-enabled")
      .setAttribute("checked", canSpell && InlineSpellCheckerUI.enabled);

    this.showItem("spell-add-to-dictionary", onMisspelling);
    this.showItem("spell-undo-add-to-dictionary", showUndo);

    // suggestion list
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
      InlineSpellCheckerUI.addDictionaryListToMenu(dictMenu, dictSep);
      this.showItem("spell-add-dictionaries-main", false);
    } else if (this.onSpellcheckable) {
      // when there is no spellchecker but we might be able to spellcheck
      // add the add to dictionaries item. This will ensure that people
      // with no dictionaries will be able to download them
      this.showItem("spell-add-dictionaries-main", showDictionaries);
    } else {
      this.showItem("spell-add-dictionaries-main", false);
    }
  }

  initClipboardItems() {
    // Copy depends on whether there is selected text.
    // Enabling this context menu item is now done through the global
    // command updating system
    // this.setItemAttr( "context-copy", "disabled", !this.isTextSelected() );
    goUpdateGlobalEditMenuItems();

    this.showItem("context-undo", this.onTextInput);
    this.showItem("context-redo", this.onTextInput);
    this.showItem("context-cut", this.onTextInput);
    this.showItem("context-copy", this.isContentSelected || this.onTextInput);
    this.showItem("context-paste", this.onTextInput);
    this.showItem("context-delete", this.onTextInput);
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

    // XXX dr
    // ------
    // nsDocumentViewer.cpp has code to determine whether we're
    // on a link or an image. we really ought to be using that...

    // Copy email link depends on whether we're on an email link.
    this.showItem("context-copyemail", this.onMailtoLink);

    // Copy link location depends on whether we're on a non-mailto link.
    this.showItem("context-copylink", this.onLink && !this.onMailtoLink);

    this.showItem("context-copyvideourl", this.onVideo);
    this.showItem("context-copyaudiourl", this.onAudio);
    this.setItemAttr("context-copyvideourl", "disabled", !this.mediaURL);
    this.setItemAttr("context-copyaudiourl", "disabled", !this.mediaURL);
  }

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
        !this.target.ownerDocument.fullscreen &&
        this.target.readyState > 0;
      this.showItem("context-video-pictureinpicture", shouldDisplay);
    }
    this.showItem("context-media-eme-learnmore", this.onDRMMedia);

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
  }

  initClickToPlayItems() {
    this.showItem("context-ctp-play", this.onCTPPlugin);
    this.showItem("context-ctp-hide", this.onCTPPlugin);
  }

  initPasswordManagerItems() {
    let showUseSavedLogin = false;
    let showGenerate = false;
    let showManage = false;
    let enableGeneration = Services.logins.isLoggedIn;
    try {
      // If we could not find a password field we don't want to
      // show the form fill, manage logins and the password generation items.
      if (!this.isLoginForm()) {
        return;
      }
      showManage = true;

      // Disable the fill option if the user hasn't unlocked with their master password
      // or if the password field or target field are disabled.
      // XXX: Bug 1529025 to maybe respect signon.rememberSignons.
      let loginFillInfo = this.contentData?.loginFillInfo;
      let disableFill =
        !Services.logins.isLoggedIn ||
        loginFillInfo?.passwordField.disabled ||
        loginFillInfo?.activeField.disabled;
      this.setItemAttr("fill-login", "disabled", disableFill);

      let onPasswordLikeField = PASSWORD_FIELDNAME_HINTS.includes(
        loginFillInfo.activeField.fieldNameHint
      );

      // Set the correct label for the fill menu
      let fillMenu = document.getElementById("fill-login");
      if (onPasswordLikeField) {
        fillMenu.setAttribute(
          "data-l10n-id",
          "main-context-menu-use-saved-password"
        );
      } else {
        // On a username field
        fillMenu.setAttribute(
          "data-l10n-id",
          "main-context-menu-use-saved-login"
        );
      }

      let documentURI = this.contentData?.documentURIObject;
      let formOrigin = LoginHelper.getLoginOrigin(documentURI?.spec);
      let isGeneratedPasswordEnabled =
        LoginHelper.generationAvailable && LoginHelper.generationEnabled;
      showGenerate =
        onPasswordLikeField &&
        isGeneratedPasswordEnabled &&
        Services.logins.getLoginSavingEnabled(formOrigin);

      if (disableFill) {
        showUseSavedLogin = true;

        // No need to update the submenu if the fill item is disabled.
        return;
      }

      // Update sub-menu items.
      let fragment = nsContextMenu.LoginManagerContextMenu.addLoginsToMenu(
        this.targetIdentifier,
        this.browser,
        formOrigin
      );

      if (!fragment) {
        return;
      }

      showUseSavedLogin = true;
      let popup = document.getElementById("fill-login-popup");
      popup.appendChild(fragment);
    } finally {
      this.showItem("fill-login", showUseSavedLogin);
      this.showItem("fill-login-generated-password", showGenerate);
      this.showItem("manage-saved-logins", showManage);
      this.setItemAttr(
        "fill-login-generated-password",
        "disabled",
        !enableGeneration
      );
      this.showItem(
        "passwordmgr-items-separator",
        showUseSavedLogin || showGenerate || showManage
      );
    }
  }

  initSyncItems() {
    gSync.updateContentContextMenu(this);
  }

  initViewSourceItems() {
    const getString = name => {
      const { bundle } = gViewSourceUtils.getPageActor(this.browser);
      return bundle.GetStringFromName(name);
    };
    const showViewSourceItem = (id, check, accesskey) => {
      const fullId = `context-viewsource-${id}`;
      this.showItem(fullId, onViewSource);
      if (!onViewSource) {
        return;
      }
      check().then(checked => this.setItemAttr(fullId, "checked", checked));
      this.setItemAttr(fullId, "label", getString(`context_${id}_label`));
      if (accesskey) {
        this.setItemAttr(
          fullId,
          "accesskey",
          getString(`context_${id}_accesskey`)
        );
      }
    };

    const onViewSource = this.browser.currentURI.schemeIs("view-source");

    showViewSourceItem("goToLine", async () => false, true);
    showViewSourceItem("wrapLongLines", () =>
      gViewSourceUtils.getPageActor(this.browser).queryIsWrapping()
    );
    showViewSourceItem("highlightSyntax", () =>
      gViewSourceUtils.getPageActor(this.browser).queryIsSyntaxHighlighting()
    );
  }

  // Iterate over the visible items on the menu and its submenus and
  // hide any duplicated separators next to each other.
  showHideSeparators(aPopup) {
    let lastVisibleSeparator = null;
    let count = 0;
    for (let menuItem of aPopup.children) {
      // Skip any items that were added by the page menu.
      if (menuItem.hasAttribute("generateditemid")) {
        count++;
        continue;
      }

      if (menuItem.localName == "menuseparator") {
        if (!count) {
          menuItem.hidden = true;
        } else {
          menuItem.hidden = false;
          lastVisibleSeparator = menuItem;
        }

        count = 0;
      } else if (!menuItem.hidden) {
        if (menuItem.localName == "menu") {
          this.showHideSeparators(menuItem.menupopup);
        } else if (menuItem.localName == "menugroup") {
          this.showHideSeparators(menuItem);
        }
        count++;
      }
    }

    // If count is 0 yet lastVisibleSeparator is set, then there must be a separator
    // visible at the end of the menu, so hide it. Note that there could be more than
    // one but this isn't handled here.
    if (!count && lastVisibleSeparator) {
      lastVisibleSeparator.hidden = true;
    }
  }

  initScreenshotItem() {
    // About pages other than about:reader are not currently supported by
    // screenshots (see Bug 1620992)
    let uri = this.contentData?.documentURIObject;
    let shouldShow =
      !screenshotsDisabled &&
      (uri.scheme != "about" || uri.spec.startsWith("about:reader")) &&
      this.inTabBrowser &&
      !this.onTextInput &&
      !this.onLink &&
      !this.onPlainTextLink &&
      !this.onImage &&
      !this.onVideo &&
      !this.onAudio &&
      !this.onEditable &&
      !this.onPassword &&
      !this.inFrame;

    this.showItem("context-sep-screenshots", shouldShow);
    this.showItem("context-take-screenshot", shouldShow);
  }

  openPasswordManager() {
    LoginHelper.openPasswordManager(window, {
      entryPoint: "contextmenu",
    });
  }

  useGeneratedPassword() {
    nsContextMenu.LoginManagerContextMenu.useGeneratedPassword(
      this.targetIdentifier,
      this.contentData.documentURIObject,
      this.browser
    );
  }

  isLoginForm() {
    let loginFillInfo = this.contentData?.loginFillInfo;
    let documentURI = this.contentData?.documentURIObject;

    // If we could not find a password field then
    // don't treat this as a login form.
    return (
      loginFillInfo?.passwordField?.found &&
      !documentURI?.schemeIs("about") &&
      this.browser.contentPrincipal.spec != "resource://pdf.js/web/viewer.html"
    );
  }

  inspectNode() {
    return nsContextMenu.DevToolsShim.inspectNode(
      gBrowser.selectedTab,
      this.targetIdentifier
    );
  }

  inspectA11Y() {
    return nsContextMenu.DevToolsShim.inspectA11Y(
      gBrowser.selectedTab,
      this.targetIdentifier
    );
  }

  _openLinkInParameters(extra) {
    let params = {
      charset: this.contentData.charSet,
      originPrincipal: this.principal,
      originStoragePrincipal: this.storagePrincipal,
      triggeringPrincipal: this.principal,
      csp: this.csp,
      frameID: this.contentData.frameID,
    };
    for (let p in extra) {
      params[p] = extra[p];
    }

    let referrerInfo = this.onLink
      ? this.contentData.linkReferrerInfo
      : this.contentData.referrerInfo;
    // If we want to change userContextId, we must be sure that we don't
    // propagate the referrer.
    if (
      ("userContextId" in params &&
        params.userContextId != this.contentData.userContextId) ||
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
  }

  // Open linked-to URL in a new window.
  openLink() {
    openLinkIn(this.linkURL, "window", this._openLinkInParameters());
  }

  // Open linked-to URL in a new private window.
  openLinkInPrivateWindow() {
    openLinkIn(
      this.linkURL,
      "window",
      this._openLinkInParameters({ private: true })
    );
  }

  // Open linked-to URL in a new tab.
  openLinkInTab(event) {
    let referrerURI = this.contentData.documentURIObject;

    // if its parent allows mixed content and the referring URI passes
    // a same origin check with the target URI, we can preserve the users
    // decision of disabling MCB on a page for it's child tabs.
    let persistAllowMixedContentInChildTab = false;

    if (this.contentData.parentAllowsMixedContent) {
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
  }

  // open URL in current tab
  openLinkInCurrent() {
    openLinkIn(this.linkURL, "current", this._openLinkInParameters());
  }

  // Open frame in a new tab.
  openFrameInTab() {
    openLinkIn(this.contentData.docLocation, "tab", {
      charset: this.contentData.charSet,
      triggeringPrincipal: this.browser.contentPrincipal,
      csp: this.browser.csp,
      referrerInfo: this.contentData.frameReferrerInfo,
    });
  }

  // Reload clicked-in frame.
  reloadFrame(aEvent) {
    let forceReload = aEvent.shiftKey;
    this.actor.reloadFrame(this.targetIdentifier, forceReload);
  }

  // Open clicked-in frame in its own window.
  openFrame() {
    openLinkIn(this.contentData.docLocation, "window", {
      charset: this.contentData.charSet,
      triggeringPrincipal: this.browser.contentPrincipal,
      csp: this.browser.csp,
      referrerInfo: this.contentData.frameReferrerInfo,
    });
  }

  // Open clicked-in frame in the same window.
  showOnlyThisFrame() {
    urlSecurityCheck(
      this.contentData.docLocation,
      this.browser.contentPrincipal,
      Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT
    );
    openWebLinkIn(this.contentData.docLocation, "current", {
      referrerInfo: this.contentData.frameReferrerInfo,
      triggeringPrincipal: this.browser.contentPrincipal,
    });
  }

  takeScreenshot() {
    Services.obs.notifyObservers(null, "contextmenu-screenshot");
  }

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

    top.gViewSourceUtils.viewPartialSourceInBrowser(
      this.actor.browsingContext,
      openSelectionFn
    );
  }

  // Open new "view source" window with the frame's URL.
  viewFrameSource() {
    BrowserViewSourceOfDocument({
      browser: this.browser,
      URL: this.contentData.docLocation,
      outerWindowID: this.frameOuterWindowID,
    });
  }

  viewInfo() {
    BrowserPageInfo(
      this.contentData.docLocation,
      null,
      null,
      null,
      this.browser
    );
  }

  viewImageDesc(e) {
    urlSecurityCheck(
      this.imageDescURL,
      this.principal,
      Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT
    );
    openUILink(this.imageDescURL, e, {
      referrerInfo: this.contentData.referrerInfo,
      triggeringPrincipal: this.principal,
      csp: this.csp,
    });
  }

  viewFrameInfo() {
    BrowserPageInfo(
      this.contentData.docLocation,
      null,
      null,
      this.actor.browsingContext,
      this.browser
    );
  }

  reloadImage() {
    urlSecurityCheck(
      this.mediaURL,
      this.principal,
      Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT
    );
    this.actor.reloadImage(this.targetIdentifier);
  }

  _canvasToBlobURL(targetIdentifier) {
    return this.actor.canvasToBlobURL(targetIdentifier);
  }

  // Change current window to the URL of the image, video, or audio.
  viewMedia(e) {
    let referrerInfo = this.contentData.referrerInfo;
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
  }

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
    let referrerInfo = this.contentData.referrerInfo;
    let cookieJarSettings = this.contentData.cookieJarSettings;

    this.actor.saveVideoFrameAsImage(this.targetIdentifier).then(dataURL => {
      // FIXME can we switch this to a blob URL?
      internalSave(
        dataURL,
        null, // document
        name,
        null, // content disposition
        "image/jpeg", // content type - keep in sync with ContextMenuChild!
        true, // bypass cache
        "SaveImageTitle",
        null, // chosen data
        referrerInfo,
        cookieJarSettings,
        null, // initiating doc
        false, // don't skip prompt for where to save
        null, // cache key
        isPrivate,
        this.principal
      );
    });
  }

  leaveDOMFullScreen() {
    document.exitFullscreen();
  }

  // Change current window to the URL of the background image.
  viewBGImage(e) {
    urlSecurityCheck(
      this.bgImageURL,
      this.principal,
      Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT
    );

    openUILink(this.bgImageURL, e, {
      referrerInfo: this.contentData.referrerInfo,
      forceAllowDataURI: true,
      triggeringPrincipal: this.principal,
      csp: this.csp,
    });
  }

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
          "chrome://browser/content/setDesktopBackground.xhtml";

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
  }

  // Save URL of clicked-on frame.
  saveFrame() {
    saveBrowser(this.browser, false, this.frameBrowsingContext);
  }

  // Helper function to wait for appropriate MIME-type headers and
  // then prompt the user with a file picker
  saveHelper(
    linkURL,
    linkText,
    dialogTitle,
    bypassCache,
    doc,
    referrerInfo,
    cookieJarSettings,
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
            let msg = bundle.GetStringFromName("downloadErrorGeneric");

            try {
              const channel = aRequest.QueryInterface(Ci.nsIChannel);
              const reason = channel.loadInfo.requestBlockingReason;
              if (
                reason == Ci.nsILoadInfo.BLOCKING_REASON_EXTENSION_WEBREQUEST
              ) {
                try {
                  const properties = channel.QueryInterface(Ci.nsIPropertyBag);
                  const id = properties.getProperty("cancelledByExtension");
                  msg = bundle.formatStringFromName("downloadErrorBlockedBy", [
                    WebExtensionPolicy.getByID(id).name,
                  ]);
                } catch (err) {
                  // "cancelledByExtension" doesn't have to be available.
                  msg = bundle.GetStringFromName("downloadErrorExtension");
                }
              }
            } catch (ex) {}

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
            cookieJarSettings,
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
        throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
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
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT,
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

      channel.loadInfo.cookieJarSettings = cookieJarSettings;
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
  }

  // Save URL of clicked-on link.
  saveLink() {
    let referrerInfo = this.onLink
      ? this.contentData.linkReferrerInfo
      : this.contentData.referrerInfo;

    let isContentWindowPrivate = this.ownerDoc.isPrivate;
    this.saveHelper(
      this.linkURL,
      this.linkTextStr,
      null,
      true,
      this.ownerDoc,
      referrerInfo,
      this.contentData.cookieJarSettings,
      this.frameOuterWindowID,
      this.linkDownload,
      isContentWindowPrivate
    );
  }

  // Backwards-compatibility wrapper
  saveImage() {
    if (this.onCanvas || this.onImage) {
      this.saveMedia();
    }
  }

  // Save URL of the clicked upon image, video, or audio.
  saveMedia() {
    let doc = this.ownerDoc;
    let isContentWindowPrivate = this.ownerDoc.isPrivate;
    let referrerInfo = this.contentData.referrerInfo;
    let cookieJarSettings = this.contentData.cookieJarSettings;
    let isPrivate = PrivateBrowsingUtils.isBrowserPrivate(this.browser);
    if (this.onCanvas) {
      // Bypass cache, since it's a data: URL.
      this._canvasToBlobURL(this.targetIdentifier).then(function(blobURL) {
        internalSave(
          blobURL,
          null, // document
          "canvas.png",
          null, // content disposition
          "image/png", // _canvasToBlobURL uses image/png by default.
          true, // bypass cache
          "SaveImageTitle",
          null, // chosen data
          referrerInfo,
          cookieJarSettings,
          null, // initiating doc
          false, // don't skip prompt for where to save
          null, // cache key
          isPrivate,
          document.nodePrincipal /* system, because blob: */
        );
      }, Cu.reportError);
    } else if (this.onImage) {
      urlSecurityCheck(this.mediaURL, this.principal);
      internalSave(
        this.mediaURL,
        null, // document
        null, // file name; we'll take it from the URL
        this.contentData.contentDisposition,
        this.contentData.contentType,
        false, // do not bypass the cache
        "SaveImageTitle",
        null, // chosen data
        referrerInfo,
        cookieJarSettings,
        null, // initiating doc
        false, // don't skip prompt for where to save
        null, // cache key
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
        cookieJarSettings,
        this.frameOuterWindowID,
        "",
        isContentWindowPrivate
      );
    }
  }

  // Backwards-compatibility wrapper
  sendImage() {
    if (this.onCanvas || this.onImage) {
      this.sendMedia();
    }
  }

  sendMedia() {
    MailIntegration.sendMessage(this.mediaURL, "");
  }

  playPlugin() {
    this.actor.pluginCommand("play", this.targetIdentifier);
  }

  hidePlugin() {
    this.actor.pluginCommand("hide", this.targetIdentifier);
  }

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
      addresses = Services.textToSubURI.unEscapeURIForUI(addresses);
    } catch (ex) {
      // Do nothing.
    }

    var clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(
      Ci.nsIClipboardHelper
    );
    clipboard.copyString(addresses);
  }

  copyLink() {
    // If we're in a view source tab, remove the view-source: prefix
    let linkURL = this.linkURL.replace(/^view-source:/, "");
    var clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(
      Ci.nsIClipboardHelper
    );
    clipboard.copyString(linkURL);
  }

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
  }

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
  }

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
  }

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
  }

  getLinkURI() {
    try {
      return makeURI(this.linkURL);
    } catch (ex) {
      // e.g. empty URL string
    }

    return null;
  }

  // Kept for addon compat
  linkText() {
    return this.linkTextStr;
  }

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
  }

  shouldShowAddKeyword() {
    return this.onTextInput && this.onKeywordField && !this.isLoginForm();
  }

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
  }

  bookmarkThisPage() {
    window.top.PlacesCommandHook.bookmarkPage().catch(Cu.reportError);
  }

  bookmarkLink() {
    window.top.PlacesCommandHook.bookmarkLink(
      this.linkURL,
      this.linkTextStr
    ).catch(Cu.reportError);
  }

  addBookmarkForFrame() {
    let uri = this.contentData.documentURIObject;

    this.actor.getFrameTitle(this.targetIdentifier).then(title => {
      window.top.PlacesCommandHook.bookmarkLink(uri.spec, title).catch(
        Cu.reportError
      );
    });
  }

  savePageAs() {
    saveBrowser(this.browser);
  }

  printFrame() {
    PrintUtils.startPrintWindow(this.actor.browsingContext, {
      printFrameOnly: true,
    });
  }

  printSelection() {
    PrintUtils.startPrintWindow(this.actor.browsingContext, {
      printSelectionOnly: true,
    });
  }

  switchPageDirection() {
    gBrowser.selectedBrowser.sendMessageToActor(
      "SwitchDocumentDirection",
      {},
      "SwitchDocumentDirection",
      "roots"
    );
  }

  mediaCommand(command, data) {
    this.actor.mediaCommand(this.targetIdentifier, command, data);
  }

  copyMediaLocation() {
    var clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(
      Ci.nsIClipboardHelper
    );
    clipboard.copyString(this.mediaURL);
  }

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
  }

  get imageURL() {
    if (this.onImage) {
      return this.mediaURL;
    }
    return "";
  }

  // Formats the 'Search <engine> for "<selection or link text>"' context menu.
  showAndFormatSearchContextItem() {
    let menuItem = document.getElementById("context-searchselect");
    let menuItemPrivate = document.getElementById(
      "context-searchselect-private"
    );
    if (!Services.search.isInitialized) {
      menuItem.hidden = true;
      menuItemPrivate.hidden = true;
      return;
    }
    const docIsPrivate = PrivateBrowsingUtils.isBrowserPrivate(this.browser);
    const privatePref = "browser.search.separatePrivateDefault.ui.enabled";
    let showSearchSelect =
      !this.inAboutDevtoolsToolbox &&
      (this.isTextSelected || this.onLink) &&
      !this.onImage;
    // Don't show the private search item when we're already in a private
    // browsing window.
    let showPrivateSearchSelect =
      showSearchSelect &&
      !docIsPrivate &&
      Services.prefs.getBoolPref(privatePref);

    menuItem.hidden = !showSearchSelect;
    menuItemPrivate.hidden = !showPrivateSearchSelect;
    // If we're not showing the menu items, we can skip formatting the labels.
    if (!showSearchSelect) {
      return;
    }

    let selectedText = this.isTextSelected
      ? this.textSelected
      : this.linkTextStr;

    // Store searchTerms in context menu item so we know what to search onclick
    menuItem.searchTerms = menuItemPrivate.searchTerms = selectedText;
    menuItem.principal = menuItemPrivate.principal = this.principal;
    menuItem.csp = menuItemPrivate.csp = this.csp;

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
    let engineName = Services.search.defaultEngine.name;
    let privateEngineName = Services.search.defaultPrivateEngine.name;
    menuItem.usePrivate = docIsPrivate;
    let menuLabel = gNavigatorBundle.getFormattedString("contextMenuSearch", [
      docIsPrivate ? privateEngineName : engineName,
      selectedText,
    ]);
    menuItem.label = menuLabel;
    menuItem.accessKey = gNavigatorBundle.getString(
      "contextMenuSearch.accesskey"
    );

    if (showPrivateSearchSelect) {
      let otherEngine = engineName != privateEngineName;
      let accessKey = "contextMenuPrivateSearch.accesskey";
      if (otherEngine) {
        menuItemPrivate.label = gNavigatorBundle.getFormattedString(
          "contextMenuPrivateSearchOtherEngine",
          [privateEngineName]
        );
        accessKey = "contextMenuPrivateSearchOtherEngine.accesskey";
      } else {
        menuItemPrivate.label = gNavigatorBundle.getString(
          "contextMenuPrivateSearch"
        );
      }
      menuItemPrivate.accessKey = gNavigatorBundle.getString(accessKey);
    }
  }

  createContainerMenu(aEvent) {
    let createMenuOptions = {
      isContextMenu: true,
      excludeUserContextId: this.contentData.userContextId,
    };
    return createUserContextMenu(aEvent, createMenuOptions);
  }

  doCustomCommand(generatedItemId, handlingUserInput) {
    this.actor.doCustomCommand(generatedItemId, handlingUserInput);
  }
}

XPCOMUtils.defineLazyModuleGetters(nsContextMenu, {
  LoginManagerContextMenu: "resource://gre/modules/LoginManagerContextMenu.jsm",
  DevToolsShim: "chrome://devtools-startup/content/DevToolsShim.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "screenshotsDisabled",
  "extensions.screenshots.disabled",
  false
);
