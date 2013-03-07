/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

this.EXPORTED_SYMBOLS = ["InspectorPanel"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");
Cu.import("resource:///modules/devtools/EventEmitter.jsm");
Cu.import("resource:///modules/devtools/CssLogic.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "MarkupView",
  "resource:///modules/devtools/MarkupView.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Selection",
  "resource:///modules/devtools/Selection.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "HTMLBreadcrumbs",
  "resource:///modules/devtools/Breadcrumbs.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Highlighter",
  "resource:///modules/devtools/Highlighter.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ToolSidebar",
  "resource:///modules/devtools/Sidebar.jsm");

const LAYOUT_CHANGE_TIMER = 250;

/**
 * Represents an open instance of the Inspector for a tab.
 * The inspector controls the highlighter, the breadcrumbs,
 * the markup view, and the sidebar (computed view, rule view
 * and layout view).
 */
this.InspectorPanel = function InspectorPanel(iframeWindow, toolbox) {
  this._toolbox = toolbox;
  this._target = toolbox._target;
  this.panelDoc = iframeWindow.document;
  this.panelWin = iframeWindow;
  this.panelWin.inspector = this;

  EventEmitter.decorate(this);
}

InspectorPanel.prototype = {
  /**
   * open is effectively an asynchronous constructor
   */
  open: function InspectorPanel_open() {
    let deferred = Promise.defer();

    this.preventNavigateAway = this.preventNavigateAway.bind(this);
    this.onNavigatedAway = this.onNavigatedAway.bind(this);
    this.target.on("will-navigate", this.preventNavigateAway);
    this.target.on("navigate", this.onNavigatedAway);

    this.nodemenu = this.panelDoc.getElementById("inspector-node-popup");
    this.lastNodemenuItem = this.nodemenu.lastChild;
    this._setupNodeMenu = this._setupNodeMenu.bind(this);
    this._resetNodeMenu = this._resetNodeMenu.bind(this);
    this.nodemenu.addEventListener("popupshowing", this._setupNodeMenu, true);
    this.nodemenu.addEventListener("popuphiding", this._resetNodeMenu, true);

    // Initialize the search related items
    this.searchBox = this.panelDoc.getElementById("inspector-searchbox");
    this._lastSearched = null;
    this._searchResults = null;
    this._searchIndex = 0;
    this._onHTMLSearch = this._onHTMLSearch.bind(this);
    this._onSearchKeypress = this._onSearchKeypress.bind(this);
    this.searchBox.addEventListener("command", this._onHTMLSearch, true);
    this.searchBox.addEventListener("keypress", this._onSearchKeypress, true);

    // Create an empty selection
    this._selection = new Selection();
    this.onNewSelection = this.onNewSelection.bind(this);
    this.selection.on("new-node", this.onNewSelection);
    this.onBeforeNewSelection = this.onBeforeNewSelection.bind(this);
    this.selection.on("before-new-node", this.onBeforeNewSelection);
    this.onDetached = this.onDetached.bind(this);
    this.selection.on("detached", this.onDetached);

    this.breadcrumbs = new HTMLBreadcrumbs(this);

    if (this.target.isLocalTab) {
      this.browser = this.target.tab.linkedBrowser;
      this.scheduleLayoutChange = this.scheduleLayoutChange.bind(this);
      this.browser.addEventListener("resize", this.scheduleLayoutChange, true);

      this.highlighter = new Highlighter(this.target, this, this._toolbox);
      let button = this.panelDoc.getElementById("inspector-inspect-toolbutton");
      button.hidden = false;
      this.onLockStateChanged = function() {
        if (this.highlighter.locked) {
          button.removeAttribute("checked");
          this._toolbox.raise();
        } else {
          button.setAttribute("checked", "true");
        }
      }.bind(this);
      this.highlighter.on("locked", this.onLockStateChanged);
      this.highlighter.on("unlocked", this.onLockStateChanged);

      // Show a warning when the debugger is paused.
      // We show the warning only when the inspector
      // is selected.
      this.updateDebuggerPausedWarning = function() {
        let notificationBox = this._toolbox.getNotificationBox();
        let notification = notificationBox.getNotificationWithValue("inspector-script-paused");
        if (!notification && this._toolbox.currentToolId == "inspector" &&
            this.target.isThreadPaused) {
          let message = this.strings.GetStringFromName("debuggerPausedWarning.message");
          notificationBox.appendNotification(message,
            "inspector-script-paused", "", notificationBox.PRIORITY_WARNING_HIGH);
        }

        if (notification && this._toolbox.currentToolId != "inspector") {
          notificationBox.removeNotification(notification);
        }

        if (notification && !this.target.isThreadPaused) {
          notificationBox.removeNotification(notification);
        }

      }.bind(this);
      this.target.on("thread-paused", this.updateDebuggerPausedWarning);
      this.target.on("thread-resumed", this.updateDebuggerPausedWarning);
      this._toolbox.on("select", this.updateDebuggerPausedWarning);
      this.updateDebuggerPausedWarning();
    }

    this._initMarkup();
    this.isReady = false;

    this.once("markuploaded", function() {
      this.isReady = true;

      // All the components are initialized. Let's select a node.
      if (this.target.isLocalTab) {
        let root = this.browser.contentDocument.documentElement;
        this._selection.setNode(root);
      } else if (this.target.window) {
        let root = this.target.window.document.documentElement;
        this._selection.setNode(root);
      }

      if (this.highlighter) {
        this.highlighter.unlock();
      }

      this.emit("ready");
      deferred.resolve(this);
    }.bind(this));

    this.setupSidebar();

    return deferred.promise;
  },

  /**
   * Selection object (read only)
   */
  get selection() {
    return this._selection;
  },

  /**
   * Target getter.
   */
  get target() {
    return this._target;
  },

  /**
   * Target setter.
   */
  set target(value) {
    this._target = value;
  },

  /**
   * Expose gViewSourceUtils so that other tools can make use of them.
   */
  get viewSourceUtils() {
    return this.panelWin.gViewSourceUtils;
  },

  /**
   * Indicate that a tool has modified the state of the page.  Used to
   * decide whether to show the "are you sure you want to navigate"
   * notification.
   */
  markDirty: function InspectorPanel_markDirty() {
    this.isDirty = true;
  },

  /**
   * Build the sidebar.
   */
  setupSidebar: function InspectorPanel_setupSidebar() {
    let tabbox = this.panelDoc.querySelector("#inspector-sidebar");
    this.sidebar = new ToolSidebar(tabbox, this);

    let defaultTab = Services.prefs.getCharPref("devtools.inspector.activeSidebar");

    this._setDefaultSidebar = function(event, toolId) {
      Services.prefs.setCharPref("devtools.inspector.activeSidebar", toolId);
    }.bind(this);

    this.sidebar.on("select", this._setDefaultSidebar);
    this.toggleHighlighter = this.toggleHighlighter.bind(this);

    this.sidebar.addTab("ruleview",
                        "chrome://browser/content/devtools/cssruleview.xul",
                        "ruleview" == defaultTab);

    this.sidebar.addTab("computedview",
                        "chrome://browser/content/devtools/csshtmltree.xul",
                        "computedview" == defaultTab);

    this.sidebar.addTab("layoutview",
                        "chrome://browser/content/devtools/layoutview/view.xhtml",
                        "layoutview" == defaultTab);

    let ruleViewTab = this.sidebar.getTab("ruleview");
    ruleViewTab.addEventListener("mouseover", this.toggleHighlighter, false);
    ruleViewTab.addEventListener("mouseout", this.toggleHighlighter, false);

    this.sidebar.show();
  },

  /**
   * Reset the inspector on navigate away.
   */
  onNavigatedAway: function InspectorPanel_onNavigatedAway(event, newWindow) {
    this.selection.setNode(null);
    this._destroyMarkup();
    this.isDirty = false;
    let self = this;

    function onDOMReady() {
      newWindow.removeEventListener("DOMContentLoaded", onDOMReady, true);

      if (self._destroyed) {
        return;
      }

      if (!self.selection.node) {
        self.selection.setNode(newWindow.document.documentElement);
      }
      self._initMarkup();
    }

    if (newWindow.document.readyState == "loading") {
      newWindow.addEventListener("DOMContentLoaded", onDOMReady, true);
    } else {
      onDOMReady();
    }
  },

  /**
   * Show a message if the inspector is dirty.
   */
  preventNavigateAway: function InspectorPanel_preventNavigateAway(event, request) {
    if (!this.isDirty) {
      return;
    }

    request.suspend();

    let notificationBox = null;
    if (this.target.isLocalTab) {
      let gBrowser = this.target.tab.ownerDocument.defaultView.gBrowser;
      notificationBox = gBrowser.getNotificationBox();
    }
    else {
      notificationBox = this._toolbox.getNotificationBox();
    }

    let notification = notificationBox.
      getNotificationWithValue("inspector-page-navigation");

    if (notification) {
      notificationBox.removeNotification(notification, true);
    }

    let cancelRequest = function onCancelRequest() {
      if (request) {
        request.cancel(Cr.NS_BINDING_ABORTED);
        request.resume(); // needed to allow the connection to be cancelled.
        request = null;
      }
    };

    let eventCallback = function onNotificationCallback(event) {
      if (event == "removed") {
        cancelRequest();
      }
    };

    let buttons = [
      {
        id: "inspector.confirmNavigationAway.buttonLeave",
        label: this.strings.GetStringFromName("confirmNavigationAway.buttonLeave"),
        accessKey: this.strings.GetStringFromName("confirmNavigationAway.buttonLeaveAccesskey"),
        callback: function onButtonLeave() {
          if (request) {
            request.resume();
            request = null;
          }
        }.bind(this),
      },
      {
        id: "inspector.confirmNavigationAway.buttonStay",
        label: this.strings.GetStringFromName("confirmNavigationAway.buttonStay"),
        accessKey: this.strings.GetStringFromName("confirmNavigationAway.buttonStayAccesskey"),
        callback: cancelRequest
      },
    ];

    let message = this.strings.GetStringFromName("confirmNavigationAway.message2");

    notification = notificationBox.appendNotification(message,
      "inspector-page-navigation", "chrome://browser/skin/Info.png",
      notificationBox.PRIORITY_WARNING_HIGH, buttons, eventCallback);

    // Make sure this not a transient notification, to avoid the automatic
    // transient notification removal.
    notification.persistence = -1;
  },

  /**
   * When a new node is selected.
   */
  onNewSelection: function InspectorPanel_onNewSelection() {
    this.cancelLayoutChange();
  },

  /**
   * When a new node is selected, before the selection has changed.
   */
  onBeforeNewSelection: function InspectorPanel_onBeforeNewSelection(event,
                                                                     node) {
    if (this.breadcrumbs.indexOf(node) == -1) {
      // only clear locks if we'd have to update breadcrumbs
      this.clearPseudoClasses();
    }
  },

  /**
   * When a node is deleted, select its parent node.
   */
  onDetached: function InspectorPanel_onDetached(event, parentNode) {
    this.cancelLayoutChange();
    this.breadcrumbs.cutAfter(this.breadcrumbs.indexOf(parentNode));
    this.selection.setNode(parentNode, "detached");
  },

  /**
   * Destroy the inspector.
   */
  destroy: function InspectorPanel__destroy() {
    if (this._destroyed) {
      return Promise.resolve(null);
    }
    this._destroyed = true;

    this.cancelLayoutChange();

    if (this.browser) {
      this.browser.removeEventListener("resize", this.scheduleLayoutChange, true);
      this.browser = null;
    }

    this.target.off("will-navigate", this.preventNavigateAway);
    this.target.off("navigate", this.onNavigatedAway);

    if (this.highlighter) {
      this.highlighter.off("locked", this.onLockStateChanged);
      this.highlighter.off("unlocked", this.onLockStateChanged);
      this.highlighter.destroy();
    }

    this.target.off("thread-paused", this.updateDebuggerPausedWarning);
    this.target.off("thread-resumed", this.updateDebuggerPausedWarning);
    this._toolbox.off("select", this.updateDebuggerPausedWarning);

    this._toolbox = null;

    this.sidebar.off("select", this._setDefaultSidebar);
    this.sidebar.destroy();
    this.sidebar = null;

    this.nodemenu.removeEventListener("popupshowing", this._setupNodeMenu, true);
    this.nodemenu.removeEventListener("popuphiding", this._resetNodeMenu, true);
    this.searchBox.removeEventListener("command", this._onHTMLSearch, true);
    this.searchBox.removeEventListener("keypress", this._onSearchKeypress, true);
    this.breadcrumbs.destroy();
    this.selection.off("new-node", this.onNewSelection);
    this.selection.off("before-new-node", this.onBeforeNewSelection);
    this.selection.off("detached", this.onDetached);
    this._destroyMarkup();
    this._selection.destroy();
    this._selection = null;
    this.panelWin.inspector = null;
    this.target = null;
    this.panelDoc = null;
    this.panelWin = null;
    this.breadcrumbs = null;
    this.lastNodemenuItem = null;
    this.nodemenu = null;
    this.searchBox = null;
    this.highlighter = null;
    this._searchResults = null;

    return Promise.resolve(null);
  },

  /**
   * The command callback for the HTML search box. This function is
   * automatically invoked as the user is typing.
   */
  _onHTMLSearch: function InspectorPanel__onHTMLSearch() {
    let query = this.searchBox.value;
    if (query == this._lastSearched) {
      return;
    }
    this._lastSearched = query;
    this._searchIndex = 0;

    if (query.length == 0) {
      this.searchBox.removeAttribute("filled");
      this.searchBox.classList.remove("devtools-no-search-result");
      return;
    }

    this.searchBox.setAttribute("filled", true);
    this._searchResults = this.browser.contentDocument.querySelectorAll(query);
    if (this._searchResults.length > 0) {
      this.searchBox.classList.remove("devtools-no-search-result");
      this.cancelLayoutChange();
      this.selection.setNode(this._searchResults[0]);
    } else {
      this.searchBox.classList.add("devtools-no-search-result");
    }
  },

  /**
   * Search for the search box value as a query selector.
   */
  _onSearchKeypress: function InspectorPanel__onSearchKeypress(aEvent) {
    let query = this.searchBox.value;
    switch(aEvent.keyCode) {
      case aEvent.DOM_VK_ENTER:
      case aEvent.DOM_VK_RETURN:
        if (query == this._lastSearched) {
          this._searchIndex = (this._searchIndex + 1) % this._searchResults.length;
        } else {
          this._onHTMLSearch();
          return;
        }
        break;

      case aEvent.DOM_VK_UP:
        if (--this._searchIndex < 0) {
          this._searchIndex = this._searchResults.length - 1;
        }
        break;

      case aEvent.DOM_VK_DOWN:
        this._searchIndex = (this._searchIndex + 1) % this._searchResults.length;
        break;

      default:
        return;
    }

    aEvent.preventDefault();
    aEvent.stopPropagation();
    this.cancelLayoutChange();
    if (this._searchResults.length > 0) {
      this.selection.setNode(this._searchResults[this._searchIndex]);
    }
  },

  /**
   * Show the node menu.
   */
  showNodeMenu: function InspectorPanel_showNodeMenu(aButton, aPosition, aExtraItems) {
    if (aExtraItems) {
      for (let item of aExtraItems) {
        this.nodemenu.appendChild(item);
      }
    }
    this.nodemenu.openPopup(aButton, aPosition, 0, 0, true, false);
  },

  hideNodeMenu: function InspectorPanel_hideNodeMenu() {
    this.nodemenu.hidePopup();
  },

  /**
   * Disable the delete item if needed. Update the pseudo classes.
   */
  _setupNodeMenu: function InspectorPanel_setupNodeMenu() {
    // Set the pseudo classes
    for (let name of ["hover", "active", "focus"]) {
      let menu = this.panelDoc.getElementById("node-menu-pseudo-" + name);

      if (this.selection.isElementNode()) {
        let checked = DOMUtils.hasPseudoClassLock(this.selection.node, ":" + name);
        menu.setAttribute("checked", checked);
        menu.removeAttribute("disabled");
      } else {
        menu.setAttribute("disabled", "true");
      }
    }

    // Disable delete item if needed
    let deleteNode = this.panelDoc.getElementById("node-menu-delete");
    if (this.selection.isRoot() || this.selection.isDocumentTypeNode()) {
      deleteNode.setAttribute("disabled", "true");
    } else {
      deleteNode.removeAttribute("disabled");
    }

    // Disable / enable "Copy Unique Selector", "Copy inner HTML" &
    // "Copy outer HTML" as appropriate
    let unique = this.panelDoc.getElementById("node-menu-copyuniqueselector");
    let copyInnerHTML = this.panelDoc.getElementById("node-menu-copyinner");
    let copyOuterHTML = this.panelDoc.getElementById("node-menu-copyouter");
    if (this.selection.isElementNode()) {
      unique.removeAttribute("disabled");
      copyInnerHTML.removeAttribute("disabled");
      copyOuterHTML.removeAttribute("disabled");
    } else {
      unique.setAttribute("disabled", "true");
      copyInnerHTML.setAttribute("disabled", "true");
      copyOuterHTML.setAttribute("disabled", "true");
    }
  },

  _resetNodeMenu: function InspectorPanel_resetNodeMenu() {
    // Remove any extra items
    while (this.lastNodemenuItem.nextSibling) {
      let toDelete = this.lastNodemenuItem.nextSibling;
      toDelete.parentNode.removeChild(toDelete);
    }
  },

  _initMarkup: function InspectorPanel_initMarkup() {
    let doc = this.panelDoc;

    this._markupBox = doc.getElementById("markup-box");

    // create tool iframe
    this._markupFrame = doc.createElement("iframe");
    this._markupFrame.setAttribute("flex", "1");
    this._markupFrame.setAttribute("tooltip", "aHTMLTooltip");
    this._markupFrame.setAttribute("context", "inspector-node-popup");

    // This is needed to enable tooltips inside the iframe document.
    this._boundMarkupFrameLoad = function InspectorPanel_initMarkupPanel_onload() {
      this._markupFrame.contentWindow.focus();
      this._onMarkupFrameLoad();
    }.bind(this);
    this._markupFrame.addEventListener("load", this._boundMarkupFrameLoad, true);

    this._markupBox.setAttribute("hidden", true);
    this._markupBox.appendChild(this._markupFrame);
    this._markupFrame.setAttribute("src", "chrome://browser/content/devtools/markup-view.xhtml");
  },

  _onMarkupFrameLoad: function InspectorPanel__onMarkupFrameLoad() {
    this._markupFrame.removeEventListener("load", this._boundMarkupFrameLoad, true);
    delete this._boundMarkupFrameLoad;

    this._markupBox.removeAttribute("hidden");

    let controllerWindow = this._toolbox.doc.defaultView;
    this.markup = new MarkupView(this, this._markupFrame, controllerWindow);

    this.emit("markuploaded");
  },

  _destroyMarkup: function InspectorPanel__destroyMarkup() {
    if (this._boundMarkupFrameLoad) {
      this._markupFrame.removeEventListener("load", this._boundMarkupFrameLoad, true);
      delete this._boundMarkupFrameLoad;
    }

    if (this.markup) {
      this.markup.destroy();
      delete this.markup;
    }

    if (this._markupFrame) {
      this._markupFrame.parentNode.removeChild(this._markupFrame);
      delete this._markupFrame;
    }
  },

  /**
   * Toggle a pseudo class.
   */
  togglePseudoClass: function InspectorPanel_togglePseudoClass(aPseudo) {
    if (this.selection.isElementNode()) {
      if (DOMUtils.hasPseudoClassLock(this.selection.node, aPseudo)) {
        this.breadcrumbs.nodeHierarchy.forEach(function(crumb) {
          DOMUtils.removePseudoClassLock(crumb.node, aPseudo);
        });
      } else {
        let hierarchical = aPseudo == ":hover" || aPseudo == ":active";
        let node = this.selection.node;
        do {
          DOMUtils.addPseudoClassLock(node, aPseudo);
          node = node.parentNode;
        } while (hierarchical && node.parentNode)
      }
    }
    this.selection.emit("pseudoclass");
  },

  /**
   * Clear any pseudo-class locks applied to the current hierarchy.
   */
  clearPseudoClasses: function InspectorPanel_clearPseudoClasses() {
    this.breadcrumbs.nodeHierarchy.forEach(function(crumb) {
      DOMUtils.clearPseudoClassLocks(crumb.node);
    });
  },

  /**
   * Toggle the highlighter when ruleview is hovered.
   */
  toggleHighlighter: function InspectorPanel_toggleHighlighter(event)
  {
    if (event.type == "mouseover") {
      this.highlighter.hide();
    }
    else if (event.type == "mouseout") {
      this.highlighter.show();
    }
  },

  /**
   * Copy the innerHTML of the selected Node to the clipboard.
   */
  copyInnerHTML: function InspectorPanel_copyInnerHTML()
  {
    if (!this.selection.isNode()) {
      return;
    }
    let toCopy = this.selection.node.innerHTML;
    if (toCopy) {
      clipboardHelper.copyString(toCopy);
    }
  },

  /**
   * Copy the outerHTML of the selected Node to the clipboard.
   */
  copyOuterHTML: function InspectorPanel_copyOuterHTML()
  {
    if (!this.selection.isNode()) {
      return;
    }
    let toCopy = this.selection.node.outerHTML;
    if (toCopy) {
      clipboardHelper.copyString(toCopy);
    }
  },

  /**
   * Copy a unique selector of the selected Node to the clipboard.
   */
  copyUniqueSelector: function InspectorPanel_copyUniqueSelector()
  {
    if (!this.selection.isNode()) {
      return;
    }

    let toCopy = CssLogic.findCssSelector(this.selection.node);
    if (toCopy) {
      clipboardHelper.copyString(toCopy);
    }
  },

  /**
   * Delete the selected node.
   */
  deleteNode: function IUI_deleteNode() {
    if (!this.selection.isNode() ||
         this.selection.isRoot()) {
      return;
    }

    let toDelete = this.selection.node;

    let parent = this.selection.node.parentNode;

    // If the markup panel is active, use the markup panel to delete
    // the node, making this an undoable action.
    if (this.markup) {
      this.markup.deleteNode(toDelete);
    } else {
      // remove the node from content
      parent.removeChild(toDelete);
    }
  },

  /**
   * Schedule a low-priority change event for things like paint
   * and resize.
   */
  scheduleLayoutChange: function Inspector_scheduleLayoutChange()
  {
    if (this._timer) {
      return null;
    }
    this._timer = this.panelWin.setTimeout(function() {
      this.emit("layout-change");
      this._timer = null;
    }.bind(this), LAYOUT_CHANGE_TIMER);
  },

  /**
   * Cancel a pending low-priority change event if any is
   * scheduled.
   */
  cancelLayoutChange: function Inspector_cancelLayoutChange()
  {
    if (this._timer) {
      this.panelWin.clearTimeout(this._timer);
      delete this._timer;
    }
  },

}

/////////////////////////////////////////////////////////////////////////
//// Initializers

XPCOMUtils.defineLazyGetter(InspectorPanel.prototype, "strings",
  function () {
    return Services.strings.createBundle(
            "chrome://browser/locale/devtools/inspector.properties");
  });

XPCOMUtils.defineLazyGetter(this, "clipboardHelper", function() {
  return Cc["@mozilla.org/widget/clipboardhelper;1"].
    getService(Ci.nsIClipboardHelper);
});


XPCOMUtils.defineLazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});
