/* vim: set ts=2 sw=2 sts=2 et tw=80: */
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

Components.utils.import("resource://gre/modules/PrivateBrowsingUtils.jsm");
Components.utils.import("resource://gre/modules/InlineSpellChecker.jsm");
Components.utils.import("resource://gre/modules/BrowserUtils.jsm");

var gContextMenuContentData = null;

function nsContextMenu(aXulMenu, aIsShift) {
  this.shouldDisplay = true;
  this.initMenu(aXulMenu, aIsShift);
}

// Prototype for nsContextMenu "class."
nsContextMenu.prototype = {
  initMenu: function CM_initMenu(aXulMenu, aIsShift) {
    // Get contextual info.
    this.setTarget(document.popupNode, document.popupRangeParent,
                   document.popupRangeOffset);
    if (!this.shouldDisplay)
      return;

    this.hasPageMenu = false;
    if (!aIsShift) {
      if (this.isRemote) {
        this.hasPageMenu =
          PageMenuParent.addToPopup(gContextMenuContentData.customMenuItems,
                                    this.browser, aXulMenu);
      }
      else {
        this.hasPageMenu = PageMenuParent.buildAndAddToPopup(this.target, aXulMenu);
      }
    }

    this.isFrameImage = document.getElementById("isFrameImage");
    this.ellipsis = "\u2026";
    try {
      this.ellipsis = gPrefService.getComplexValue("intl.ellipsis",
                                                   Ci.nsIPrefLocalizedString).data;
    } catch (e) { }

    this.isContentSelected = this.isContentSelection();
    this.onPlainTextLink = false;

    // Initialize (disable/remove) menu items.
    this.initItems();

    // Register this opening of the menu with telemetry:
    this._checkTelemetryForMenu(aXulMenu);
  },

  hiding: function CM_hiding() {
    gContextMenuContentData = null;
    InlineSpellCheckerUI.clearSuggestionsFromMenu();
    InlineSpellCheckerUI.clearDictionaryListFromMenu();
    InlineSpellCheckerUI.uninit();

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
  },

  initPageMenuSeparator: function CM_initPageMenuSeparator() {
    this.showItem("page-menu-separator", this.hasPageMenu);
  },

  initOpenItems: function CM_initOpenItems() {
    var isMailtoInternal = false;
    if (this.onMailtoLink) {
      var mailtoHandler = Cc["@mozilla.org/uriloader/external-protocol-service;1"].
                          getService(Ci.nsIExternalProtocolService).
                          getProtocolHandlerInfo("mailto");
      isMailtoInternal = (!mailtoHandler.alwaysAskBeforeHandling &&
                          mailtoHandler.preferredAction == Ci.nsIHandlerInfo.useHelperApp &&
                          (mailtoHandler.preferredApplicationHandler instanceof Ci.nsIWebHandlerApp));
    }

    // Time to do some bad things and see if we've highlighted a URL that
    // isn't actually linked.
    if (this.isTextSelected && !this.onLink) {
      // Ok, we have some text, let's figure out if it looks like a URL.
      let selection =  this.focusedWindow.getSelection();
      let linkText = selection.toString().trim();
      let uri;
      if (/^(?:https?|ftp):/i.test(linkText)) {
        try {
          uri = makeURI(linkText);
        } catch (ex) {}
      }
      // Check if this could be a valid url, just missing the protocol.
      else if (/^(?:[a-z\d-]+\.)+[a-z]+$/i.test(linkText)) {
        // Now let's see if this is an intentional link selection. Our guess is
        // based on whether the selection begins/ends with whitespace or is
        // preceded/followed by a non-word character.

        // selection.toString() trims trailing whitespace, so we look for
        // that explicitly in the first and last ranges.
        let beginRange = selection.getRangeAt(0);
        let delimitedAtStart = /^\s/.test(beginRange);
        if (!delimitedAtStart) {
          let container = beginRange.startContainer;
          let offset = beginRange.startOffset;
          if (container.nodeType == Node.TEXT_NODE && offset > 0)
            delimitedAtStart = /\W/.test(container.textContent[offset - 1]);
          else
            delimitedAtStart = true;
        }

        let delimitedAtEnd = false;
        if (delimitedAtStart) {
          let endRange = selection.getRangeAt(selection.rangeCount - 1);
          delimitedAtEnd = /\s$/.test(endRange);
          if (!delimitedAtEnd) {
            let container = endRange.endContainer;
            let offset = endRange.endOffset;
            if (container.nodeType == Node.TEXT_NODE &&
                offset < container.textContent.length)
              delimitedAtEnd = /\W/.test(container.textContent[offset]);
            else
              delimitedAtEnd = true;
          }
        }

        if (delimitedAtStart && delimitedAtEnd) {
          let uriFixup = Cc["@mozilla.org/docshell/urifixup;1"]
                           .getService(Ci.nsIURIFixup);
          try {
            uri = uriFixup.createFixupURI(linkText, uriFixup.FIXUP_FLAG_NONE);
          } catch (ex) {}
        }
      }

      if (uri && uri.host) {
        this.linkURI = uri;
        this.linkURL = this.linkURI.spec;
        this.linkText = linkText;
        this.onPlainTextLink = true;
      }
    }

    var shouldShow = this.onSaveableLink || isMailtoInternal || this.onPlainTextLink;
    var isWindowPrivate = PrivateBrowsingUtils.isWindowPrivate(window);
    this.showItem("context-openlink", shouldShow && !isWindowPrivate);
    this.showItem("context-openlinkprivate", shouldShow);
    this.showItem("context-openlinkintab", shouldShow);
    this.showItem("context-openlinkincurrent", this.onPlainTextLink);
    this.showItem("context-sep-open", shouldShow);
  },

  initNavigationItems: function CM_initNavigationItems() {
    var shouldShow = !(this.isContentSelected || this.onLink || this.onImage ||
                       this.onCanvas || this.onVideo || this.onAudio ||
                       this.onTextInput || this.onSocial);
    this.showItem("context-navigation", shouldShow);
    this.showItem("context-sep-navigation", shouldShow);

    let stopped = XULBrowserWindow.stopCommand.getAttribute("disabled") == "true";

    let stopReloadItem = "";
    if (shouldShow || this.onSocial) {
      stopReloadItem = (stopped || this.onSocial) ? "reload" : "stop";
    }

    this.showItem("context-reload", stopReloadItem == "reload");
    this.showItem("context-stop", stopReloadItem == "stop");

    // XXX: Stop is determined in browser.js; the canStop broadcaster is broken
    //this.setItemAttrFromNode( "context-stop", "disabled", "canStop" );
  },

  initLeaveDOMFullScreenItems: function CM_initLeaveFullScreenItem() {
    // only show the option if the user is in DOM fullscreen
    var shouldShow = (this.target.ownerDocument.mozFullScreenElement != null);
    this.showItem("context-leave-dom-fullscreen", shouldShow);

    // Explicitly show if in DOM fullscreen, but do not hide it has already been shown
    if (shouldShow)
        this.showItem("context-media-sep-commands", true);
  },

  initSaveItems: function CM_initSaveItems() {
    var shouldShow = !(this.onTextInput || this.onLink ||
                       this.isContentSelected || this.onImage ||
                       this.onCanvas || this.onVideo || this.onAudio);
    this.showItem("context-savepage", shouldShow);

    // Save link depends on whether we're in a link, or selected text matches valid URL pattern.
    this.showItem("context-savelink", this.onSaveableLink || this.onPlainTextLink);

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
    this.showItem("context-castvideo", this.onVideo);
    this.showItem("context-sendaudio", this.onAudio);
    this.setItemAttr("context-sendvideo", "disabled", !this.mediaURL);
    this.setItemAttr("context-sendaudio", "disabled", !this.mediaURL);
    let shouldShowCast = Services.prefs.getBoolPref("browser.casting.enabled");
    // getServicesForVideo alone would be sufficient here (it depends on
    // SimpleServiceDiscovery.services), but SimpleServiceDiscovery is guaranteed
    // to be already loaded, since we load it on startup in nsBrowserGlue,
    // and CastingApps isn't, so check SimpleServiceDiscovery.services first
    // to avoid needing to load CastingApps.jsm if we don't need to.
    shouldShowCast = shouldShowCast && this.mediaURL &&
                     SimpleServiceDiscovery.services.length > 0 &&
                     CastingApps.getServicesForVideo(this.target).length > 0;
    this.setItemAttr("context-castvideo", "disabled", !shouldShowCast);
  },

  initViewItems: function CM_initViewItems() {
    // View source is always OK, unless in directory listing.
    this.showItem("context-viewpartialsource-selection",
                  this.isContentSelected);
    this.showItem("context-viewpartialsource-mathml",
                  this.onMathML && !this.isContentSelected);

    var shouldShow = !(this.isContentSelected ||
                       this.onImage || this.onCanvas ||
                       this.onVideo || this.onAudio ||
                       this.onLink || this.onTextInput);
    var showInspect = !this.onSocial && gPrefService.getBoolPref("devtools.inspector.enabled");
    this.showItem("context-viewsource", shouldShow);
    this.showItem("context-viewinfo", shouldShow);
    this.showItem("inspect-separator", showInspect);
    this.showItem("context-inspect", showInspect);

    this.showItem("context-sep-viewsource", shouldShow);

    // Set as Desktop background depends on whether an image was clicked on,
    // and only works if we have a shell service.
    var haveSetDesktopBackground = false;
#ifdef HAVE_SHELL_SERVICE
    // Only enable Set as Desktop Background if we can get the shell service.
    var shell = getShellService();
    if (shell)
      haveSetDesktopBackground = shell.canSetDesktopBackground;
#endif
    this.showItem("context-setDesktopBackground",
                  haveSetDesktopBackground && this.onLoadedImage);

    if (haveSetDesktopBackground && this.onLoadedImage) {
      document.getElementById("context-setDesktopBackground")
              .disabled = this.disableSetDesktopBackground();
    }

    // Reload image depends on an image that's not fully loaded
    this.showItem("context-reloadimage", (this.onImage && !this.onCompletedImage));

    // View image depends on having an image that's not standalone
    // (or is in a frame), or a canvas.
    this.showItem("context-viewimage", (this.onImage &&
                  (!this.inSyntheticDoc || this.inFrame)) || this.onCanvas);

    // View video depends on not having a standalone video.
    this.showItem("context-viewvideo", this.onVideo && (!this.inSyntheticDoc || this.inFrame));
    this.setItemAttr("context-viewvideo",  "disabled", !this.mediaURL);

    // View background image depends on whether there is one, but don't make
    // background images of a stand-alone media document available.
    this.showItem("context-viewbgimage", shouldShow &&
                                         !this._hasMultipleBGImages &&
                                         !this.inSyntheticDoc);
    this.showItem("context-sep-viewbgimage", shouldShow &&
                                             !this._hasMultipleBGImages &&
                                             !this.inSyntheticDoc);
    document.getElementById("context-viewbgimage")
            .disabled = !this.hasBGImage;

    this.showItem("context-viewimageinfo", this.onImage);
    this.showItem("context-viewimagedesc", this.onImage && this.imageDescURL !== "");
  },

  initMiscItems: function CM_initMiscItems() {
    // Use "Bookmark This Link" if on a link.
    this.showItem("context-bookmarkpage",
                  !(this.isContentSelected || this.onTextInput || this.onLink ||
                    this.onImage || this.onVideo || this.onAudio || this.onSocial ||
                    this.onCanvas));
    this.showItem("context-bookmarklink", (this.onLink && !this.onMailtoLink &&
                                           !this.onSocial) || this.onPlainTextLink);
    this.showItem("context-keywordfield",
                  this.onTextInput && this.onKeywordField);
    this.showItem("frame", this.inFrame);

    let showSearchSelect = (this.isTextSelected || this.onLink) && !this.onImage;
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
      if (BrowserUtils.mimeTypeIsTextBased(this.target.ownerDocument.contentType))
        this.isFrameImage.removeAttribute('hidden');
      else
        this.isFrameImage.setAttribute('hidden', 'true');
    }

    // BiDi UI
    this.showItem("context-sep-bidi", !this.onNumeric && top.gBidiUI);
    this.showItem("context-bidi-text-direction-toggle",
                  this.onTextInput && !this.onNumeric && top.gBidiUI);
    this.showItem("context-bidi-page-direction-toggle",
                  !this.onTextInput && top.gBidiUI);

    // SocialMarks. Marks does not work with text selections, only links. If
    // there is more than MENU_LIMIT providers, we show a submenu for them,
    // otherwise we have a menuitem per provider (added in SocialMarks class).
    let markProviders = SocialMarks.getProviders();
    let enablePageMarks = markProviders.length > 0 && !(this.onLink || this.onImage
                            || this.onVideo || this.onAudio);
    this.showItem("context-markpageMenu", enablePageMarks && markProviders.length > SocialMarks.MENU_LIMIT);
    let enablePageMarkItems = enablePageMarks && markProviders.length <= SocialMarks.MENU_LIMIT;
    let linkmenus = document.getElementsByClassName("context-markpage");
    [m.hidden = !enablePageMarkItems for (m of linkmenus)];

    let enableLinkMarks = markProviders.length > 0 &&
                            ((this.onLink && !this.onMailtoLink) || this.onPlainTextLink);
    this.showItem("context-marklinkMenu", enableLinkMarks && markProviders.length > SocialMarks.MENU_LIMIT);
    let enableLinkMarkItems = enableLinkMarks && markProviders.length <= SocialMarks.MENU_LIMIT;
    linkmenus = document.getElementsByClassName("context-marklink");
    [m.hidden = !enableLinkMarkItems for (m of linkmenus)];

    // SocialShare
    let shareButton = SocialShare.shareButton;
    let shareEnabled = shareButton && !shareButton.disabled && !this.onSocial;
    let pageShare = shareEnabled && !(this.isContentSelected ||
                            this.onTextInput || this.onLink || this.onImage ||
                            this.onVideo || this.onAudio || this.onCanvas);
    this.showItem("context-sharepage", pageShare);
    this.showItem("context-shareselect", shareEnabled && this.isContentSelected);
    this.showItem("context-sharelink", shareEnabled && (this.onLink || this.onPlainTextLink) && !this.onMailtoLink);
    this.showItem("context-shareimage", shareEnabled && this.onImage);
    this.showItem("context-sharevideo", shareEnabled && this.onVideo);
    this.setItemAttr("context-sharevideo", "disabled", !this.mediaURL);
  },

  initSpellingItems: function() {
    var canSpell = InlineSpellCheckerUI.canSpellCheck &&
                   !InlineSpellCheckerUI.initialSpellCheckPending &&
                   this.canSpellCheck;
    var onMisspelling = InlineSpellCheckerUI.overMisspelling;
    var showUndo = canSpell && InlineSpellCheckerUI.canUndo();
    this.showItem("spell-check-enabled", canSpell);
    this.showItem("spell-separator", canSpell || this.onEditableArea);
    document.getElementById("spell-check-enabled")
            .setAttribute("checked", canSpell && InlineSpellCheckerUI.enabled);

    this.showItem("spell-add-to-dictionary", onMisspelling);
    this.showItem("spell-undo-add-to-dictionary", showUndo);

    // suggestion list
    this.showItem("spell-suggestions-separator", onMisspelling || showUndo);
    if (onMisspelling) {
      var suggestionsSeparator =
        document.getElementById("spell-add-to-dictionary");
      var numsug =
        InlineSpellCheckerUI.addSuggestionsToMenu(suggestionsSeparator.parentNode,
                                                  suggestionsSeparator, 5);
      this.showItem("spell-no-suggestions", numsug == 0);
    }
    else
      this.showItem("spell-no-suggestions", false);

    // dictionary list
    this.showItem("spell-dictionaries", canSpell && InlineSpellCheckerUI.enabled);
    if (canSpell) {
      var dictMenu = document.getElementById("spell-dictionaries-menu");
      var dictSep = document.getElementById("spell-language-separator");
      let count = InlineSpellCheckerUI.addDictionaryListToMenu(dictMenu, dictSep);
      this.showItem(dictSep, count > 0);
      this.showItem("spell-add-dictionaries-main", false);
    }
    else if (this.onEditableArea) {
      // when there is no spellchecker but we might be able to spellcheck
      // add the add to dictionaries item. This will ensure that people
      // with no dictionaries will be able to download them
      this.showItem("spell-add-dictionaries-main", true);
    }
    else
      this.showItem("spell-add-dictionaries-main", false);
  },

  initClipboardItems: function() {
    // Copy depends on whether there is selected text.
    // Enabling this context menu item is now done through the global
    // command updating system
    // this.setItemAttr( "context-copy", "disabled", !this.isTextSelected() );
    goUpdateGlobalEditMenuItems();

    this.showItem("context-undo", this.onTextInput);
    this.showItem("context-sep-undo", this.onTextInput);
    this.showItem("context-cut", this.onTextInput);
    this.showItem("context-copy",
                  this.isContentSelected || this.onTextInput);
    this.showItem("context-paste", this.onTextInput);
    this.showItem("context-delete", this.onTextInput);
    this.showItem("context-sep-paste", this.onTextInput);
    this.showItem("context-selectall", !(this.onLink || this.onImage ||
                                         this.onVideo || this.onAudio ||
                                         this.inSyntheticDoc) ||
                                       this.isDesignMode);
    this.showItem("context-sep-selectall", this.isContentSelected );

    // XXX dr
    // ------
    // nsDocumentViewer.cpp has code to determine whether we're
    // on a link or an image. we really ought to be using that...

    // Copy email link depends on whether we're on an email link.
    this.showItem("context-copyemail", this.onMailtoLink);

    // Copy link location depends on whether we're on a non-mailto link.
    this.showItem("context-copylink", this.onLink && !this.onMailtoLink);
    this.showItem("context-sep-copylink", this.onLink &&
                  (this.onImage || this.onVideo || this.onAudio));

#ifdef CONTEXT_COPY_IMAGE_CONTENTS
    // Copy image contents depends on whether we're on an image.
    this.showItem("context-copyimage-contents", this.onImage);
#endif
    // Copy image location depends on whether we're on an image.
    this.showItem("context-copyimage", this.onImage);
    this.showItem("context-copyvideourl", this.onVideo);
    this.showItem("context-copyaudiourl", this.onAudio);
    this.setItemAttr("context-copyvideourl",  "disabled", !this.mediaURL);
    this.setItemAttr("context-copyaudiourl",  "disabled", !this.mediaURL);
    this.showItem("context-sep-copyimage", this.onImage ||
                  this.onVideo || this.onAudio);
  },

  initMediaPlayerItems: function() {
    var onMedia = (this.onVideo || this.onAudio);
    // Several mutually exclusive items... play/pause, mute/unmute, show/hide
    this.showItem("context-media-play",  onMedia && (this.target.paused || this.target.ended));
    this.showItem("context-media-pause", onMedia && !this.target.paused && !this.target.ended);
    this.showItem("context-media-mute",   onMedia && !this.target.muted);
    this.showItem("context-media-unmute", onMedia && this.target.muted);
    this.showItem("context-media-playbackrate", onMedia);
    this.showItem("context-media-showcontrols", onMedia && !this.target.controls);
    this.showItem("context-media-hidecontrols", onMedia && this.target.controls);
    this.showItem("context-video-fullscreen", this.onVideo && this.target.ownerDocument.mozFullScreenElement == null);
    var statsShowing = this.onVideo && this.target.mozMediaStatisticsShowing;
    this.showItem("context-video-showstats", this.onVideo && this.target.controls && !statsShowing);
    this.showItem("context-video-hidestats", this.onVideo && this.target.controls && statsShowing);
    this.showItem("context-media-eme-learnmore", this.onDRMMedia);
    this.showItem("context-media-eme-separator", this.onDRMMedia);

    // Disable them when there isn't a valid media source loaded.
    if (onMedia) {
      this.setItemAttr("context-media-playbackrate-050x", "checked", this.target.playbackRate == 0.5);
      this.setItemAttr("context-media-playbackrate-100x", "checked", this.target.playbackRate == 1.0);
      this.setItemAttr("context-media-playbackrate-150x", "checked", this.target.playbackRate == 1.5);
      this.setItemAttr("context-media-playbackrate-200x", "checked", this.target.playbackRate == 2.0);
      var hasError = this.target.error != null ||
                     this.target.networkState == this.target.NETWORK_NO_SOURCE;
      this.setItemAttr("context-media-play",  "disabled", hasError);
      this.setItemAttr("context-media-pause", "disabled", hasError);
      this.setItemAttr("context-media-mute",   "disabled", hasError);
      this.setItemAttr("context-media-unmute", "disabled", hasError);
      this.setItemAttr("context-media-playbackrate", "disabled", hasError);
      this.setItemAttr("context-media-playbackrate-050x", "disabled", hasError);
      this.setItemAttr("context-media-playbackrate-100x", "disabled", hasError);
      this.setItemAttr("context-media-playbackrate-150x", "disabled", hasError);
      this.setItemAttr("context-media-playbackrate-200x", "disabled", hasError);
      this.setItemAttr("context-media-showcontrols", "disabled", hasError);
      this.setItemAttr("context-media-hidecontrols", "disabled", hasError);
      if (this.onVideo) {
        let canSaveSnapshot = !this.onDRMMedia && this.target.readyState >= this.target.HAVE_CURRENT_DATA;
        this.setItemAttr("context-video-saveimage",  "disabled", !canSaveSnapshot);
        this.setItemAttr("context-video-fullscreen", "disabled", hasError);
        this.setItemAttr("context-video-showstats", "disabled", hasError);
        this.setItemAttr("context-video-hidestats", "disabled", hasError);
      }
    }
    this.showItem("context-media-sep-commands",  onMedia);
  },

  initClickToPlayItems: function() {
    this.showItem("context-ctp-play", this.onCTPPlugin);
    this.showItem("context-ctp-hide", this.onCTPPlugin);
    this.showItem("context-sep-ctp", this.onCTPPlugin);
  },

  inspectNode: function CM_inspectNode() {
    let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
    let gBrowser = this.browser.ownerDocument.defaultView.gBrowser;
    let tt = devtools.TargetFactory.forTab(gBrowser.selectedTab);
    return gDevTools.showToolbox(tt, "inspector").then(function(toolbox) {
      let inspector = toolbox.getCurrentPanel();
      if (this.isRemote) {
        this.browser.messageManager.sendAsyncMessage("debug:inspect", {}, {node: this.target});
        inspector.walker.findInspectingNode().then(nodeFront => {
          inspector.selection.setNodeFront(nodeFront, "browser-context-menu");
        });
      } else {
        inspector.selection.setNode(this.target, "browser-context-menu");
      }
    }.bind(this));
  },

  // Set various context menu attributes based on the state of the world.
  setTarget: function (aNode, aRangeParent, aRangeOffset) {
    // gContextMenuContentData.isRemote tells us if the event came from a remote
    // process. gContextMenuContentData can be null if something (like tests)
    // opens the context menu directly.
    let editFlags;
    this.isRemote = gContextMenuContentData && gContextMenuContentData.isRemote;
    if (this.isRemote) {
      aNode = gContextMenuContentData.event.target;
      aRangeParent = gContextMenuContentData.event.rangeParent;
      aRangeOffset = gContextMenuContentData.event.rangeOffset;
      editFlags = gContextMenuContentData.editFlags;
    }

    const xulNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    if (aNode.namespaceURI == xulNS ||
        aNode.nodeType == Node.DOCUMENT_NODE) {
      this.shouldDisplay = false;
      return;
    }

    // Initialize contextual info.
    this.onImage           = false;
    this.onLoadedImage     = false;
    this.onCompletedImage  = false;
    this.imageDescURL      = "";
    this.onCanvas          = false;
    this.onVideo           = false;
    this.onAudio           = false;
    this.onDRMMedia        = false;
    this.onTextInput       = false;
    this.onNumeric         = false;
    this.onKeywordField    = false;
    this.mediaURL          = "";
    this.onLink            = false;
    this.onMailtoLink      = false;
    this.onSaveableLink    = false;
    this.link              = null;
    this.linkURL           = "";
    this.linkURI           = null;
    this.linkText          = "";
    this.linkProtocol      = "";
    this.linkDownload      = "";
    this.linkHasNoReferrer = false;
    this.onMathML          = false;
    this.inFrame           = false;
    this.inSrcdocFrame     = false;
    this.inSyntheticDoc    = false;
    this.hasBGImage        = false;
    this.bgImageURL        = "";
    this.onEditableArea    = false;
    this.isDesignMode      = false;
    this.onCTPPlugin       = false;
    this.canSpellCheck     = false;
    this.textSelected      = getBrowserSelection();
    this.isTextSelected    = this.textSelected.length != 0;

    // Remember the node that was clicked.
    this.target = aNode;

    let [elt, win] = BrowserUtils.getFocusSync(document);
    this.focusedWindow = win;
    this.focusedElement = elt;

    let ownerDoc = this.target.ownerDocument;
    this.ownerDoc = ownerDoc;

    // If this is a remote context menu event, use the information from
    // gContextMenuContentData instead.
    if (this.isRemote) {
      this.browser = gContextMenuContentData.browser;
      this.principal = gContextMenuContentData.principal;
      this.frameOuterWindowID = gContextMenuContentData.frameOuterWindowID;
    } else {
      editFlags = SpellCheckHelper.isEditable(this.target, window);
      this.browser = ownerDoc.defaultView
                             .QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIWebNavigation)
                             .QueryInterface(Ci.nsIDocShell)
                             .chromeEventHandler;
      this.principal = ownerDoc.nodePrincipal;
      this.frameOuterWindowID = ownerDoc.defaultView
                                        .QueryInterface(Ci.nsIInterfaceRequestor)
                                        .getInterface(Ci.nsIDOMWindowUtils)
                                        .outerWindowID;
    }
    this.onSocial = !!this.browser.getAttribute("origin");

    // Check if we are in a synthetic document (stand alone image, video, etc.).
    this.inSyntheticDoc = ownerDoc.mozSyntheticDocument;
    // First, do checks for nodes that never have children.
    if (this.target.nodeType == Node.ELEMENT_NODE) {
      // See if the user clicked on an image.
      if (this.target instanceof Ci.nsIImageLoadingContent &&
          this.target.currentURI) {
        this.onImage = true;

        var request =
          this.target.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
        if (request && (request.imageStatus & request.STATUS_SIZE_AVAILABLE))
          this.onLoadedImage = true;
        if (request && (request.imageStatus & request.STATUS_LOAD_COMPLETE))
          this.onCompletedImage = true;

        this.mediaURL = this.target.currentURI.spec;

        var descURL = this.target.getAttribute("longdesc");
        if (descURL) {
          this.imageDescURL = makeURLAbsolute(ownerDoc.body.baseURI, descURL);
        }
      }
      else if (this.target instanceof HTMLCanvasElement) {
        this.onCanvas = true;
      }
      else if (this.target instanceof HTMLVideoElement) {
        let mediaURL = this.target.currentSrc || this.target.src;
        if (this.isMediaURLReusable(mediaURL)) {
          this.mediaURL = mediaURL;
        }
        if (this.target.isEncrypted) {
          this.onDRMMedia = true;
        }
        // Firefox always creates a HTMLVideoElement when loading an ogg file
        // directly. If the media is actually audio, be smarter and provide a
        // context menu with audio operations.
        if (this.target.readyState >= this.target.HAVE_METADATA &&
            (this.target.videoWidth == 0 || this.target.videoHeight == 0)) {
          this.onAudio = true;
        } else {
          this.onVideo = true;
        }
      }
      else if (this.target instanceof HTMLAudioElement) {
        this.onAudio = true;
        let mediaURL = this.target.currentSrc || this.target.src;
        if (this.isMediaURLReusable(mediaURL)) {
          this.mediaURL = mediaURL;
        }
        if (this.target.isEncrypted) {
          this.onDRMMedia = true;
        }
      }
      else if (editFlags & (SpellCheckHelper.INPUT | SpellCheckHelper.TEXTAREA)) {
        this.onTextInput = (editFlags & SpellCheckHelper.TEXTINPUT) !== 0;
        this.onNumeric = (editFlags & SpellCheckHelper.NUMERIC) !== 0;
        this.onEditableArea = (editFlags & SpellCheckHelper.EDITABLE) !== 0;
        if (this.onEditableArea) {
          if (this.isRemote) {
            InlineSpellCheckerUI.initFromRemote(gContextMenuContentData.spellInfo);
          }
          else {
            InlineSpellCheckerUI.init(this.target.QueryInterface(Ci.nsIDOMNSEditableElement).editor);
            InlineSpellCheckerUI.initFromEvent(aRangeParent, aRangeOffset);
          }
        }
        this.onKeywordField = (editFlags & SpellCheckHelper.KEYWORD);
      }
      else if (this.target instanceof HTMLHtmlElement) {
        var bodyElt = ownerDoc.body;
        if (bodyElt) {
          let computedURL;
          try {
            computedURL = this.getComputedURL(bodyElt, "background-image");
            this._hasMultipleBGImages = false;
          } catch (e) {
            this._hasMultipleBGImages = true;
          }
          if (computedURL) {
            this.hasBGImage = true;
            this.bgImageURL = makeURLAbsolute(bodyElt.baseURI,
                                              computedURL);
          }
        }
      }
      else if ((this.target instanceof HTMLEmbedElement ||
                this.target instanceof HTMLObjectElement ||
                this.target instanceof HTMLAppletElement) &&
               this.target.matches(":-moz-handler-clicktoplay")) {
        this.onCTPPlugin = true;
      }

      this.canSpellCheck = this._isSpellCheckEnabled(this.target);
    }
    else if (this.target.nodeType == Node.TEXT_NODE) {
      // For text nodes, look at the parent node to determine the spellcheck attribute.
      this.canSpellCheck = this.target.parentNode &&
                           this._isSpellCheckEnabled(this.target);
    }

    // Second, bubble out, looking for items of interest that can have childen.
    // Always pick the innermost link, background image, etc.
    const XMLNS = "http://www.w3.org/XML/1998/namespace";
    var elem = this.target;
    while (elem) {
      if (elem.nodeType == Node.ELEMENT_NODE) {
        // Link?
        if (!this.onLink &&
            // Be consistent with what hrefAndLinkNodeForClickEvent
            // does in browser.js
             ((elem instanceof HTMLAnchorElement && elem.href) ||
              (elem instanceof HTMLAreaElement && elem.href) ||
              elem instanceof HTMLLinkElement ||
              elem.getAttributeNS("http://www.w3.org/1999/xlink", "type") == "simple")) {

          // Target is a link or a descendant of a link.
          this.onLink = true;

          // Remember corresponding element.
          this.link = elem;
          this.linkURL = this.getLinkURL();
          this.linkURI = this.getLinkURI();
          this.linkText = this.getLinkText();
          this.linkProtocol = this.getLinkProtocol();
          this.onMailtoLink = (this.linkProtocol == "mailto");
          this.onSaveableLink = this.isLinkSaveable( this.link );
          this.linkHasNoReferrer = BrowserUtils.linkHasNoReferrer(elem);
          try {
            if (elem.download) {
              // Ignore download attribute on cross-origin links
              this.principal.checkMayLoad(this.linkURI, false, true);
              this.linkDownload = elem.download;
            }
          }
          catch (ex) {}
        }

        // Background image?  Don't bother if we've already found a
        // background image further down the hierarchy.  Otherwise,
        // we look for the computed background-image style.
        if (!this.hasBGImage &&
            !this._hasMultipleBGImages) {
          let bgImgUrl;
          try {
            bgImgUrl = this.getComputedURL(elem, "background-image");
            this._hasMultipleBGImages = false;
          } catch (e) {
            this._hasMultipleBGImages = true;
          }
          if (bgImgUrl) {
            this.hasBGImage = true;
            this.bgImageURL = makeURLAbsolute(elem.baseURI,
                                              bgImgUrl);
          }
        }
      }

      elem = elem.parentNode;
    }

    // See if the user clicked on MathML
    const NS_MathML = "http://www.w3.org/1998/Math/MathML";
    if ((this.target.nodeType == Node.TEXT_NODE &&
         this.target.parentNode.namespaceURI == NS_MathML)
         || (this.target.namespaceURI == NS_MathML))
      this.onMathML = true;

    // See if the user clicked in a frame.
    var docDefaultView = ownerDoc.defaultView;
    if (docDefaultView != docDefaultView.top) {
      this.inFrame = true;

      if (ownerDoc.isSrcdocDocument) {
          this.inSrcdocFrame = true;
      }
    }

    // if the document is editable, show context menu like in text inputs
    if (!this.onEditableArea) {
      if (editFlags & SpellCheckHelper.CONTENTEDITABLE) {
        // If this.onEditableArea is false but editFlags is CONTENTEDITABLE, then
        // the document itself must be editable.
        this.onTextInput       = true;
        this.onKeywordField    = false;
        this.onImage           = false;
        this.onLoadedImage     = false;
        this.onCompletedImage  = false;
        this.onMathML          = false;
        this.inFrame           = false;
        this.inSrcdocFrame     = false;
        this.hasBGImage        = false;
        this.isDesignMode      = true;
        this.onEditableArea = true;
        if (this.isRemote) {
          InlineSpellCheckerUI.initFromRemote(gContextMenuContentData.spellInfo);
        }
        else {
          var targetWin = ownerDoc.defaultView;
          var editingSession = targetWin.QueryInterface(Ci.nsIInterfaceRequestor)
                                        .getInterface(Ci.nsIWebNavigation)
                                        .QueryInterface(Ci.nsIInterfaceRequestor)
                                        .getInterface(Ci.nsIEditingSession);
          InlineSpellCheckerUI.init(editingSession.getEditorForWindow(targetWin));
          InlineSpellCheckerUI.initFromEvent(aRangeParent, aRangeOffset);
        }
        var canSpell = InlineSpellCheckerUI.canSpellCheck && this.canSpellCheck;
        this.showItem("spell-check-enabled", canSpell);
        this.showItem("spell-separator", canSpell);
      }
    }
  },

  // Returns the computed style attribute for the given element.
  getComputedStyle: function(aElem, aProp) {
    return aElem.ownerDocument
                .defaultView
                .getComputedStyle(aElem, "").getPropertyValue(aProp);
  },

  // Returns a "url"-type computed style attribute value, with the url() stripped.
  getComputedURL: function(aElem, aProp) {
    var url = aElem.ownerDocument
                   .defaultView.getComputedStyle(aElem, "")
                   .getPropertyCSSValue(aProp);
    if (url instanceof CSSValueList) {
      if (url.length != 1)
        throw "found multiple URLs";
      url = url[0];
    }
    return url.primitiveType == CSSPrimitiveValue.CSS_URI ?
           url.getStringValue() : null;
  },

  // Returns true if clicked-on link targets a resource that can be saved.
  isLinkSaveable: function(aLink) {
    // We don't do the Right Thing for news/snews yet, so turn them off
    // until we do.
    return this.linkProtocol && !(
             this.linkProtocol == "mailto"     ||
             this.linkProtocol == "javascript" ||
             this.linkProtocol == "news"       ||
             this.linkProtocol == "snews"      );
  },

  _isSpellCheckEnabled: function(aNode) {
    // We can always force-enable spellchecking on textboxes
    if (this.isTargetATextBox(aNode)) {
      return true;
    }
    // We can never spell check something which is not content editable
    var editable = aNode.isContentEditable;
    if (!editable && aNode.ownerDocument) {
      editable = aNode.ownerDocument.designMode == "on";
    }
    if (!editable) {
      return false;
    }
    // Otherwise make sure that nothing in the parent chain disables spellchecking
    return aNode.spellcheck;
  },

  _openLinkInParameters : function (extra) {
    let params = { charset: gContextMenuContentData.charSet,
                   referrerURI: gContextMenuContentData.documentURIObject,
                   referrerPolicy: gContextMenuContentData.referrerPolicy,
                   noReferrer: this.linkHasNoReferrer };
    for (let p in extra)
      params[p] = extra[p];
    return params;
  },

  // Open linked-to URL in a new window.
  openLink : function () {
    urlSecurityCheck(this.linkURL, this.principal);
    openLinkIn(this.linkURL, "window", this._openLinkInParameters());
  },

  // Open linked-to URL in a new private window.
  openLinkInPrivateWindow : function () {
    urlSecurityCheck(this.linkURL, this.principal);
    openLinkIn(this.linkURL, "window",
               this._openLinkInParameters({ private: true }));
  },

  // Open linked-to URL in a new tab.
  openLinkInTab: function() {
    urlSecurityCheck(this.linkURL, this.principal);
    let referrerURI = gContextMenuContentData.documentURIObject;

    // if the mixedContentChannel is present and the referring URI passes
    // a same origin check with the target URI, we can preserve the users
    // decision of disabling MCB on a page for it's child tabs.
    let persistAllowMixedContentInChildTab = false;

    if (this.browser.docShell && this.browser.docShell.mixedContentChannel) {
      const sm = Services.scriptSecurityManager;
      try {
        let targetURI = this.linkURI;
        sm.checkSameOriginURI(referrerURI, targetURI, false);
        persistAllowMixedContentInChildTab = true;
      }
      catch (e) { }
    }

    let params = this._openLinkInParameters({
      allowMixedContent: persistAllowMixedContentInChildTab,
    });
    openLinkIn(this.linkURL, "tab", params);
  },

  // open URL in current tab
  openLinkInCurrent: function() {
    urlSecurityCheck(this.linkURL, this.principal);
    openLinkIn(this.linkURL, "current", this._openLinkInParameters());
  },

  // Open frame in a new tab.
  openFrameInTab: function() {
    let referrer = gContextMenuContentData.referrer;
    openLinkIn(gContextMenuContentData.docLocation, "tab",
               { charset: gContextMenuContentData.charSet,
                 referrerURI: referrer ? makeURI(referrer) : null });
  },

  // Reload clicked-in frame.
  reloadFrame: function() {
    this.browser.messageManager.sendAsyncMessage("ContextMenu:ReloadFrame",
                                                 null, { target: this.target });
  },

  // Open clicked-in frame in its own window.
  openFrame: function() {
    let referrer = gContextMenuContentData.referrer;
    openLinkIn(gContextMenuContentData.docLocation, "window",
               { charset: gContextMenuContentData.charSet,
                 referrerURI: referrer ? makeURI(referrer) : null });
  },

  // Open clicked-in frame in the same window.
  showOnlyThisFrame: function() {
    urlSecurityCheck(gContextMenuContentData.docLocation,
                     this.browser.contentPrincipal,
                     Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT);
    let referrer = gContextMenuContentData.referrer;
    openUILinkIn(gContextMenuContentData.docLocation, "current",
                 { disallowInheritPrincipal: true,
                   referrerURI: referrer ? makeURI(referrer) : null });
  },

  reload: function(event) {
    if (this.onSocial) {
      // full reload of social provider
      Social._getProviderFromOrigin(this.browser.getAttribute("origin")).reload();
    } else {
      BrowserReloadOrDuplicate(event);
    }
  },

  // View Partial Source
  viewPartialSource: function(aContext) {
    var focusedWindow = document.commandDispatcher.focusedWindow;
    if (focusedWindow == window)
      focusedWindow = gBrowser.selectedBrowser.contentWindowAsCPOW;

    var docCharset = null;
    if (focusedWindow)
      docCharset = "charset=" + focusedWindow.document.characterSet;

    // "View Selection Source" and others such as "View MathML Source"
    // are mutually exclusive, with the precedence given to the selection
    // when there is one
    var reference = null;
    if (aContext == "selection")
      reference = focusedWindow.getSelection();
    else if (aContext == "mathml")
      reference = this.target;
    else
      throw "not reached";

    // unused (and play nice for fragments generated via XSLT too)
    var docUrl = null;
    window.openDialog("chrome://global/content/viewPartialSource.xul",
                      "_blank", "scrollbars,resizable,chrome,dialog=no",
                      docUrl, docCharset, reference, aContext);
  },

  // Open new "view source" window with the frame's URL.
  viewFrameSource: function() {
    BrowserViewSourceOfDocument(this.target.ownerDocument);
  },

  viewInfo: function() {
    BrowserPageInfo(this.target.ownerDocument.defaultView.top.document);
  },

  viewImageInfo: function() {
    BrowserPageInfo(this.target.ownerDocument.defaultView.top.document,
                    "mediaTab", this.target);
  },

  viewImageDesc: function(e) {
    urlSecurityCheck(this.imageDescURL,
                     this.browser.contentPrincipal,
                     Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT);
    openUILink(this.imageDescURL, e, { disallowInheritPrincipal: true,
                                       referrerURI: gContextMenuContentData.documentURIObject });
  },

  viewFrameInfo: function() {
    BrowserPageInfo(this.target.ownerDocument);
  },

  reloadImage: function() {
    urlSecurityCheck(this.mediaURL,
                     this.browser.contentPrincipal,
                     Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT);

    this.browser.messageManager.sendAsyncMessage("ContextMenu:ReloadImage",
                                                 null, { target: this.target });
  },

  _canvasToDataURL: function(target) {
    let mm = this.browser.messageManager;
    return new Promise(function(resolve) {
      mm.sendAsyncMessage("ContextMenu:Canvas:ToDataURL", {}, { target });

      let onMessage = (message) => {
        mm.removeMessageListener("ContextMenu:Canvas:ToDataURL:Result", onMessage);
        resolve(message.data.dataURL);
      };
      mm.addMessageListener("ContextMenu:Canvas:ToDataURL:Result", onMessage);
    });
  },

  // Change current window to the URL of the image, video, or audio.
  viewMedia: function(e) {
    let referrerURI = gContextMenuContentData.documentURIObject;
    if (this.onCanvas) {
      this._canvasToDataURL(this.target).then(function(dataURL) {
        openUILink(dataURL, e, { disallowInheritPrincipal: true,
                                 referrerURI: referrerURI });
      }, Cu.reportError);
    }
    else {
      urlSecurityCheck(this.mediaURL,
                       this.browser.contentPrincipal,
                       Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT);
      openUILink(this.mediaURL, e, { disallowInheritPrincipal: true,
                                     referrerURI: referrerURI });
    }
  },

  saveVideoFrameAsImage: function () {
    let mm = this.browser.messageManager;
    let name = "";
    if (this.mediaURL) {
      try {
        let uri = makeURI(this.mediaURL);
        let url = uri.QueryInterface(Ci.nsIURL);
        if (url.fileBaseName)
          name = decodeURI(url.fileBaseName) + ".jpg";
      } catch (e) { }
    }
    if (!name)
      name = "snapshot.jpg";

    mm.sendAsyncMessage("ContextMenu:SaveVideoFrameAsImage", {}, {
      target: this.target,
    });

    let onMessage = (message) => {
      mm.removeMessageListener("ContextMenu:SaveVideoFrameAsImage:Result", onMessage);
      let dataURL = message.data.dataURL;
      saveImageURL(dataURL, name, "SaveImageTitle", true, false,
                   document.documentURIObject, document);
    };
    mm.addMessageListener("ContextMenu:SaveVideoFrameAsImage:Result", onMessage);
  },

  leaveDOMFullScreen: function() {
    document.mozCancelFullScreen();
  },

  // Change current window to the URL of the background image.
  viewBGImage: function(e) {
    urlSecurityCheck(this.bgImageURL,
                     this.browser.contentPrincipal,
                     Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT);
    openUILink(this.bgImageURL, e, { disallowInheritPrincipal: true,
                                     referrerURI: gContextMenuContentData.documentURIObject });
  },

  disableSetDesktopBackground: function() {
    // Disable the Set as Desktop Background menu item if we're still trying
    // to load the image or the load failed.
    if (!(this.target instanceof Ci.nsIImageLoadingContent))
      return true;

    if (("complete" in this.target) && !this.target.complete)
      return true;

    if (this.target.currentURI.schemeIs("javascript"))
      return true;

    var request = this.target
                      .QueryInterface(Ci.nsIImageLoadingContent)
                      .getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
    if (!request)
      return true;

    return false;
  },

  setDesktopBackground: function() {
    // Paranoia: check disableSetDesktopBackground again, in case the
    // image changed since the context menu was initiated.
    if (this.disableSetDesktopBackground())
      return;

    var doc = this.target.ownerDocument;
    urlSecurityCheck(this.target.currentURI.spec, this.principal);

    // Confirm since it's annoying if you hit this accidentally.
    const kDesktopBackgroundURL = 
                  "chrome://browser/content/setDesktopBackground.xul";
#ifdef XP_MACOSX
    // On Mac, the Set Desktop Background window is not modal.
    // Don't open more than one Set Desktop Background window.
    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);
    var dbWin = wm.getMostRecentWindow("Shell:SetDesktopBackground");
    if (dbWin) {
      dbWin.gSetBackground.init(this.target);
      dbWin.focus();
    }
    else {
      openDialog(kDesktopBackgroundURL, "",
                 "centerscreen,chrome,dialog=no,dependent,resizable=no",
                 this.target);
    }
#else
    // On non-Mac platforms, the Set Wallpaper dialog is modal.
    openDialog(kDesktopBackgroundURL, "",
               "centerscreen,chrome,dialog,modal,dependent",
               this.target);
#endif
  },

  // Save URL of clicked-on frame.
  saveFrame: function () {
    saveDocument(this.target.ownerDocument);
  },

  // Helper function to wait for appropriate MIME-type headers and
  // then prompt the user with a file picker
  saveHelper: function(linkURL, linkText, dialogTitle, bypassCache, doc, docURI,
                       windowID, linkDownload) {
    // canonical def in nsURILoader.h
    const NS_ERROR_SAVE_LINK_AS_TIMEOUT = 0x805d0020;

    // an object to proxy the data through to
    // nsIExternalHelperAppService.doContent, which will wait for the
    // appropriate MIME-type headers and then prompt the user with a
    // file picker
    function saveAsListener() {}
    saveAsListener.prototype = {
      extListener: null, 

      onStartRequest: function saveLinkAs_onStartRequest(aRequest, aContext) {

        // if the timer fired, the error status will have been caused by that,
        // and we'll be restarting in onStopRequest, so no reason to notify
        // the user
        if (aRequest.status == NS_ERROR_SAVE_LINK_AS_TIMEOUT)
          return;

        timer.cancel();

        // some other error occured; notify the user...
        if (!Components.isSuccessCode(aRequest.status)) {
          try {
            const sbs = Cc["@mozilla.org/intl/stringbundle;1"].
                        getService(Ci.nsIStringBundleService);
            const bundle = sbs.createBundle(
                    "chrome://mozapps/locale/downloads/downloads.properties");

            const title = bundle.GetStringFromName("downloadErrorAlertTitle");
            const msg = bundle.GetStringFromName("downloadErrorGeneric");

            const promptSvc = Cc["@mozilla.org/embedcomp/prompt-service;1"].
                              getService(Ci.nsIPromptService);
            const wm = Cc["@mozilla.org/appshell/window-mediator;1"].
                       getService(Ci.nsIWindowMediator);
            let window = wm.getOuterWindowWithId(windowID);
            promptSvc.alert(window, title, msg);
          } catch (ex) {}
          return;
        }

        let extHelperAppSvc = 
          Cc["@mozilla.org/uriloader/external-helper-app-service;1"].
          getService(Ci.nsIExternalHelperAppService);
        let channel = aRequest.QueryInterface(Ci.nsIChannel);
        this.extListener =
          extHelperAppSvc.doContent(channel.contentType, aRequest, 
                                    null, true, window);
        this.extListener.onStartRequest(aRequest, aContext);
      }, 

      onStopRequest: function saveLinkAs_onStopRequest(aRequest, aContext, 
                                                       aStatusCode) {
        if (aStatusCode == NS_ERROR_SAVE_LINK_AS_TIMEOUT) {
          // do it the old fashioned way, which will pick the best filename
          // it can without waiting.
          saveURL(linkURL, linkText, dialogTitle, bypassCache, false, docURI,
                  doc);
        }
        if (this.extListener)
          this.extListener.onStopRequest(aRequest, aContext, aStatusCode);
      },

      onDataAvailable: function saveLinkAs_onDataAvailable(aRequest, aContext,
                                                           aInputStream,
                                                           aOffset, aCount) {
        this.extListener.onDataAvailable(aRequest, aContext, aInputStream,
                                         aOffset, aCount);
      }
    }

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
      } 
    }

    // if it we don't have the headers after a short time, the user 
    // won't have received any feedback from their click.  that's bad.  so
    // we give up waiting for the filename. 
    function timerCallback() {}
    timerCallback.prototype = {
      notify: function sLA_timer_notify(aTimer) {
        channel.cancel(NS_ERROR_SAVE_LINK_AS_TIMEOUT);
        return;
      }
    }

    // set up a channel to do the saving
    var ioService = Cc["@mozilla.org/network/io-service;1"].
                    getService(Ci.nsIIOService);
    var channel = ioService.newChannelFromURI2(makeURI(linkURL),
                                               null, // aLoadingNode
                                               this.principal, // aLoadingPrincipal
                                               null, // aTriggeringPrincipal
                                               Ci.nsILoadInfo.SEC_NORMAL,
                                               Ci.nsIContentPolicy.TYPE_OTHER);
    if (linkDownload)
      channel.contentDispositionFilename = linkDownload;
    if (channel instanceof Ci.nsIPrivateBrowsingChannel) {
      let docIsPrivate = PrivateBrowsingUtils.isBrowserPrivate(gBrowser.selectedBrowser);
      channel.setPrivate(docIsPrivate);
    }
    channel.notificationCallbacks = new callbacks();

    let flags = Ci.nsIChannel.LOAD_CALL_CONTENT_SNIFFERS;

    if (bypassCache)
      flags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;

    if (channel instanceof Ci.nsICachingChannel)
      flags |= Ci.nsICachingChannel.LOAD_BYPASS_LOCAL_CACHE_IF_BUSY;

    channel.loadFlags |= flags;

    if (channel instanceof Ci.nsIHttpChannel) {
      channel.referrer = docURI;
      if (channel instanceof Ci.nsIHttpChannelInternal)
        channel.forceAllowThirdPartyCookie = true;
    }

    // fallback to the old way if we don't see the headers quickly 
    var timeToWait = 
      gPrefService.getIntPref("browser.download.saveLinkAsFilenameTimeout");
    var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(new timerCallback(), timeToWait,
                           timer.TYPE_ONE_SHOT);

    // kick off the channel with our proxy object as the listener
    channel.asyncOpen(new saveAsListener(), null);
  },

  // Save URL of clicked-on link.
  saveLink: function() {
    urlSecurityCheck(this.linkURL, this.principal);
    this.saveHelper(this.linkURL, this.linkText, null, true, this.ownerDoc,
                    gContextMenuContentData.documentURIObject,
                    gContextMenuContentData.frameOuterWindowID,
                    this.linkDownload);
  },

  // Backwards-compatibility wrapper
  saveImage : function() {
    if (this.onCanvas || this.onImage)
        this.saveMedia();
  },

  // Save URL of the clicked upon image, video, or audio.
  saveMedia: function() {
    var doc =  this.target.ownerDocument;
    let referrerURI = gContextMenuContentData.documentURIObject;
    if (this.onCanvas) {
      // Bypass cache, since it's a data: URL.
      saveImageURL(this.target.toDataURL(), "canvas.png", "SaveImageTitle",
                   true, false, referrerURI, doc);
    }
    else if (this.onImage) {
      urlSecurityCheck(this.mediaURL, this.principal);
      saveImageURL(this.mediaURL, null, "SaveImageTitle", false,
                   false, referrerURI, doc, gContextMenuContentData.contentType,
                   gContextMenuContentData.contentDisposition);
    }
    else if (this.onVideo || this.onAudio) {
      urlSecurityCheck(this.mediaURL, this.principal);
      var dialogTitle = this.onVideo ? "SaveVideoTitle" : "SaveAudioTitle";
      this.saveHelper(this.mediaURL, null, dialogTitle, false, doc, referrerURI,
                      gContextMenuContentData.frameOuterWindowID, "");
    }
  },

  // Backwards-compatibility wrapper
  sendImage : function() {
    if (this.onCanvas || this.onImage)
        this.sendMedia();
  },

  sendMedia: function() {
    MailIntegration.sendMessage(this.mediaURL, "");
  },

  castVideo: function() {
    CastingApps.openExternal(this.target, window);
  },

  populateCastVideoMenu: function(popup) {
    let videoEl = this.target;
    popup.innerHTML = null;
    let doc = popup.ownerDocument;
    let services = CastingApps.getServicesForVideo(videoEl);
    services.forEach(service => {
      let item = doc.createElement("menuitem");
      item.setAttribute("label", service.friendlyName);
      item.addEventListener("command", event => {
        CastingApps.sendVideoToService(videoEl, service);
      });
      popup.appendChild(item);
    });
  },

  playPlugin: function() {
    gPluginHandler.contextMenuCommand(this.browser, this.target, "play");
  },

  hidePlugin: function() {
    gPluginHandler.contextMenuCommand(this.browser, this.target, "hide");
  },

  // Generate email address and put it on clipboard.
  copyEmail: function() {
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
      const textToSubURI = Cc["@mozilla.org/intl/texttosuburi;1"].
                           getService(Ci.nsITextToSubURI);
      addresses = textToSubURI.unEscapeURIForUI(gContextMenuContentData.charSet,
                                                addresses);
    }
    catch(ex) {
      // Do nothing.
    }

    var clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].
                    getService(Ci.nsIClipboardHelper);
    clipboard.copyString(addresses, document);
  },

  copyLink: function() {
    var clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].
                    getService(Ci.nsIClipboardHelper);
    clipboard.copyString(this.linkURL, document);
  },

  ///////////////
  // Utilities //
  ///////////////

  // Show/hide one item (specified via name or the item element itself).
  showItem: function(aItemOrId, aShow) {
    var item = aItemOrId.constructor == String ?
      document.getElementById(aItemOrId) : aItemOrId;
    if (item)
      item.hidden = !aShow;
  },

  // Set given attribute of specified context-menu item.  If the
  // value is null, then it removes the attribute (which works
  // nicely for the disabled attribute).
  setItemAttr: function(aID, aAttr, aVal ) {
    var elem = document.getElementById(aID);
    if (elem) {
      if (aVal == null) {
        // null indicates attr should be removed.
        elem.removeAttribute(aAttr);
      }
      else {
        // Set attr=val.
        elem.setAttribute(aAttr, aVal);
      }
    }
  },

  // Set context menu attribute according to like attribute of another node
  // (such as a broadcaster).
  setItemAttrFromNode: function(aItem_id, aAttr, aOther_id) {
    var elem = document.getElementById(aOther_id);
    if (elem && elem.getAttribute(aAttr) == "true")
      this.setItemAttr(aItem_id, aAttr, "true");
    else
      this.setItemAttr(aItem_id, aAttr, null);
  },

  // Temporary workaround for DOM api not yet implemented by XUL nodes.
  cloneNode: function(aItem) {
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

  // Generate fully qualified URL for clicked-on link.
  getLinkURL: function() {
    var href = this.link.href;  
    if (href)
      return href;

    href = this.link.getAttributeNS("http://www.w3.org/1999/xlink",
                                    "href");

    if (!href || !href.match(/\S/)) {
      // Without this we try to save as the current doc,
      // for example, HTML case also throws if empty
      throw "Empty href";
    }

    return makeURLAbsolute(this.link.baseURI, href);
  },

  getLinkURI: function() {
    try {
      return makeURI(this.linkURL);
    }
    catch (ex) {
     // e.g. empty URL string
    }

    return null;
  },

  getLinkProtocol: function() {
    if (this.linkURI)
      return this.linkURI.scheme; // can be |undefined|

    return null;
  },

  // Get text of link.
  getLinkText: function() {
    var text = gatherTextUnder(this.link);
    if (!text || !text.match(/\S/)) {
      text = this.link.getAttribute("title");
      if (!text || !text.match(/\S/)) {
        text = this.link.getAttribute("alt");
        if (!text || !text.match(/\S/))
          text = this.linkURL;
      }
    }

    return text;
  },

  // Returns true if anything is selected.
  isContentSelection: function() {
    return !this.focusedWindow.getSelection().isCollapsed;
  },

  isMediaURLReusable: function(aURL) {
    return !/^(?:blob|mediasource):/.test(aURL);
  },

  toString: function () {
    return "contextMenu.target     = " + this.target + "\n" +
           "contextMenu.onImage    = " + this.onImage + "\n" +
           "contextMenu.onLink     = " + this.onLink + "\n" +
           "contextMenu.link       = " + this.link + "\n" +
           "contextMenu.inFrame    = " + this.inFrame + "\n" +
           "contextMenu.hasBGImage = " + this.hasBGImage + "\n";
  },

  isTargetATextBox: function(node) {
    if (node instanceof HTMLInputElement)
      return node.mozIsTextField(false);

    return (node instanceof HTMLTextAreaElement);
  },

  // Determines whether or not the separator with the specified ID should be
  // shown or not by determining if there are any non-hidden items between it
  // and the previous separator.
  shouldShowSeparator: function (aSeparatorID) {
    var separator = document.getElementById(aSeparatorID);
    if (separator) {
      var sibling = separator.previousSibling;
      while (sibling && sibling.localName != "menuseparator") {
        if (!sibling.hidden)
          return true;
        sibling = sibling.previousSibling;
      }
    }
    return false;
  },

  addDictionaries: function() {
    var uri = formatURL("browser.dictionaries.download.url", true);

    var locale = "-";
    try {
      locale = gPrefService.getComplexValue("intl.accept_languages",
                                            Ci.nsIPrefLocalizedString).data;
    }
    catch (e) { }

    var version = "-";
    try {
      version = Cc["@mozilla.org/xre/app-info;1"].
                getService(Ci.nsIXULAppInfo).version;
    }
    catch (e) { }

    uri = uri.replace(/%LOCALE%/, escape(locale)).replace(/%VERSION%/, version);

    var newWindowPref = gPrefService.getIntPref("browser.link.open_newwindow");
    var where = newWindowPref == 3 ? "tab" : "window";

    openUILinkIn(uri, where);
  },

  bookmarkThisPage: function CM_bookmarkThisPage() {
    window.top.PlacesCommandHook.bookmarkPage(this.browser, PlacesUtils.bookmarksMenuFolderId, true);
  },

  bookmarkLink: function CM_bookmarkLink() {
    window.top.PlacesCommandHook.bookmarkLink(PlacesUtils.bookmarksMenuFolderId,
                                              this.linkURL, this.linkText);
  },

  addBookmarkForFrame: function CM_addBookmarkForFrame() {
    var doc = this.target.ownerDocument;
    var uri = doc.documentURIObject;

    var itemId = PlacesUtils.getMostRecentBookmarkForURI(uri);
    if (itemId == -1) {
      var title = doc.title;
      var description = PlacesUIUtils.getDescriptionFromDocument(doc);
      PlacesUIUtils.showBookmarkDialog({ action: "add"
                                       , type: "bookmark"
                                       , uri: uri
                                       , title: title
                                       , description: description
                                       , hiddenRows: [ "description"
                                                     , "location"
                                                     , "loadInSidebar"
                                                     , "keyword" ]
                                       }, window.top);
    }
    else {
      PlacesUIUtils.showBookmarkDialog({ action: "edit"
                                       , type: "bookmark"
                                       , itemId: itemId
                                       }, window.top);
    }
  },
  markLink: function CM_markLink(origin) {
    // send link to social, if it is the page url linkURI will be null
    SocialMarks.markLink(origin, this.linkURI ? this.linkURI.spec : null, this.target);
  },
  shareLink: function CM_shareLink() {
    SocialShare.sharePage(null, { url: this.linkURI.spec }, this.target);
  },

  shareImage: function CM_shareImage() {
    SocialShare.sharePage(null, { url: this.imageURL, previews: [ this.mediaURL ] }, this.target);
  },

  shareVideo: function CM_shareVideo() {
    SocialShare.sharePage(null, { url: this.mediaURL, source: this.mediaURL }, this.target);
  },

  shareSelect: function CM_shareSelect() {
    SocialShare.sharePage(null, { url: this.browser.currentURI.spec, text: this.textSelected }, this.target);
  },

  savePageAs: function CM_savePageAs() {
    saveDocument(this.browser.contentDocumentAsCPOW);
  },

  printFrame: function CM_printFrame() {
    PrintUtils.print(this.target.ownerDocument.defaultView, this.browser);
  },

  switchPageDirection: function CM_switchPageDirection() {
    this.browser.messageManager.sendAsyncMessage("SwitchDocumentDirection");
  },

  mediaCommand : function CM_mediaCommand(command, data) {
    let mm = this.browser.messageManager;
    mm.sendAsyncMessage("ContextMenu:MediaCommand",
                        {command: command, data: data},
                        {element: this.target});
  },

  copyMediaLocation : function () {
    var clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].
                    getService(Ci.nsIClipboardHelper);
    clipboard.copyString(this.mediaURL, document);
  },

  drmLearnMore: function(aEvent) {
    let drmInfoURL = Services.urlFormatter.formatURLPref("app.support.baseURL") + "drm-content";
    let dest = whereToOpenLink(aEvent);
    // Don't ever want this to open in the same tab as it'll unload the
    // DRM'd video, which is going to be a bad idea in most cases.
    if (dest == "current") {
      dest = "tab";
    }
    openUILinkIn(drmInfoURL, dest);
  },

  get imageURL() {
    if (this.onImage)
      return this.mediaURL;
    return "";
  },

  // Formats the 'Search <engine> for "<selection or link text>"' context menu.
  formatSearchContextItem: function() {
    var menuItem = document.getElementById("context-searchselect");
    let selectedText = this.isTextSelected ? this.textSelected : this.linkText;

    // Store searchTerms in context menu item so we know what to search onclick
    menuItem.searchTerms = selectedText;

    // If the JS character after our truncation point is a trail surrogate,
    // include it in the truncated string to avoid splitting a surrogate pair.
    if (selectedText.length > 15) {
      let truncLength = 15;
      let truncChar = selectedText[15].charCodeAt(0);
      if (truncChar >= 0xDC00 && truncChar <= 0xDFFF)
        truncLength++;
      selectedText = selectedText.substr(0,truncLength) + this.ellipsis;
    }

    // Use the current engine if the search bar is visible, the default
    // engine otherwise.
    var engineName = "";
    var ss = Cc["@mozilla.org/browser/search-service;1"].
             getService(Ci.nsIBrowserSearchService);
    if (isElementVisible(BrowserSearch.searchBar))
      engineName = ss.currentEngine.name;
    else
      engineName = ss.defaultEngine.name;

    // format "Search <engine> for <selection>" string to show in menu
    var menuLabel = gNavigatorBundle.getFormattedString("contextMenuSearch",
                                                        [engineName,
                                                         selectedText]);
    menuItem.label = menuLabel;
    menuItem.accessKey = gNavigatorBundle.getString("contextMenuSearch.accesskey");
  },

  _getTelemetryClickInfo: function(aXulMenu) {
    this._onPopupHiding = () => {
      aXulMenu.ownerDocument.removeEventListener("command", activationHandler, true);
      aXulMenu.removeEventListener("popuphiding", this._onPopupHiding, true);
      delete this._onPopupHiding;

      let eventKey = [
          this._telemetryPageContext,
          this._telemetryHadCustomItems ? "withcustom" : "withoutcustom"
      ];
      let target = this._telemetryClickID || "close-without-interaction";
      BrowserUITelemetry.registerContextMenuInteraction(eventKey, target);
    };
    let activationHandler = (e) => {
      // Deal with command events being routed to command elements; figure out
      // what triggered the event (which will have the right e.target)
      if (e.sourceEvent) {
        e = e.sourceEvent;
      }
      // Target should be in the menu (this catches using shortcuts for items
      // not in the menu while the menu is up)
      if (!aXulMenu.contains(e.target)) {
        return;
      }

      // Check if this is a page menu item:
      if (e.target.hasAttribute(PageMenuParent.GENERATEDITEMID_ATTR)) {
        this._telemetryClickID = "custom-page-item";
      } else {
        this._telemetryClickID = (e.target.id || "unknown").replace(/^context-/i, "");
      }
    };
    aXulMenu.ownerDocument.addEventListener("command", activationHandler, true);
    aXulMenu.addEventListener("popuphiding", this._onPopupHiding, true);
  },

  _getTelemetryPageContextInfo: function() {
    let rv = [];
    for (let k of ["isContentSelected", "onLink", "onImage", "onCanvas", "onVideo", "onAudio",
                   "onTextInput", "onSocial"]) {
      if (this[k]) {
        rv.push(k.replace(/^(?:is|on)(.)/, (match, firstLetter) => firstLetter.toLowerCase()));
      }
    }
    if (!rv.length) {
      rv.push('other');
    }

    return JSON.stringify(rv);
  },

  _checkTelemetryForMenu: function(aXulMenu) {
    this._telemetryClickID = null;
    this._telemetryPageContext = this._getTelemetryPageContextInfo();
    this._telemetryHadCustomItems = this.hasPageMenu;
    this._getTelemetryClickInfo(aXulMenu);
  },
};
