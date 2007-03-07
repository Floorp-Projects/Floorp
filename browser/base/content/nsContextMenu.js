# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Blake Ross <blake@cs.stanford.edu>
#   David Hyatt <hyatt@mozilla.org>
#   Peter Annema <disttsc@bart.nl>
#   Dean Tessman <dean_tessman@hotmail.com>
#   Kevin Puetz <puetzk@iastate.edu>
#   Ben Goodger <ben@netscape.com>
#   Pierre Chanial <chanial@noos.fr>
#   Jason Eager <jce2@po.cwru.edu>
#   Joe Hewitt <hewitt@netscape.com>
#   Alec Flett <alecf@netscape.com>
#   Asaf Romano <mozilla.mano@sent.com>
#   Jason Barnabe <jason_barnabe@fastmail.fm>
#   Peter Parente <parente@cs.unc.edu>
#   Giorgio Maone <g.maone@informaction.com>
#   Tom Germeau <tom.germeau@epigoon.com>
#   Jesse Ruderman <jruderman@gmail.com>
#   Joe Hughes <joe@retrovirus.com>
#   Pamela Greene <pamg.bugs@gmail.com>
#   Michael Ventnor <ventnors_dogs234@yahoo.com.au>
#   Simon Bünzli <zeniko@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

function nsContextMenu(aXulMenu, aBrowser) {
  this.target            = null;
  this.browser           = null;
  this.menu              = null;
  this.onTextInput       = false;
  this.onKeywordField    = false;
  this.onImage           = false;
  this.onLoadedImage     = false;
  this.onLink            = false;
  this.onMailtoLink      = false;
  this.onSaveableLink    = false;
  this.onMetaDataItem    = false;
  this.onMathML          = false;
  this.link              = false;
  this.linkURL           = "";
  this.linkURI           = null;
  this.linkProtocol      = null;
  this.inFrame           = false;
  this.hasBGImage        = false;
  this.isTextSelected    = false;
  this.isContentSelected = false;
  this.inDirList         = false;
  this.shouldDisplay     = true;
  this.isDesignMode      = false;
  this.possibleSpellChecking = false;

  // Initialize new menu.
  this.initMenu(aXulMenu, aBrowser);
}

// Prototype for nsContextMenu "class."
nsContextMenu.prototype = {
  // onDestroy is a no-op at this point.
  onDestroy: function () {
  },

  // Initialize context menu.
  initMenu: function (aPopup, aBrowser) {
    this.menu = aPopup;
    this.browser = aBrowser;

    // Get contextual info.
    this.setTarget(document.popupNode, document.popupRangeParent,
                   document.popupRangeOffset);

    this.isTextSelected = this.isTextSelection();
    this.isContentSelected = this.isContentSelection();

    // Initialize (disable/remove) menu items.
    this.initItems();
  },

  initItems: function () {
    this.initOpenItems();
    this.initNavigationItems();
    this.initViewItems();
    this.initMiscItems();
    this.initSpellingItems();
    this.initSaveItems();
    this.initClipboardItems();
    this.initMetadataItems();
  },

  initOpenItems: function () {
    var shouldShow = this.onSaveableLink ||
                     (this.inDirList && this.onLink);
    this.showItem("context-openlink", shouldShow);
    this.showItem("context-openlinkintab", shouldShow);
    this.showItem("context-sep-open", shouldShow);
  },

  initNavigationItems: function() {
    var shouldShow = !(this.isContentSelected || this.onLink || this.onImage ||
                       this.onTextInput);
    this.showItem("context-back", shouldShow);
    this.showItem("context-forward", shouldShow);
    this.showItem("context-reload", shouldShow);
    this.showItem("context-stop", shouldShow);
    this.showItem("context-sep-stop", shouldShow);

    // XXX: Stop is determined in browser.js; the canStop broadcaster is broken
    //this.setItemAttrFromNode( "context-stop", "disabled", "canStop" );
  },

  initSaveItems: function() {
    var shouldShow = !(this.inDirList || this.isContentSelected ||
                       this.onTextInput || this.onLink || this.onImage);
    this.showItem("context-savepage", shouldShow);
    this.showItem("context-sendpage", shouldShow);

    // Save+Send link depends on whether we're in a link.
    this.showItem("context-savelink", this.onSaveableLink);
    this.showItem("context-sendlink", this.onSaveableLink);

    // Save+Send image depends on whether we're on an image.
    this.showItem("context-saveimage", this.onLoadedImage);
    this.showItem("context-sendimage", this.onImage);
  },

  initViewItems: function () {
    // View source is always OK, unless in directory listing.
    this.showItem("context-viewpartialsource-selection",
                  this.isContentSelected);
    this.showItem("context-viewpartialsource-mathml",
                  this.onMathML && !this.isContentSelected);

    var shouldShow = !(this.inDirList || this.isContentSelected ||
                       this.onImage || this.onLink || this.onTextInput);
    this.showItem("context-viewsource", shouldShow);
    this.showItem("context-viewinfo", shouldShow);

    this.showItem("context-sep-properties",
                  !(this.inDirList || this.isContentSelected ||
                    this.onTextInput));

    // Set as Desktop background depends on whether an image was clicked on,
    // and only works if we have a shell service.
    var haveSetDesktopBackground = false;
#ifdef HAVE_SHELL_SERVICE
    // Only enable Set as Desktop Background if we can get the shell service.
    var shell = getShellService();
    if (shell)
      haveSetDesktopBackground = true;
#endif
    this.showItem("context-setDesktopBackground",
                  haveSetDesktopBackground && this.onLoadedImage);

    if (haveSetDesktopBackground && this.onLoadedImage) {
      document.getElementById("context-setDesktopBackground")
              .disabled = this.disableSetDesktopBackground();
    }

    // View Image depends on whether an image was clicked on.
    this.showItem("context-viewimage",
                  this.onImage && (!this.onStandaloneImage || this.inFrame));

    // View background image depends on whether there is one.
    this.showItem("context-viewbgimage", shouldShow);
    this.showItem("context-sep-viewbgimage", shouldShow);
    document.getElementById("context-viewbgimage")
            .disabled = !this.hasBGImage;
  },

  initMiscItems: function() {
    // Use "Bookmark This Link" if on a link.
    this.showItem("context-bookmarkpage",
                  !(this.isContentSelected || this.onTextInput ||
                    this.onLink || this.onImage));
    this.showItem("context-bookmarklink", this.onLink && !this.onMailtoLink);
    this.showItem("context-searchselect", this.isTextSelected);
    this.showItem("context-keywordfield",
                  this.onTextInput && this.onKeywordField);
    this.showItem("frame", this.inFrame);
    this.showItem("frame-sep", this.inFrame);

    // BiDi UI
    this.showItem("context-sep-bidi", gBidiUI);
    this.showItem("context-bidi-text-direction-toggle",
                  this.onTextInput && gBidiUI);
    this.showItem("context-bidi-page-direction-toggle",
                  !this.onTextInput && gBidiUI);

    if (this.onImage) {
      var blockImage = document.getElementById("context-blockimage");

      var uri = this.target
                    .QueryInterface(Ci.nsIImageLoadingContent)
                    .currentURI;

      // this throws if the image URI doesn't have a host (eg, data: image URIs)
      // see bug 293758 for details
      var hostLabel = "";
      try {
        hostLabel = uri.host;
      } catch (ex) { }

      if (hostLabel) {
        var shortenedUriHost = hostLabel.replace(/^www\./i,"");
        if (shortenedUriHost.length > 15)
          shortenedUriHost = shortenedUriHost.substr(0,15) + "...";
        blockImage.label = gNavigatorBundle.getFormattedString("blockImages", [shortenedUriHost]);

        if (this.isImageBlocked())
          blockImage.setAttribute("checked", "true");
        else
          blockImage.removeAttribute("checked");
      }
    }

    // Only show the block image item if the image can be blocked
    this.showItem("context-blockimage", this.onImage && hostLabel);
  },

  initSpellingItems: function() {
    var canSpell = InlineSpellCheckerUI.canSpellCheck;
    var onMisspelling = InlineSpellCheckerUI.overMisspelling;
    this.showItem("spell-check-enabled", canSpell);
    this.showItem("spell-separator", canSpell || this.possibleSpellChecking);
    if (canSpell) {
      document.getElementById("spell-check-enabled")
              .setAttribute("checked", InlineSpellCheckerUI.enabled);
    }

    this.showItem("spell-add-to-dictionary", onMisspelling);

    // suggestion list
    this.showItem("spell-suggestions-separator", onMisspelling);
    if (onMisspelling) {
      var menu = document.getElementById("contentAreaContextMenu");
      var suggestionsSeparator =
        document.getElementById("spell-add-to-dictionary");
      var numsug = InlineSpellCheckerUI.addSuggestionsToMenu(menu, suggestionsSeparator, 5);
      this.showItem("spell-no-suggestions", numsug == 0);
    }
    else
      this.showItem("spell-no-suggestions", false);

    // dictionary list
    this.showItem("spell-dictionaries", InlineSpellCheckerUI.enabled);
    if (canSpell) {
      var dictMenu = document.getElementById("spell-dictionaries-menu");
      var dictSep = document.getElementById("spell-language-separator");
      InlineSpellCheckerUI.addDictionaryListToMenu(dictMenu, dictSep);
      this.showItem("spell-add-dictionaries-main", false);
    }
    else if (this.possibleSpellChecking) {
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
    this.showItem("context-selectall",
                  !(this.onLink || this.onImage) || this.isDesignMode);
    this.showItem("context-sep-selectall", this.isContentSelected );

    // XXX dr
    // ------
    // nsDocumentViewer.cpp has code to determine whether we're
    // on a link or an image. we really ought to be using that...

    // Copy email link depends on whether we're on an email link.
    this.showItem("context-copyemail", this.onMailtoLink);

    // Copy link location depends on whether we're on a link.
    this.showItem("context-copylink", this.onLink);
    this.showItem("context-sep-copylink", this.onLink && this.onImage);

#ifdef CONTEXT_COPY_IMAGE_CONTENTS
    // Copy image contents depends on whether we're on an image.
    this.showItem("context-copyimage-contents", this.onImage);
#endif
    // Copy image location depends on whether we're on an image.
    this.showItem("context-copyimage", this.onImage);
    this.showItem("context-sep-copyimage", this.onImage);
  },

  initMetadataItems: function() {
    // Show if user clicked on something which has metadata.
    this.showItem("context-metadata", this.onMetaDataItem);
  },

  // Set various context menu attributes based on the state of the world.
  setTarget: function (aNode, aRangeParent, aRangeOffset) {
    const xulNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    if (aNode.namespaceURI == xulNS) {
      this.shouldDisplay = false;
      return;
    }

    // Initialize contextual info.
    this.onImage           = false;
    this.onLoadedImage     = false;
    this.onStandaloneImage = false;
    this.onMetaDataItem    = false;
    this.onTextInput       = false;
    this.onKeywordField    = false;
    this.imageURL          = "";
    this.onLink            = false;
    this.linkURL           = "";
    this.linkURI           = null;
    this.linkProtocol      = "";
    this.onMathML          = false;
    this.inFrame           = false;
    this.hasBGImage        = false;
    this.bgImageURL        = "";
    this.possibleSpellChecking = false;

    // Clear any old spellchecking items from the menu, this used to
    // be in the menu hiding code but wasn't getting called in all
    // situations. Here, we can ensure it gets cleaned up any time the
    // menu is shown. Note: must be before uninit because that clears the
    // internal vars
    InlineSpellCheckerUI.clearSuggestionsFromMenu();
    InlineSpellCheckerUI.clearDictionaryListFromMenu();

    InlineSpellCheckerUI.uninit();

    // Remember the node that was clicked.
    this.target = aNode;

    // First, do checks for nodes that never have children.
    if (this.target.nodeType == Node.ELEMENT_NODE) {
      // See if the user clicked on an image.
      if (this.target instanceof Ci.nsIImageLoadingContent &&
          this.target.currentURI) {
        this.onImage = true;
        this.onMetaDataItem = true;
                
        var request =
          this.target.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
        if (request && (request.imageStatus & request.STATUS_SIZE_AVAILABLE))
          this.onLoadedImage = true;

        this.imageURL = this.target.currentURI.spec;
        if (this.target.ownerDocument instanceof ImageDocument)
          this.onStandaloneImage = true;
      }
      else if (this.target instanceof HTMLInputElement ) {
        this.onTextInput = this.isTargetATextBox(this.target);
        // allow spellchecking UI on all writable text boxes except passwords
        if (this.onTextInput && ! this.target.readOnly &&
            this.target.type != "password") {
          this.possibleSpellChecking = true;
          InlineSpellCheckerUI.init(this.target.QueryInterface(Ci.nsIDOMNSEditableElement).editor);
          InlineSpellCheckerUI.initFromEvent(aRangeParent, aRangeOffset);
        }
        this.onKeywordField = this.isTargetAKeywordField(this.target);
      }
      else if (this.target instanceof HTMLTextAreaElement) {
        this.onTextInput = true;
        if (!this.target.readOnly) {
          this.possibleSpellChecking = true;
          InlineSpellCheckerUI.init(this.target.QueryInterface(Ci.nsIDOMNSEditableElement).editor);
          InlineSpellCheckerUI.initFromEvent(aRangeParent, aRangeOffset);
        }
      }
      else if (this.target instanceof HTMLHtmlElement) {
        var bodyElt = this.target.ownerDocument.body;
        if (bodyElt) {
          var computedURL = this.getComputedURL(bodyElt, "background-image");
          if (computedURL) {
            this.hasBGImage = true;
            this.bgImageURL = makeURLAbsolute(bodyElt.baseURI,
                                              computedURL);
          }
        }
      }
      else if ("HTTPIndex" in content &&
               content.HTTPIndex instanceof Ci.nsIHTTPIndex) {
        this.inDirList = true;
        // Bubble outward till we get to an element with URL attribute
        // (which should be the href).
        var root = this.target;
        while (root && !this.link) {
          if (root.tagName == "tree") {
            // Hit root of tree; must have clicked in empty space;
            // thus, no link.
            break;
          }

          if (root.getAttribute("URL")) {
            // Build pseudo link object so link-related functions work.
            this.onLink = true;
            this.link = { href : root.getAttribute("URL"),
                          getAttribute: function (aAttr) {
                            if (aAttr == "title") {
                              return root.firstChild.firstChild
                                         .getAttribute("label");
                            }
                            else
                              return "";
                           }
                         };

            // If element is a directory, then you can't save it.
            this.onSaveableLink = root.getAttribute("container") != "true";
          }
          else
            root = root.parentNode;
        }
      }
    }

    // Second, bubble out, looking for items of interest that can have childen.
    // Always pick the innermost link, background image, etc.
    const XMLNS = "http://www.w3.org/XML/1998/namespace";
    var elem = this.target;
    while (elem) {
      if (elem.nodeType == Node.ELEMENT_NODE) {
        // Link?
        if (!this.onLink &&
             ((elem instanceof HTMLAnchorElement && elem.href) ||
              elem instanceof HTMLAreaElement ||
              elem instanceof HTMLLinkElement ||
              elem.getAttributeNS("http://www.w3.org/1999/xlink", "type") == "simple")) {
            
          // Target is a link or a descendant of a link.
          this.onLink = true;
          this.onMetaDataItem = true;

          // xxxmpc: this is kind of a hack to work around a Gecko bug (see bug 266932)
          // we're going to walk up the DOM looking for a parent link node,
          // this shouldn't be necessary, but we're matching the existing behaviour for left click
          var realLink = elem;
          var parent = elem.parentNode;
          while (parent) {
            try {
              if ((parent instanceof HTMLAnchorElement && elem.href) ||
                  parent instanceof HTMLAreaElement ||
                  parent instanceof HTMLLinkElement ||
                  parent.getAttributeNS("http://www.w3.org/1999/xlink", "type") == "simple")
                realLink = parent;
            } catch (e) { }
            parent = parent.parentNode;
          }
          
          // Remember corresponding element.
          this.link = realLink;
          this.linkURL = this.getLinkURL();
          this.linkURI = this.getLinkURI();
          this.linkProtocol = this.getLinkProtocol();
          this.onMailtoLink = (this.linkProtocol == "mailto");
          this.onSaveableLink = this.isLinkSaveable( this.link );
        }

        // Metadata item?
        if (!this.onMetaDataItem) {
          // We display metadata on anything which fits
          // the below test, as well as for links and images
          // (which set this.onMetaDataItem to true elsewhere)
          if ((elem instanceof HTMLQuoteElement && elem.cite) ||
              (elem instanceof HTMLTableElement && elem.summary) ||
              (elem instanceof HTMLModElement &&
               (elem.cite || elem.dateTime)) ||
              (elem instanceof HTMLElement &&
               (elem.title || elem.lang)) ||
              elem.getAttributeNS(XMLNS, "lang")) {
            this.onMetaDataItem = true;
          }
        }

        // Background image?  Don't bother if we've already found a
        // background image further down the hierarchy.  Otherwise,
        // we look for the computed background-image style.
        if (!this.hasBGImage) {
          var bgImgUrl = this.getComputedURL( elem, "background-image" );
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
    var docDefaultView = this.target.ownerDocument.defaultView;
    if (docDefaultView != docDefaultView.top)
      this.inFrame = true;

    // if the document is editable, show context menu like in text inputs
    var win = this.target.ownerDocument.defaultView;
    if (win) {
      var isEditable = false;
      try {
        var editingSession = win.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIWebNavigation)
                                .QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIEditingSession);
        isEditable = editingSession.windowIsEditable(win);
      }
      catch(ex) {
        // If someone built with composer disabled, we can't get an editing session.
      }

      if (isEditable) {
        this.onTextInput       = true;
        this.onKeywordField    = false;
        this.onImage           = false;
        this.onLoadedImage     = false;
        this.onMetaDataItem    = false;
        this.onMathML          = false;
        this.inFrame           = false;
        this.hasBGImage        = false;
        this.isDesignMode      = true;
        this.possibleSpellChecking = true;
        InlineSpellCheckerUI.init(editingSession.getEditorForWindow(win));
        var canSpell = InlineSpellCheckerUI.canSpellCheck;
        InlineSpellCheckerUI.initFromEvent(aRangeParent, aRangeOffset);
        this.showItem("spell-check-enabled", canSpell);
        this.showItem("spell-separator", canSpell);
      }
    }
  },

  // Returns the computed style attribute for the given element.
  getComputedStyle: function(aElem, aProp) {
    return elem.ownerDocument
               .defaultView
               .getComputedStyle(aElem, "").getPropertyValue(prop);
  },

  // Returns a "url"-type computed style attribute value, with the url() stripped.
  getComputedURL: function(aElem, aProp) {
    var url = aElem.ownerDocument
                   .defaultView.getComputedStyle(aElem, "")
                   .getPropertyCSSValue(aProp);
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

  // Open linked-to URL in a new window.
  openLink : function () {
    openNewWindowWith(this.linkURL, this.target.ownerDocument, null, false);
  },

  // Open linked-to URL in a new tab.
  openLinkInTab: function() {
    openNewTabWith(this.linkURL, this.target.ownerDocument, null, null, false);
  },

  // Open frame in a new tab.
  openFrameInTab: function() {
    openNewTabWith(this.target.ownerDocument.location.href,
                   null, null, null, false);
  },

  // Reload clicked-in frame.
  reloadFrame: function() {
    this.target.ownerDocument.location.reload();
  },

  // Open clicked-in frame in its own window.
  openFrame: function() {
    openNewWindowWith(this.target.ownerDocument.location.href,
                      null, null, false);
  },

  // Open clicked-in frame in the same window.
  showOnlyThisFrame: function() {
    var frameURL = this.target.ownerDocument.location.href;

    try {
      urlSecurityCheck(frameURL, this.browser.contentPrincipal,
                       Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT);
      this.browser.loadURI(frameURL);
    } catch(e) {}
  },

  // View Partial Source
  viewPartialSource: function(aContext) {
    var focusedWindow = document.commandDispatcher.focusedWindow;
    if (focusedWindow == window)
      focusedWindow = content;

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

  viewFrameInfo: function() {
    BrowserPageInfo(this.target.ownerDocument);
  },

  // Change current window to the URL of the image.
  viewImage: function(e) {
    urlSecurityCheck(this.imageURL,
                     this.browser.contentPrincipal,
                     Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT);
    var doc = this.target.ownerDocument;
    openUILink( this.imageURL, e, null, null, null, null, doc.documentURIObject );
  },

  // Change current window to the URL of the background image.
  viewBGImage: function(e) {
    urlSecurityCheck(this.bgImageURL,
                     this.browser.contentPrincipal,
                     Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT);
    var doc = this.target.ownerDocument;
    openUILink(this.bgImageURL, e, null, null, null, null, doc.documentURIObject );
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

    urlSecurityCheck(this.target.currentURI.spec,
                     this.target.ownerDocument.nodePrincipal);

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

  // Save URL of clicked-on link.
  saveLink: function() {
    var doc =  this.target.ownerDocument;
    urlSecurityCheck(this.linkURL, doc.nodePrincipal);
    saveURL(this.linkURL, this.linkText(), null, true, false,
            doc.documentURIObject);
  },

  sendLink: function() {
    // we don't know the title of the link so pass in an empty string
    MailIntegration.sendMessage( this.linkURL, "" );
  },

  // Save URL of clicked-on image.
  saveImage: function() {
    var doc =  this.target.ownerDocument;
    urlSecurityCheck(this.imageURL, doc.nodePrincipal);
    saveImageURL(this.imageURL, null, "SaveImageTitle", false,
                 false, doc.documentURIObject);
  },

  sendImage: function() {
    MailIntegration.sendMessage(this.imageURL, "");
  },

  toggleImageBlocking: function(aBlock) {
    var permissionmanager = Cc["@mozilla.org/permissionmanager;1"].
                            getService(Ci.nsIPermissionManager);

    var uri = this.target.QueryInterface(Ci.nsIImageLoadingContent).currentURI;

    permissionmanager.add(uri, "image",
                          aBlock ? Ci.nsIPermissionManager.DENY_ACTION :
                                   Ci.nsIPermissionManager.ALLOW_ACTION);

    var brandBundle = document.getElementById("bundle_brand");
    var app = brandBundle.getString("brandShortName");
    var bundle_browser = document.getElementById("bundle_browser");
    var message = bundle_browser.getFormattedString(aBlock ?
     "imageBlockedWarning" : "imageAllowedWarning", [app, uri.host]);

    var notificationBox = this.browser.getNotificationBox();
    var notification = notificationBox.getNotificationWithValue("images-blocked");

    if (notification)
      notification.label = message;
    else {
      var self = this;
      var buttons = [{
        label: bundle_browser.getString("undo"),
        accessKey: bundle_browser.getString("undo.accessKey"),
        callback: function() { self.toggleImageBlocking(!aBlock); }
      }];
      const priority = notificationBox.PRIORITY_WARNING_MEDIUM;
      notificationBox.appendNotification(message, "images-blocked",
                                         "chrome://browser/skin/Info.png",
                                         priority, buttons);
    }

    // Reload the page to show the effect instantly
    BrowserReload();
  },

  isImageBlocked: function() {
    var permissionmanager = Cc["@mozilla.org/permissionmanager;1"].
                            getService(Ci.nsIPermissionManager);

    var uri = this.target.QueryInterface(Ci.nsIImageLoadingContent).currentURI;

    return permissionmanager.testPermission(uri, "image") == Ci.nsIPermissionManager.DENY_ACTION;
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
      var characterSet = this.target.ownerDocument.characterSet;
      const textToSubURI = Cc["@mozilla.org/intl/texttosuburi;1"].
                           getService(Ci.nsITextToSubURI);
      addresses = textToSubURI.unEscapeURIForUI(characterSet, addresses);
    }
    catch(ex) {
      // Do nothing.
    }

    var clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].
                    getService(Ci.nsIClipboardHelper);
    clipboard.copyString(addresses);
  },

  // Open Metadata window for node
  showMetadata: function () {
    window.openDialog("chrome://browser/content/metaData.xul",
                      "_blank",
                      "scrollbars,resizable,chrome,dialog=no",
                      this.target);
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
    var ioService = Cc["@mozilla.org/network/io-service;1"].
                    getService(Ci.nsIIOService);
    try {
      return ioService.newURI(this.linkURL, null, null);
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
  linkText: function() {
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

  // Get selected text. Only display the first 15 chars.
  isTextSelection: function() {
    // Get 16 characters, so that we can trim the selection if it's greater
    // than 15 chars
    var selectedText = getBrowserSelection(16);

    if (!selectedText)
      return false;

    if (selectedText.length > 15)
      selectedText = selectedText.substr(0,15) + "...";

    // Use the current engine if the search bar is visible, the default
    // engine otherwise.
    var engineName = "";
    var ss = Cc["@mozilla.org/browser/search-service;1"].
             getService(Ci.nsIBrowserSearchService);
    if (BrowserSearch.getSearchBar())
      engineName = ss.currentEngine.name;
    else
      engineName = ss.defaultEngine.name;

    // format "Search <engine> for <selection>" string to show in menu
    var menuLabel = gNavigatorBundle.getFormattedString("contextMenuSearchText",
                                                        [engineName,
                                                         selectedText]);
    document.getElementById("context-searchselect").label = menuLabel;

    return true;
  },

  // Returns true if anything is selected.
  isContentSelection: function() {
    return !document.commandDispatcher.focusedWindow.getSelection().isCollapsed;
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
      return (node.type == "text" || node.type == "password")

    return (node instanceof HTMLTextAreaElement);
  },

  isTargetAKeywordField: function(aNode) {
    var form = aNode.form;
    if (!form)
      return false;

    var method = form.method.toUpperCase();

    // These are the following types of forms we can create keywords for:
    //
    // method   encoding type       can create keyword
    // GET      *                                 YES
    //          *                                 YES
    // POST                                       YES
    // POST     application/x-www-form-urlencoded YES
    // POST     text/plain                        NO (a little tricky to do)
    // POST     multipart/form-data               NO
    // POST     everything else                   YES
    return (method == "GET" || method == "") ||
           (form.enctype != "text/plain") && (form.enctype != "multipart/form-data");
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

#ifdef MOZ_PLACES_BOOKMARKS
  bookmarkThisPage: function CM_bookmarkThisPage() {
    PlacesCommandHook.bookmarkPage(this.browser);
  },

  bookmarkLink: function CM_bookmarkLink() {
    PlacesCommandHook.bookmarkLink(this.linkURL, this.linkText());
  },

  addBookmarkForFrame: function CM_addBookmarkForFrame() {
    var uri = this.target.ownerDocument.documentURIObject;
    PlacesUtils.showAddBookmarkUI(uri);
  },
#else
  bookmarkThisPage: function CM_bookmarkThisPage() {
    addBookmarkAs(this.browser);
  },

  bookmarkLink: function CM_bookmarkLink() {
    BookmarksUtils.addBookmark(this.linkURL, this.linkText());
  },

  addBookmarkForFrame: function CM_addBookmarkForFrame() {
    var doc = this.target.ownerDocument;
    var uri = doc.location.href;
    var title = doc.title;
    var description = BookmarksUtils.getDescriptionFromDocument(doc);
    if (!title)
      title = uri;
    BookmarksUtils.addBookmark(uri, title, doc.charset, description);
  },
#endif

  savePageAs: function() {
    saveDocument(this.browser.contentDocument);
  },

  printFrame: function() {
    PrintUtils.print(this.target.ownerDocument.defaultView);
  }
};
