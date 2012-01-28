#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

/**
 * This singleton represents the whole 'New Tab Page' and takes care of
 * initializing all its components.
 */
let gPage = {
  /**
   * Initializes the page.
   * @param aToolbarSelector The query selector for the page toolbar.
   * @param aGridSelector The query selector for the grid.
   */
  init: function Page_init(aToolbarSelector, aGridSelector) {
    gToolbar.init(aToolbarSelector);
    this._gridSelector = aGridSelector;

    // Add ourselves to the list of pages to receive notifications.
    gAllPages.register(this);

    // Listen for 'unload' to unregister this page.
    function unload() gAllPages.unregister(self);
    addEventListener("unload", unload, false);

    // Check if the new tab feature is enabled.
    if (gAllPages.enabled)
      this._init();
    else
      this._updateAttributes(false);
  },

  /**
   * Listens for notifications specific to this page.
   */
  observe: function Page_observe() {
    let enabled = gAllPages.enabled;
    this._updateAttributes(enabled);

    // Initialize the whole page if we haven't done that, yet.
    if (enabled)
      this._init();
  },

  /**
   * Updates the whole page and the grid when the storage has changed.
   */
  update: function Page_update() {
    this.updateModifiedFlag();
    gGrid.refresh();
  },

  /**
   * Checks if the page is modified and sets the CSS class accordingly
   */
  updateModifiedFlag: function Page_updateModifiedFlag() {
    let node = document.getElementById("toolbar-button-reset");
    let modified = this._isModified();

    if (modified)
      node.setAttribute("modified", "true");
    else
      node.removeAttribute("modified");

    this._updateTabIndices(gAllPages.enabled, modified);
  },

  /**
   * Internally initializes the page. This runs only when/if the feature
   * is/gets enabled.
   */
  _init: function Page_init() {
    if (this._initialized)
      return;

    this._initialized = true;

    gLinks.populateCache(function () {
      // Check if the grid is modified.
      this.updateModifiedFlag();

      // Initialize and render the grid.
      gGrid.init(this._gridSelector);

      // Initialize the drop target shim.
      gDropTargetShim.init();

      // Workaround to prevent a delay on MacOSX due to a slow drop animation.
      let doc = document.documentElement;
      doc.addEventListener("dragover", this.onDragOver, false);
      doc.addEventListener("drop", this.onDrop, false);
    }.bind(this));
  },

  /**
   * Updates the 'page-disabled' attributes of the respective DOM nodes.
   * @param aValue Whether to set or remove attributes.
   */
  _updateAttributes: function Page_updateAttributes(aValue) {
    let nodes = document.querySelectorAll("#grid, #scrollbox, #toolbar, .toolbar-button");

    // Set the nodes' states.
    for (let i = 0; i < nodes.length; i++) {
      let node = nodes[i];
      if (aValue)
        node.removeAttribute("page-disabled");
      else
        node.setAttribute("page-disabled", "true");
    }

    this._updateTabIndices(aValue, this._isModified());
  },

  /**
   * Checks whether the page is modified.
   * @return Whether the page is modified or not.
   */
  _isModified: function Page_isModified() {
    // The page is considered modified only if sites have been removed.
    return !gBlockedLinks.isEmpty();
  },

  /**
   * Updates the tab indices of focusable elements.
   * @param aEnabled Whether the page is currently enabled.
   * @param aModified Whether the page is currently modified.
   */
  _updateTabIndices: function Page_updateTabIndices(aEnabled, aModified) {
    function setFocusable(aNode, aFocusable) {
      if (aFocusable)
        aNode.removeAttribute("tabindex");
      else
        aNode.setAttribute("tabindex", "-1");
    }

    // Sites and the 'hide' button are always focusable when the grid is shown.
    let nodes = document.querySelectorAll(".site, #toolbar-button-hide");
    for (let i = 0; i < nodes.length; i++)
      setFocusable(nodes[i], aEnabled);

    // The 'show' button is focusable when the grid is hidden.
    let btnShow = document.getElementById("toolbar-button-show");
    setFocusable(btnShow, !aEnabled);

    // The 'reset' button is focusable when the grid is shown and modified.
    let btnReset = document.getElementById("toolbar-button-reset");
    setFocusable(btnReset, aEnabled && aModified);
  },

  /**
   * Handles the 'dragover' event. Workaround to prevent a delay on MacOSX
   * due to a slow drop animation.
   * @param aEvent The 'dragover' event.
   */
  onDragOver: function Page_onDragOver(aEvent) {
    if (gDrag.isValid(aEvent))
      aEvent.preventDefault();
  },

  /**
   * Handles the 'drop' event. Workaround to prevent a delay on MacOSX due to
   * a slow drop animation.
   * @param aEvent The 'drop' event.
   */
  onDrop: function Page_onDrop(aEvent) {
    if (gDrag.isValid(aEvent)) {
      aEvent.preventDefault();
      aEvent.stopPropagation();
    }
  }
};
