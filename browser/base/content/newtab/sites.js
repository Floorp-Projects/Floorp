#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

const THUMBNAIL_PLACEHOLDER_ENABLED =
  Services.prefs.getBoolPref("browser.newtabpage.thumbnailPlaceholder");

/**
 * This class represents a site that is contained in a cell and can be pinned,
 * moved around or deleted.
 */
function Site(aNode, aLink) {
  this._node = aNode;
  this._node._newtabSite = this;

  this._link = aLink;

  this._render();
  this._addEventHandlers();
}

Site.prototype = {
  /**
   * The site's DOM node.
   */
  get node() { return this._node; },

  /**
   * The site's link.
   */
  get link() { return this._link; },

  /**
   * The url of the site's link.
   */
  get url() { return this.link.url; },

  /**
   * The title of the site's link.
   */
  get title() { return this.link.title || this.link.url; },

  /**
   * The site's parent cell.
   */
  get cell() {
    let parentNode = this.node.parentNode;
    return parentNode && parentNode._newtabCell;
  },

  /**
   * Pins the site on its current or a given index.
   * @param aIndex The pinned index (optional).
   */
  pin: function Site_pin(aIndex) {
    if (typeof aIndex == "undefined")
      aIndex = this.cell.index;

    this._updateAttributes(true);
    gPinnedLinks.pin(this._link, aIndex);
  },

  /**
   * Unpins the site and calls the given callback when done.
   */
  unpin: function Site_unpin() {
    if (this.isPinned()) {
      this._updateAttributes(false);
      gPinnedLinks.unpin(this._link);
      gUpdater.updateGrid();
    }
  },

  /**
   * Checks whether this site is pinned.
   * @return Whether this site is pinned.
   */
  isPinned: function Site_isPinned() {
    return gPinnedLinks.isPinned(this._link);
  },

  /**
   * Blocks the site (removes it from the grid) and calls the given callback
   * when done.
   */
  block: function Site_block() {
    if (!gBlockedLinks.isBlocked(this._link)) {
      gUndoDialog.show(this);
      gBlockedLinks.block(this._link);
      gUpdater.updateGrid();
    }
  },

  /**
   * Gets the DOM node specified by the given query selector.
   * @param aSelector The query selector.
   * @return The DOM node we found.
   */
  _querySelector: function Site_querySelector(aSelector) {
    return this.node.querySelector(aSelector);
  },

  /**
   * Updates attributes for all nodes which status depends on this site being
   * pinned or unpinned.
   * @param aPinned Whether this site is now pinned or unpinned.
   */
  _updateAttributes: function (aPinned) {
    let control = this._querySelector(".newtab-control-pin");

    if (aPinned) {
      this.node.setAttribute("pinned", true);
      control.setAttribute("title", newTabString("unpin"));
    } else {
      this.node.removeAttribute("pinned");
      control.setAttribute("title", newTabString("pin"));
    }
  },

  _newTabString: function(str, substrArr) {
    let regExp = /%[0-9]\$S/g;
    let matches;
    while ((matches = regExp.exec(str))) {
      let match = matches[0];
      let index = match.charAt(1); // Get the digit in the regExp.
      str = str.replace(match, substrArr[index - 1]);
    }
    return str;
  },

  /**
   * Renders the site's data (fills the HTML fragment).
   */
  _render: function Site_render() {
    // setup display variables
    let enhanced = gAllPages.enhanced && DirectoryLinksProvider.getEnhancedLink(this.link);
    let url = this.url;
    let title = enhanced && enhanced.title ? enhanced.title :
                this.link.type == "history" ? this.link.baseDomain :
                this.title;
    let tooltip = (this.title == url ? this.title : this.title + "\n" + url);

    let link = this._querySelector(".newtab-link");
    link.setAttribute("title", tooltip);
    link.setAttribute("href", url);
    this.node.setAttribute("type", this.link.type);

    let titleNode = this._querySelector(".newtab-title");
    titleNode.textContent = title;
    if (this.link.titleBgColor) {
      titleNode.style.backgroundColor = this.link.titleBgColor;
    }

    if (this.isPinned())
      this._updateAttributes(true);
    // Capture the page if the thumbnail is missing, which will cause page.js
    // to be notified and call our refreshThumbnail() method.
    this.captureIfMissing();
    // but still display whatever thumbnail might be available now.
    this.refreshThumbnail();
  },

  /**
   * Captures the site's thumbnail in the background, but only if there's no
   * existing thumbnail and the page allows background captures.
   */
  captureIfMissing: function Site_captureIfMissing() {
    if (!document.hidden && !this.link.imageURI) {
      const {unloadingPromise} = gPage;
      BackgroundPageThumbs.captureIfMissing(this.url, {unloadingPromise});
    }
  },

  /**
   * Refreshes the thumbnail for the site.
   */
  refreshThumbnail: function Site_refreshThumbnail() {
    // Only enhance tiles if that feature is turned on
    let link = gAllPages.enhanced && DirectoryLinksProvider.getEnhancedLink(this.link) ||
               this.link;

    let thumbnail = this._querySelector(".newtab-thumbnail.thumbnail");
    if (link.bgColor) {
      thumbnail.style.backgroundColor = link.bgColor;
    }
    let uri = link.imageURI || PageThumbs.getThumbnailURL(this.url);
    thumbnail.src = uri;

    if (THUMBNAIL_PLACEHOLDER_ENABLED &&
        link.type == "history" &&
        link.baseDomain) {
      let placeholder = this._querySelector(".newtab-thumbnail.placeholder");
      let charCodeSum = 0;
      for (let c of link.baseDomain) {
        charCodeSum += c.charCodeAt(0);
      }
      const COLORS = 16;
      let hue = Math.round((charCodeSum % COLORS) / COLORS * 360);
      placeholder.style.backgroundColor = "hsl(" + hue + ",80%,40%)";
      placeholder.textContent = link.baseDomain.substr(0,1).toUpperCase();
    }

    if (link.enhancedImageURI) {
      let enhanced = this._querySelector(".enhanced-content");
      enhanced.src = link.enhancedImageURI;
    }
  },

  /**
   * Adds event handlers for the site and its buttons.
   */
  _addEventHandlers: function Site_addEventHandlers() {
    // Register drag-and-drop event handlers.
    this._node.addEventListener("dragstart", this);
    this._node.addEventListener("dragend", this);
    this._node.addEventListener("mouseover", this);
  },

  /**
   * Speculatively opens a connection to the current site.
   */
  _speculativeConnect: function Site_speculativeConnect() {
    let sc = Services.io.QueryInterface(Ci.nsISpeculativeConnect);
    let uri = Services.io.newURI(this.url);

    if (!uri.schemeIs("http") && !uri.schemeIs("https")) {
      return;
    }

    try {
      // This can throw for certain internal URLs, when they wind up in
      // about:newtab. Be sure not to propagate the error.

      // We use the URI's codebase principal here to open its speculative
      // connection.
      let originAttributes = document.docShell.getOriginAttributes();
      let principal = Services.scriptSecurityManager
                              .createCodebasePrincipal(uri, originAttributes);
      sc.speculativeConnect2(uri, principal, null);
    } catch (e) {}
  },

  /**
   * Handles site click events.
   */
  onClick: function Site_onClick(aEvent) {
    let pinned = this.isPinned();
    let {button, target} = aEvent;
    const isLinkClick = target.classList.contains("newtab-link") ||
      target.parentElement.classList.contains("newtab-link");

    // Handle primary click for pin and block
    if (button == 0 && !isLinkClick) {
      aEvent.preventDefault();
      if (target.classList.contains("newtab-control-block")) {
        this.block();
      }
      else if (pinned && target.classList.contains("newtab-control-pin")) {
        this.unpin();
      }
      else if (!pinned && target.classList.contains("newtab-control-pin")) {
        this.pin();
      }
    }
  },

  /**
   * Handles all site events.
   */
  handleEvent: function Site_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "mouseover":
        this._node.removeEventListener("mouseover", this);
        this._speculativeConnect();
        break;
      case "dragstart":
        gDrag.start(this, aEvent);
        break;
      case "dragend":
        gDrag.end(this, aEvent);
        break;
    }
  }
};
