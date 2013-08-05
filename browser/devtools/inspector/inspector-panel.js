/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cc, Ci, Cu, Cr} = require("chrome");

Cu.import("resource://gre/modules/Services.jsm");

let promise = require("sdk/core/promise");
let EventEmitter = require("devtools/shared/event-emitter");
let {CssLogic} = require("devtools/styleinspector/css-logic");

loader.lazyGetter(this, "MarkupView", () => require("devtools/markupview/markup-view").MarkupView);
loader.lazyGetter(this, "Selection", () => require("devtools/inspector/selection").Selection);
loader.lazyGetter(this, "HTMLBreadcrumbs", () => require("devtools/inspector/breadcrumbs").HTMLBreadcrumbs);
loader.lazyGetter(this, "Highlighter", () => require("devtools/inspector/highlighter").Highlighter);
loader.lazyGetter(this, "ToolSidebar", () => require("devtools/framework/sidebar").ToolSidebar);
loader.lazyGetter(this, "SelectorSearch", () => require("devtools/inspector/selector-search").SelectorSearch);

const LAYOUT_CHANGE_TIMER = 250;

/**
 * Represents an open instance of the Inspector for a tab.
 * The inspector controls the highlighter, the breadcrumbs,
 * the markup view, and the sidebar (computed view, rule view
 * and layout view).
 */
function InspectorPanel(iframeWindow, toolbox) {
  this._toolbox = toolbox;
  this._target = toolbox._target;
  this.panelDoc = iframeWindow.document;
  this.panelWin = iframeWindow;
  this.panelWin.inspector = this;

  EventEmitter.decorate(this);
}

exports.InspectorPanel = InspectorPanel;

InspectorPanel.prototype = {
  /**
   * open is effectively an asynchronous constructor
   */
  open: function InspectorPanel_open() {
    return this.target.makeRemote().then(() => {
      return this.target.inspector.getWalker();
    }).then(walker => {
      if (this._destroyPromise) {
        walker.release().then(null, console.error);
      }
      this.walker = walker;
      return this._getDefaultNodeForSelection();
    }).then(defaultSelection => {
      return this._deferredOpen(defaultSelection);
    }).then(null, console.error);
  },

  _deferredOpen: function(defaultSelection) {
    let deferred = promise.defer();

    this.onNavigatedAway = this.onNavigatedAway.bind(this);
    this.target.on("navigate", this.onNavigatedAway);

    this.nodemenu = this.panelDoc.getElementById("inspector-node-popup");
    this.lastNodemenuItem = this.nodemenu.lastChild;
    this._setupNodeMenu = this._setupNodeMenu.bind(this);
    this._resetNodeMenu = this._resetNodeMenu.bind(this);
    this.nodemenu.addEventListener("popupshowing", this._setupNodeMenu, true);
    this.nodemenu.addEventListener("popuphiding", this._resetNodeMenu, true);

    // Create an empty selection
    this._selection = new Selection(this.walker);
    this.onNewSelection = this.onNewSelection.bind(this);
    this.selection.on("new-node-front", this.onNewSelection);
    this.onBeforeNewSelection = this.onBeforeNewSelection.bind(this);
    this.selection.on("before-new-node-front", this.onBeforeNewSelection);
    this.onDetached = this.onDetached.bind(this);
    this.selection.on("detached-front", this.onDetached);

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
      this._selection.setNodeFront(defaultSelection);

      this.markup.expandNode(this.selection.nodeFront);

      this.emit("ready");
      deferred.resolve(this);
    }.bind(this));

    this.setupSearchBox();
    this.setupSidebar();

    return deferred.promise;
  },

  /**
   * Return a promise that will resolve to the default node for selection.
   */
  _getDefaultNodeForSelection: function() {
    if (this._defaultNode) {
      return this._defaultNode;
    }
    let walker = this.walker;

    // if available set body node as default selected node
    // else set documentElement
    return walker.getRootNode().then(rootNode => {
      return walker.querySelector(rootNode, "body");
    }).then(front => {
      if (front) {
        return front;
      }
      return this.walker.documentElement(this.walker.rootNode);
    }).then(node => {
      if (walker !== this.walker) {
        promise.reject(null);
      }
      this._defaultNode = node;
      return node;
    })
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
   * Hooks the searchbar to show result and auto completion suggestions.
   */
  setupSearchBox: function InspectorPanel_setupSearchBox() {
    let searchDoc;
    if (this.target.isLocalTab) {
      searchDoc = this.browser.contentDocument;
    } else if (this.target.window) {
      searchDoc = this.target.window.document;
    } else {
      searchDoc = null;
    }
    // Initiate the selectors search object.
    let setNodeFunction = function(eventName, node) {
      this.selection.setNodeFront(node, "selectorsearch");
    }.bind(this);
    if (this.searchSuggestions) {
      this.searchSuggestions.destroy();
      this.searchSuggestions = null;
    }
    this.searchBox = this.panelDoc.getElementById("inspector-searchbox");
    this.searchSuggestions = new SelectorSearch(this, searchDoc, this.searchBox);
    this.searchSuggestions.on("node-selected", setNodeFunction);
  },

  /**
   * Build the sidebar.
   */
  setupSidebar: function InspectorPanel_setupSidebar() {
    let tabbox = this.panelDoc.querySelector("#inspector-sidebar");
    this.sidebar = new ToolSidebar(tabbox, this, "inspector");

    let defaultTab = Services.prefs.getCharPref("devtools.inspector.activeSidebar");

    this._setDefaultSidebar = function(event, toolId) {
      Services.prefs.setCharPref("devtools.inspector.activeSidebar", toolId);
    }.bind(this);

    this.sidebar.on("select", this._setDefaultSidebar);
    this.toggleHighlighter = this.toggleHighlighter.bind(this);

    this.sidebar.addTab("ruleview",
                        "chrome://browser/content/devtools/cssruleview.xhtml",
                        "ruleview" == defaultTab);

    this.sidebar.addTab("computedview",
                        "chrome://browser/content/devtools/computedview.xhtml",
                        "computedview" == defaultTab);

    if (Services.prefs.getBoolPref("devtools.fontinspector.enabled")) {
      this.sidebar.addTab("fontinspector",
                          "chrome://browser/content/devtools/fontinspector/font-inspector.xhtml",
                          "fontinspector" == defaultTab);
    }

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
  onNavigatedAway: function InspectorPanel_onNavigatedAway(event, payload) {
    let newWindow = payload._navPayload || payload;
    this._defaultNode = null;
    this.selection.setNodeFront(null);
    this._destroyMarkup();
    this.isDirty = false;

    this._getDefaultNodeForSelection().then(defaultNode => {
      if (this._destroyPromise) {
        return;
      }
      this.selection.setNodeFront(defaultNode, "navigateaway");

      this._initMarkup();
      this.once("markuploaded", () => {
        this.markup.expandNode(this.selection.nodeFront);
        this.setupSearchBox();
      });
    });
  },

  /**
   * When a new node is selected.
   */
  onNewSelection: function InspectorPanel_onNewSelection() {
    this.cancelLayoutChange();

    // Wait for all the known tools to finish updating and then let the
    // client know.
    let selection = this.selection.nodeFront;
    let selfUpdate = this.updating("inspector-panel");
    Services.tm.mainThread.dispatch(() => {
      try {
        selfUpdate(selection);
      } catch(ex) {
        console.error(ex);
      }
    }, Ci.nsIThread.DISPATCH_NORMAL);
  },

  /**
   * Delay the "inspector-updated" notification while a tool
   * is updating itself.  Returns a function that must be
   * invoked when the tool is done updating with the node
   * that the tool is viewing.
   */
  updating: function(name) {
    if (this._updateProgress && this._updateProgress.node != this.selection.nodeFront) {
      this.cancelUpdate();
    }

    if (!this._updateProgress) {
      // Start an update in progress.
      var self = this;
      this._updateProgress = {
        node: this.selection.nodeFront,
        outstanding: new Set(),
        checkDone: function() {
          if (this !== self._updateProgress) {
            return;
          }
          if (this.node !== self.selection.nodeFront) {
            self.cancelUpdate();
            return;
          }
          if (this.outstanding.size !== 0) {
            return;
          }

          self._updateProgress = null;
          self.emit("inspector-updated");
        },
      }
    }

    let progress = this._updateProgress;
    let done = function() {
      progress.outstanding.delete(done);
      progress.checkDone();
    };
    progress.outstanding.add(done);
    return done;
  },

  /**
   * Cancel notification of inspector updates.
   */
  cancelUpdate: function() {
    this._updateProgress = null;
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
    this.selection.setNodeFront(parentNode ? parentNode : this._defaultNode, "detached");
  },

  /**
   * Destroy the inspector.
   */
  destroy: function InspectorPanel__destroy() {
    if (this._destroyPromise) {
      return this._destroyPromise;
    }
    if (this.walker) {
      this._destroyPromise = this.walker.release().then(null, console.error);
      delete this.walker;
    } else {
      this._destroyPromise = promise.resolve(null);
    }

    this.cancelUpdate();
    this.cancelLayoutChange();

    if (this.browser) {
      this.browser.removeEventListener("resize", this.scheduleLayoutChange, true);
      this.browser = null;
    }

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
    this.breadcrumbs.destroy();
    this.searchSuggestions.destroy();
    this.selection.off("new-node-front", this.onNewSelection);
    this.selection.off("before-new-node", this.onBeforeNewSelection);
    this.selection.off("before-new-node-front", this.onBeforeNewSelection);
    this.selection.off("detached-front", this.onDetached);
    this._destroyMarkup();
    this._selection.destroy();
    this._selection = null;
    this.panelWin.inspector = null;
    this.target = null;
    this.panelDoc = null;
    this.panelWin = null;
    this.breadcrumbs = null;
    this.searchSuggestions = null;
    this.lastNodemenuItem = null;
    this.nodemenu = null;
    this.highlighter = null;

    return this._destroyPromise;
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
        let checked = this.selection.nodeFront.hasPseudoClassLock(":" + name);
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
      let node = this.selection.nodeFront;
      if (node.hasPseudoClassLock(aPseudo)) {
        return this.walker.removePseudoClassLock(node, aPseudo, { parents: true });
      }

      let hierarchical = aPseudo == ":hover" || aPseudo == ":active";
      return this.walker.addPseudoClassLock(node, aPseudo, { parents: hierarchical });
    }
  },

  /**
   * Clear any pseudo-class locks applied to the current hierarchy.
   */
  clearPseudoClasses: function InspectorPanel_clearPseudoClasses() {
    if (!this.walker) {
      return;
    }
    return this.walker.clearPseudoClassLocks().then(null, console.error);
  },

  /**
   * Toggle the highlighter when ruleview is hovered.
   */
  toggleHighlighter: function InspectorPanel_toggleHighlighter(event)
  {
    if (!this.highlighter) {
      return;
    }
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
    this._copyLongStr(this.walker.innerHTML(this.selection.nodeFront));
  },

  /**
   * Copy the outerHTML of the selected Node to the clipboard.
   */
  copyOuterHTML: function InspectorPanel_copyOuterHTML()
  {
    if (!this.selection.isNode()) {
      return;
    }

    this._copyLongStr(this.walker.outerHTML(this.selection.nodeFront));
  },

  _copyLongStr: function(promise) {
    return promise.then(longstr => {
      return longstr.string().then(toCopy => {
        longstr.release().then(null, console.error);
        clipboardHelper.copyString(toCopy);
      });
    }).then(null, console.error);
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

    // If the markup panel is active, use the markup panel to delete
    // the node, making this an undoable action.
    if (this.markup) {
      this.markup.deleteNode(this.selection.nodeFront);
    } else {
      // remove the node from content
      this.walker.removeNode(this.selection.nodeFront);
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

loader.lazyGetter(InspectorPanel.prototype, "strings",
  function () {
    return Services.strings.createBundle(
            "chrome://browser/locale/devtools/inspector.properties");
  });

loader.lazyGetter(this, "clipboardHelper", function() {
  return Cc["@mozilla.org/widget/clipboardhelper;1"].
    getService(Ci.nsIClipboardHelper);
});


loader.lazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});
