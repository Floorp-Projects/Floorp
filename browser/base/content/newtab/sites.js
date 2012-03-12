#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

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
  get node() this._node,

  /**
   * The site's link.
   */
  get link() this._link,

  /**
   * The url of the site's link.
   */
  get url() this.link.url,

  /**
   * The title of the site's link.
   */
  get title() this.link.title,

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
   * @param aCallback The callback to be called when finished.
   */
  unpin: function Site_unpin(aCallback) {
    if (this.isPinned()) {
      this._updateAttributes(false);
      gPinnedLinks.unpin(this._link);
      gUpdater.updateGrid(aCallback);
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
   * @param aCallback The function to be called when finished.
   */
  block: function Site_block(aCallback) {
    if (gBlockedLinks.isBlocked(this._link)) {
      if (aCallback)
        aCallback();
    } else {
      gBlockedLinks.block(this._link);
      gUpdater.updateGrid(aCallback);
      gPage.updateModifiedFlag();
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
    let buttonPin = this._querySelector(".strip-button-pin");

    if (aPinned) {
      this.node.setAttribute("pinned", true);
      buttonPin.setAttribute("title", newTabString("unpin"));
    } else {
      this.node.removeAttribute("pinned");
      buttonPin.setAttribute("title", newTabString("pin"));
    }
  },

  /**
   * Renders the site's data (fills the HTML fragment).
   */
  _render: function Site_render() {
    let title = this.title || this.url;
    this.node.setAttribute("title", title);
    this.node.setAttribute("href", this.url);
    this._querySelector(".site-title").textContent = title;

    if (this.isPinned())
      this._updateAttributes(true);

    this._renderThumbnail();
  },

  /**
   * Renders the site's thumbnail.
   */
  _renderThumbnail: function Site_renderThumbnail() {
    let img = this._querySelector(".site-img")
    img.setAttribute("alt", this.title || this.url);
    img.setAttribute("loading", "true");

    // Wait until the image has loaded.
    img.addEventListener("load", function onLoad() {
      img.removeEventListener("load", onLoad, false);
      img.removeAttribute("loading");
    }, false);

    // Set the thumbnail url.
    img.setAttribute("src", PageThumbs.getThumbnailURL(this.url));
  },

  /**
   * Adds event handlers for the site and its buttons.
   */
  _addEventHandlers: function Site_addEventHandlers() {
    // Register drag-and-drop event handlers.
    ["DragStart", /*"Drag",*/ "DragEnd"].forEach(function (aType) {
      let method = "_on" + aType;
      this[method] = this[method].bind(this);
      this._node.addEventListener(aType.toLowerCase(), this[method], false);
    }, this);

    let self = this;

    function pin(aEvent) {
      if (aEvent)
        aEvent.preventDefault();

      if (self.isPinned())
        self.unpin();
      else
        self.pin();
    }

    function block(aEvent) {
      if (aEvent)
        aEvent.preventDefault();

      self.block();
    }

    this._querySelector(".strip-button-pin").addEventListener("click", pin, false);
    this._querySelector(".strip-button-block").addEventListener("click", block, false);
  },

  /**
   * Event handler for the 'dragstart' event.
   * @param aEvent The drag event.
   */
  _onDragStart: function Site_onDragStart(aEvent) {
    gDrag.start(this, aEvent);
  },

  /**
   * Event handler for the 'drag' event.
   * @param aEvent The drag event.
  */
  _onDrag: function Site_onDrag(aEvent) {
    gDrag.drag(this, aEvent);
  },

  /**
   * Event handler for the 'dragend' event.
   * @param aEvent The drag event.
   */
  _onDragEnd: function Site_onDragEnd(aEvent) {
    gDrag.end(this, aEvent);
  }
};
