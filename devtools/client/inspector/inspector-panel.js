/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* Experimenting with 100 char long lines */
/* eslint max-len: [2, 100, 2, {ignoreUrls: true, "ignorePattern": "\\s*require\\s*\\(|^\\s*loader\\.lazy|-\\*-"}] */ // eslint-disable-line

"use strict";

const {Cc, Ci} = require("chrome");

var Services = require("Services");
var promise = require("promise");
var defer = require("devtools/shared/defer");
var EventEmitter = require("devtools/shared/event-emitter");
var clipboard = require("sdk/clipboard");
const {executeSoon} = require("devtools/shared/DevToolsUtils");
var {KeyShortcuts} = require("devtools/client/shared/key-shortcuts");
var {Task} = require("devtools/shared/task");
const {initCssProperties} = require("devtools/shared/fronts/css-properties");
const nodeConstants = require("devtools/shared/dom-node-constants");

const Menu = require("devtools/client/framework/menu");
const MenuItem = require("devtools/client/framework/menu-item");

loader.lazyRequireGetter(this, "CSS", "CSS");

loader.lazyRequireGetter(this, "CommandUtils", "devtools/client/shared/developer-toolbar", true);
loader.lazyRequireGetter(this, "ComputedViewTool", "devtools/client/inspector/computed/computed", true);
loader.lazyRequireGetter(this, "FontInspector", "devtools/client/inspector/fonts/fonts", true);
loader.lazyRequireGetter(this, "HTMLBreadcrumbs", "devtools/client/inspector/breadcrumbs", true);
loader.lazyRequireGetter(this, "InspectorSearch", "devtools/client/inspector/inspector-search", true);
loader.lazyRequireGetter(this, "MarkupView", "devtools/client/inspector/markup/markup", true);
loader.lazyRequireGetter(this, "RuleViewTool", "devtools/client/inspector/rules/rules", true);
loader.lazyRequireGetter(this, "ToolSidebar", "devtools/client/inspector/toolsidebar", true);
loader.lazyRequireGetter(this, "ViewHelpers", "devtools/client/shared/widgets/view-helpers", true);

loader.lazyGetter(this, "strings", () => {
  return Services.strings.createBundle("chrome://devtools/locale/inspector.properties");
});
loader.lazyGetter(this, "toolboxStrings", () => {
  return Services.strings.createBundle("chrome://devtools/locale/toolbox.properties");
});
loader.lazyGetter(this, "clipboardHelper", () => {
  return Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
});

/**
 * Represents an open instance of the Inspector for a tab.
 * The inspector controls the breadcrumbs, the markup view, and the sidebar
 * (computed view, rule view, font view and layout view).
 *
 * Events:
 * - ready
 *      Fired when the inspector panel is opened for the first time and ready to
 *      use
 * - new-root
 *      Fired after a new root (navigation to a new page) event was fired by
 *      the walker, and taken into account by the inspector (after the markup
 *      view has been reloaded)
 * - markuploaded
 *      Fired when the markup-view frame has loaded
 * - breadcrumbs-updated
 *      Fired when the breadcrumb widget updates to a new node
 * - layoutview-updated
 *      Fired when the layoutview (box model) updates to a new node
 * - markupmutation
 *      Fired after markup mutations have been processed by the markup-view
 * - computed-view-refreshed
 *      Fired when the computed rules view updates to a new node
 * - computed-view-property-expanded
 *      Fired when a property is expanded in the computed rules view
 * - computed-view-property-collapsed
 *      Fired when a property is collapsed in the computed rules view
 * - computed-view-sourcelinks-updated
 *      Fired when the stylesheet source links have been updated (when switching
 *      to source-mapped files)
 * - rule-view-refreshed
 *      Fired when the rule view updates to a new node
 * - rule-view-sourcelinks-updated
 *      Fired when the stylesheet source links have been updated (when switching
 *      to source-mapped files)
 */
function InspectorPanel(iframeWindow, toolbox) {
  this._toolbox = toolbox;
  this._target = toolbox._target;
  this.panelDoc = iframeWindow.document;
  this.panelWin = iframeWindow;
  this.panelWin.inspector = this;

  this.nodeMenuTriggerInfo = null;

  this._onBeforeNavigate = this._onBeforeNavigate.bind(this);
  this.onNewRoot = this.onNewRoot.bind(this);
  this._onContextMenu = this._onContextMenu.bind(this);
  this._updateSearchResultsLabel = this._updateSearchResultsLabel.bind(this);
  this.onNewSelection = this.onNewSelection.bind(this);
  this.onBeforeNewSelection = this.onBeforeNewSelection.bind(this);
  this.onDetached = this.onDetached.bind(this);
  this.onPaneToggleButtonClicked = this.onPaneToggleButtonClicked.bind(this);
  this._onMarkupFrameLoad = this._onMarkupFrameLoad.bind(this);

  let doc = this.panelDoc;

  // Handle 'Add Node' toolbar button.
  this.addNode = this.addNode.bind(this);
  this.addNodeButton = doc.getElementById("inspector-element-add-button");
  this.addNodeButton.addEventListener("click", this.addNode);

  this._target.on("will-navigate", this._onBeforeNavigate);
  this._detectingActorFeatures = this._detectActorFeatures();

  EventEmitter.decorate(this);
}

exports.InspectorPanel = InspectorPanel;

InspectorPanel.prototype = {
  /**
   * open is effectively an asynchronous constructor
   */
  open: Task.async(function* () {
    this._cssPropertiesLoaded = initCssProperties(this.toolbox);
    yield this._cssPropertiesLoaded;
    yield this.target.makeRemote();
    yield this._getPageStyle();
    let defaultSelection = yield this._getDefaultNodeForSelection();
    return yield this._deferredOpen(defaultSelection);
  }),

  get toolbox() {
    return this._toolbox;
  },

  get inspector() {
    return this._toolbox.inspector;
  },

  get walker() {
    return this._toolbox.walker;
  },

  get selection() {
    return this._toolbox.selection;
  },

  get isOuterHTMLEditable() {
    return this._target.client.traits.editOuterHTML;
  },

  get hasUrlToImageDataResolver() {
    return this._target.client.traits.urlToImageDataResolver;
  },

  get canGetUniqueSelector() {
    return this._target.client.traits.getUniqueSelector;
  },

  get canGetUsedFontFaces() {
    return this._target.client.traits.getUsedFontFaces;
  },

  get canPasteInnerOrAdjacentHTML() {
    return this._target.client.traits.pasteHTML;
  },

  /**
   * Figure out what features the backend supports
   */
  _detectActorFeatures: function () {
    this._supportsDuplicateNode = false;
    this._supportsScrollIntoView = false;
    this._supportsResolveRelativeURL = false;

    return promise.all([
      this._target.actorHasMethod("domwalker", "duplicateNode").then(value => {
        this._supportsDuplicateNode = value;
      }).catch(e => console.error(e)),
      this._target.actorHasMethod("domnode", "scrollIntoView").then(value => {
        this._supportsScrollIntoView = value;
      }).catch(e => console.error(e)),
      this._target.actorHasMethod("inspector", "resolveRelativeURL").then(value => {
        this._supportsResolveRelativeURL = value;
      }).catch(e => console.error(e)),
    ]);
  },

  _deferredOpen: function (defaultSelection) {
    let deferred = defer();

    this.walker.on("new-root", this.onNewRoot);

    this.selection.on("new-node-front", this.onNewSelection);
    this.selection.on("before-new-node-front", this.onBeforeNewSelection);
    this.selection.on("detached-front", this.onDetached);

    this.breadcrumbs = new HTMLBreadcrumbs(this);

    if (this.target.isLocalTab) {
      // Show a warning when the debugger is paused.
      // We show the warning only when the inspector
      // is selected.
      this.updateDebuggerPausedWarning = () => {
        let notificationBox = this._toolbox.getNotificationBox();
        let notification = notificationBox.getNotificationWithValue("inspector-script-paused");
        if (!notification && this._toolbox.currentToolId == "inspector" &&
            this._toolbox.threadClient.paused) {
          let message = strings.GetStringFromName("debuggerPausedWarning.message");
          notificationBox.appendNotification(message,
            "inspector-script-paused", "", notificationBox.PRIORITY_WARNING_HIGH);
        }

        if (notification && this._toolbox.currentToolId != "inspector") {
          notificationBox.removeNotification(notification);
        }

        if (notification && !this._toolbox.threadClient.paused) {
          notificationBox.removeNotification(notification);
        }
      };
      this.target.on("thread-paused", this.updateDebuggerPausedWarning);
      this.target.on("thread-resumed", this.updateDebuggerPausedWarning);
      this._toolbox.on("select", this.updateDebuggerPausedWarning);
      this.updateDebuggerPausedWarning();
    }

    this._initMarkup();
    this.isReady = false;

    this.once("markuploaded", () => {
      this.isReady = true;

      // All the components are initialized. Let's select a node.
      this.selection.setNodeFront(defaultSelection, "inspector-open");

      this.markup.expandNode(this.selection.nodeFront);

      this.emit("ready");
      deferred.resolve(this);
    });

    this.setupSearchBox();
    this.setupSidebar();

    return deferred.promise;
  },

  _onBeforeNavigate: function () {
    this._defaultNode = null;
    this.selection.setNodeFront(null);
    this._destroyMarkup();
    this.isDirty = false;
    this._pendingSelection = null;
  },

  _getPageStyle: function () {
    return this._toolbox.inspector.getPageStyle().then(pageStyle => {
      this.pageStyle = pageStyle;
    });
  },

  /**
   * Return a promise that will resolve to the default node for selection.
   */
  _getDefaultNodeForSelection: function () {
    if (this._defaultNode) {
      return this._defaultNode;
    }
    let walker = this.walker;
    let rootNode = null;
    let pendingSelection = this._pendingSelection;

    // A helper to tell if the target has or is about to navigate.
    // this._pendingSelection changes on "will-navigate" and "new-root" events.
    let hasNavigated = () => pendingSelection !== this._pendingSelection;

    // If available, set either the previously selected node or the body
    // as default selected, else set documentElement
    return walker.getRootNode().then(node => {
      if (hasNavigated()) {
        return promise.reject("navigated; resolution of _defaultNode aborted");
      }

      rootNode = node;
      if (this.selectionCssSelector) {
        return walker.querySelector(rootNode, this.selectionCssSelector);
      }
      return null;
    }).then(front => {
      if (hasNavigated()) {
        return promise.reject("navigated; resolution of _defaultNode aborted");
      }

      if (front) {
        return front;
      }
      return walker.querySelector(rootNode, "body");
    }).then(front => {
      if (hasNavigated()) {
        return promise.reject("navigated; resolution of _defaultNode aborted");
      }

      if (front) {
        return front;
      }
      return this.walker.documentElement();
    }).then(node => {
      if (hasNavigated()) {
        return promise.reject("navigated; resolution of _defaultNode aborted");
      }
      this._defaultNode = node;
      return node;
    });
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
   * Indicate that a tool has modified the state of the page.  Used to
   * decide whether to show the "are you sure you want to navigate"
   * notification.
   */
  markDirty: function () {
    this.isDirty = true;
  },

  /**
   * Hooks the searchbar to show result and auto completion suggestions.
   */
  setupSearchBox: function () {
    this.searchBox = this.panelDoc.getElementById("inspector-searchbox");
    this.searchResultsLabel = this.panelDoc.getElementById("inspector-searchlabel");

    this.search = new InspectorSearch(this, this.searchBox);
    this.search.on("search-cleared", this._updateSearchResultsLabel);
    this.search.on("search-result", this._updateSearchResultsLabel);

    let shortcuts = new KeyShortcuts({
      window: this.panelDoc.defaultView,
    });
    let key = strings.GetStringFromName("inspector.searchHTML.key");
    shortcuts.on(key, (name, event) => {
      // Prevent overriding same shortcut from the computed/rule views
      if (event.target.closest("#sidebar-panel-ruleview") ||
          event.target.closest("#sidebar-panel-computedview")) {
        return;
      }
      event.preventDefault();
      this.searchBox.focus();
    });
  },

  get searchSuggestions() {
    return this.search.autocompleter;
  },

  _updateSearchResultsLabel: function (event, result) {
    let str = "";
    if (event !== "search-cleared") {
      if (result) {
        str = strings.formatStringFromName(
          "inspector.searchResultsCount2",
          [result.resultsIndex + 1, result.resultsLength], 2);
      } else {
        str = strings.GetStringFromName("inspector.searchResultsNone");
      }
    }

    this.searchResultsLabel.textContent = str;
  },

  get React() {
    return this._toolbox.React;
  },

  get ReactDOM() {
    return this._toolbox.ReactDOM;
  },

  get browserRequire() {
    return this._toolbox.browserRequire;
  },

  /**
   * Build the sidebar.
   */
  setupSidebar: function () {
    let tabbox = this.panelDoc.querySelector("#inspector-sidebar");
    this.sidebar = new ToolSidebar(tabbox, this, "inspector", {
      showAllTabsMenu: true
    });

    let defaultTab = Services.prefs.getCharPref("devtools.inspector.activeSidebar");

    if (!Services.prefs.getBoolPref("devtools.fontinspector.enabled") &&
       defaultTab == "fontinspector") {
      defaultTab = "ruleview";
    }

    // Append all side panels
    this.sidebar.addExistingTab(
      "ruleview",
      strings.GetStringFromName("inspector.sidebar.ruleViewTitle"),
      defaultTab == "ruleview");

    this.sidebar.addExistingTab(
      "computedview",
      strings.GetStringFromName("inspector.sidebar.computedViewTitle"),
      defaultTab == "computedview");

    this._setDefaultSidebar = (event, toolId) => {
      Services.prefs.setCharPref("devtools.inspector.activeSidebar", toolId);
    };

    this.sidebar.on("select", this._setDefaultSidebar);

    this.ruleview = new RuleViewTool(this, this.panelWin);
    this.computedview = new ComputedViewTool(this, this.panelWin);

    if (this.target.form.animationsActor) {
      this.sidebar.addFrameTab(
        "animationinspector",
        strings.GetStringFromName("inspector.sidebar.animationInspectorTitle"),
        "chrome://devtools/content/animationinspector/animation-inspector.xhtml",
        defaultTab == "animationinspector");
    }

    if (Services.prefs.getBoolPref("devtools.fontinspector.enabled") &&
        this.canGetUsedFontFaces) {
      this.sidebar.addExistingTab(
        "fontinspector",
        strings.GetStringFromName("inspector.sidebar.fontInspectorTitle"),
        defaultTab == "fontinspector");

      this.fontInspector = new FontInspector(this, this.panelWin);
      this.sidebar.toggleTab(true, "fontinspector");
    }

    this.setupSidebarToggle();
    this.setupSidebarSize();

    this.sidebar.show(defaultTab);
  },

  /**
   * Sidebar size is currently driven by vbox.inspector-sidebar-container
   * element, which is located at the left/bottom side of the side bar splitter.
   * Its size is changed by the splitter and stored into preferences.
   * As soon as bug 1260552 is fixed and new HTML based splitter in place
   * the size can be driven by div.inspector-sidebar element. This element
   * represents the ToolSidebar and so, the entire logic related to size
   * persistence can be done inside the ToolSidebar.
   */
  setupSidebarSize: function () {
    let sidePaneContainer = this.panelDoc.querySelector(
      "#inspector-sidebar-container");

    this.sidebar.on("show", () => {
      try {
        sidePaneContainer.width = Services.prefs.getIntPref(
          "devtools.toolsidebar-width.inspector");
        sidePaneContainer.height = Services.prefs.getIntPref(
          "devtools.toolsidebar-height.inspector");
      } catch (e) {
        // The default width is the min-width set in CSS
        // for #inspector-sidebar-container
        // Set width and height of the sidebar container. Only one
        // value is really useful at a time depending on the current
        // toolbox orientation and having both doesn't break anything.
        sidePaneContainer.width = 450;
        sidePaneContainer.height = 450;
      }
    });

    this.sidebar.on("hide", () => {
      Services.prefs.setIntPref("devtools.toolsidebar-width.inspector",
        sidePaneContainer.width);
      Services.prefs.setIntPref("devtools.toolsidebar-height.inspector",
        sidePaneContainer.height);
    });

    this.sidebar.on("destroy", () => {
      Services.prefs.setIntPref("devtools.toolsidebar-width.inspector",
        sidePaneContainer.width);
      Services.prefs.setIntPref("devtools.toolsidebar-height.inspector",
        sidePaneContainer.height);
    });
  },

  /**
   * Add the expand/collapse behavior for the sidebar panel.
   */
  setupSidebarToggle: function () {
    let SidebarToggle = this.React.createFactory(this.browserRequire(
      "devtools/client/shared/components/sidebar-toggle"));

    let sidebarToggle = SidebarToggle({
      onClick: this.onPaneToggleButtonClicked,
      collapsed: false,
      expandPaneTitle: strings.GetStringFromName("inspector.expandPane"),
      collapsePaneTitle: strings.GetStringFromName("inspector.collapsePane"),
    });

    let parentBox = this.panelDoc.getElementById("inspector-sidebar-toggle-box");
    this._sidebarToggle = this.ReactDOM.render(sidebarToggle, parentBox);
  },

  /**
   * Reset the inspector on new root mutation.
   */
  onNewRoot: function () {
    this._defaultNode = null;
    this.selection.setNodeFront(null);
    this._destroyMarkup();
    this.isDirty = false;

    let onNodeSelected = defaultNode => {
      // Cancel this promise resolution as a new one had
      // been queued up.
      if (this._pendingSelection != onNodeSelected) {
        return;
      }
      this._pendingSelection = null;
      this.selection.setNodeFront(defaultNode, "navigateaway");

      this._initMarkup();
      this.once("markuploaded", () => {
        if (!this.markup) {
          return;
        }
        this.markup.expandNode(this.selection.nodeFront);
        this.emit("new-root");
      });
    };
    this._pendingSelection = onNodeSelected;
    this._getDefaultNodeForSelection().then(onNodeSelected, console.error);
  },

  _selectionCssSelector: null,

  /**
   * Set the currently selected node unique css selector.
   * Will store the current target url along with it to allow pre-selection at
   * reload
   */
  set selectionCssSelector(cssSelector = null) {
    if (this._panelDestroyer) {
      return;
    }

    this._selectionCssSelector = {
      selector: cssSelector,
      url: this._target.url
    };
  },

  /**
   * Get the current selection unique css selector if any, that is, if a node
   * is actually selected and that node has been selected while on the same url
   */
  get selectionCssSelector() {
    if (this._selectionCssSelector &&
        this._selectionCssSelector.url === this._target.url) {
      return this._selectionCssSelector.selector;
    }
    return null;
  },

  /**
   * Can a new HTML element be inserted into the currently selected element?
   * @return {Boolean}
   */
  canAddHTMLChild: function () {
    let selection = this.selection;

    // Don't allow to insert an element into these elements. This should only
    // contain elements where walker.insertAdjacentHTML has no effect.
    let invalidTagNames = ["html", "iframe"];

    return selection.isHTMLNode() &&
           selection.isElementNode() &&
           !selection.isPseudoElementNode() &&
           !selection.isAnonymousNode() &&
           invalidTagNames.indexOf(
            selection.nodeFront.nodeName.toLowerCase()) === -1;
  },

  /**
   * When a new node is selected.
   */
  onNewSelection: function (event, value, reason) {
    if (reason === "selection-destroy") {
      return;
    }

    // Wait for all the known tools to finish updating and then let the
    // client know.
    let selection = this.selection.nodeFront;

    // Update the state of the add button in the toolbar depending on the
    // current selection.
    let btn = this.panelDoc.querySelector("#inspector-element-add-button");
    if (this.canAddHTMLChild()) {
      btn.removeAttribute("disabled");
    } else {
      btn.setAttribute("disabled", "true");
    }

    // On any new selection made by the user, store the unique css selector
    // of the selected node so it can be restored after reload of the same page
    if (this.canGetUniqueSelector &&
        this.selection.isElementNode()) {
      selection.getUniqueSelector().then(selector => {
        this.selectionCssSelector = selector;
      }).then(null, e => {
        // Only log this as an error if the panel hasn't been destroyed in the
        // meantime.
        if (!this._panelDestroyer) {
          console.error(e);
        } else {
          console.warn("Could not set the unique selector for the newly " +
            "selected node, the inspector was destroyed.");
        }
      });
    }

    let selfUpdate = this.updating("inspector-panel");
    executeSoon(() => {
      try {
        selfUpdate(selection);
      } catch (ex) {
        console.error(ex);
      }
    });
  },

  /**
   * Delay the "inspector-updated" notification while a tool
   * is updating itself.  Returns a function that must be
   * invoked when the tool is done updating with the node
   * that the tool is viewing.
   */
  updating: function (name) {
    if (this._updateProgress && this._updateProgress.node != this.selection.nodeFront) {
      this.cancelUpdate();
    }

    if (!this._updateProgress) {
      // Start an update in progress.
      let self = this;
      this._updateProgress = {
        node: this.selection.nodeFront,
        outstanding: new Set(),
        checkDone: function () {
          if (this !== self._updateProgress) {
            return;
          }
          // Cancel update if there is no `selection` anymore.
          // It can happen if the inspector panel is already destroyed.
          if (!self.selection || (this.node !== self.selection.nodeFront)) {
            self.cancelUpdate();
            return;
          }
          if (this.outstanding.size !== 0) {
            return;
          }

          self._updateProgress = null;
          self.emit("inspector-updated", name);
        },
      };
    }

    let progress = this._updateProgress;
    let done = function () {
      progress.outstanding.delete(done);
      progress.checkDone();
    };
    progress.outstanding.add(done);
    return done;
  },

  /**
   * Cancel notification of inspector updates.
   */
  cancelUpdate: function () {
    this._updateProgress = null;
  },

  /**
   * When a new node is selected, before the selection has changed.
   */
  onBeforeNewSelection: function (event, node) {
    if (this.breadcrumbs.indexOf(node) == -1) {
      // only clear locks if we'd have to update breadcrumbs
      this.clearPseudoClasses();
    }
  },

  /**
   * When a node is deleted, select its parent node or the defaultNode if no
   * parent is found (may happen when deleting an iframe inside which the
   * node was selected).
   */
  onDetached: function (event, parentNode) {
    this.breadcrumbs.cutAfter(this.breadcrumbs.indexOf(parentNode));
    this.selection.setNodeFront(parentNode ? parentNode : this._defaultNode, "detached");
  },

  /**
   * Destroy the inspector.
   */
  destroy: function () {
    if (this._panelDestroyer) {
      return this._panelDestroyer;
    }

    if (this.walker) {
      this.walker.off("new-root", this.onNewRoot);
      this.pageStyle = null;
    }

    this.cancelUpdate();

    this.target.off("will-navigate", this._onBeforeNavigate);

    this.target.off("thread-paused", this.updateDebuggerPausedWarning);
    this.target.off("thread-resumed", this.updateDebuggerPausedWarning);
    this._toolbox.off("select", this.updateDebuggerPausedWarning);

    if (this.ruleview) {
      this.ruleview.destroy();
    }

    if (this.computedview) {
      this.computedview.destroy();
    }

    if (this.fontInspector) {
      this.fontInspector.destroy();
    }

    let cssPropertiesDestroyer = this._cssPropertiesLoaded.then(({front}) => {
      if (front) {
        front.destroy();
      }
    });

    this.sidebar.off("select", this._setDefaultSidebar);
    let sidebarDestroyer = this.sidebar.destroy();
    this.sidebar = null;

    this.addNodeButton.removeEventListener("click", this.addNode);
    this.breadcrumbs.destroy();
    this.selection.off("new-node-front", this.onNewSelection);
    this.selection.off("before-new-node", this.onBeforeNewSelection);
    this.selection.off("before-new-node-front", this.onBeforeNewSelection);
    this.selection.off("detached-front", this.onDetached);
    let markupDestroyer = this._destroyMarkup();
    this.panelWin.inspector = null;
    this.target = null;
    this.panelDoc = null;
    this.panelWin = null;
    this.breadcrumbs = null;
    this._toolbox = null;
    this.search.destroy();
    this.search = null;
    this.searchBox = null;

    this._panelDestroyer = promise.all([
      sidebarDestroyer,
      markupDestroyer,
      cssPropertiesDestroyer
    ]);

    return this._panelDestroyer;
  },

  /**
   * Returns the clipboard content if it is appropriate for pasting
   * into the current node's outer HTML, otherwise returns null.
   */
  _getClipboardContentForPaste: function () {
    let flavors = clipboard.currentFlavors;
    if (flavors.indexOf("text") != -1 ||
        (flavors.indexOf("html") != -1 && flavors.indexOf("image") == -1)) {
      let content = clipboard.get();
      if (content && content.trim().length > 0) {
        return content;
      }
    }
    return null;
  },

  _onContextMenu: function (e) {
    e.preventDefault();
    this._openMenu({
      screenX: e.screenX,
      screenY: e.screenY,
      target: e.target,
    });
  },

  _openMenu: function ({ target, screenX = 0, screenY = 0 } = { }) {
    let markupContainer = this.markup.getContainer(this.selection.nodeFront);

    this.contextMenuTarget = target;
    this.nodeMenuTriggerInfo = markupContainer &&
      markupContainer.editor.getInfoAtNode(target);

    let isSelectionElement = this.selection.isElementNode() &&
                             !this.selection.isPseudoElementNode();
    let isEditableElement = isSelectionElement &&
                            !this.selection.isAnonymousNode();
    let isDuplicatableElement = isSelectionElement &&
                                !this.selection.isAnonymousNode() &&
                                !this.selection.isRoot();
    let isScreenshotable = isSelectionElement &&
                           this.canGetUniqueSelector &&
                           this.selection.nodeFront.isTreeDisplayed;

    let menu = new Menu();
    menu.append(new MenuItem({
      id: "node-menu-edithtml",
      label: strings.GetStringFromName("inspectorHTMLEdit.label"),
      accesskey: strings.GetStringFromName("inspectorHTMLEdit.accesskey"),
      disabled: !isEditableElement || !this.isOuterHTMLEditable,
      click: () => this.editHTML(),
    }));
    menu.append(new MenuItem({
      id: "node-menu-add",
      label: strings.GetStringFromName("inspectorAddNode.label"),
      accesskey: strings.GetStringFromName("inspectorAddNode.accesskey"),
      disabled: !this.canAddHTMLChild(),
      click: () => this.addNode(),
    }));
    menu.append(new MenuItem({
      id: "node-menu-duplicatenode",
      label: strings.GetStringFromName("inspectorDuplicateNode.label"),
      hidden: !this._supportsDuplicateNode,
      disabled: !isDuplicatableElement,
      click: () => this.duplicateNode(),
    }));
    menu.append(new MenuItem({
      id: "node-menu-delete",
      label: strings.GetStringFromName("inspectorHTMLDelete.label"),
      accesskey: strings.GetStringFromName("inspectorHTMLDelete.accesskey"),
      disabled: !isEditableElement,
      click: () => this.deleteNode(),
    }));

    menu.append(new MenuItem({
      label: strings.GetStringFromName("inspectorAttributesSubmenu.label"),
      accesskey:
        strings.GetStringFromName("inspectorAttributesSubmenu.accesskey"),
      submenu: this._getAttributesSubmenu(isEditableElement),
    }));

    menu.append(new MenuItem({
      type: "separator",
    }));

    // Set the pseudo classes
    for (let name of ["hover", "active", "focus"]) {
      let menuitem = new MenuItem({
        id: "node-menu-pseudo-" + name,
        label: name,
        type: "checkbox",
        click: this.togglePseudoClass.bind(this, ":" + name),
      });

      if (isSelectionElement) {
        let checked = this.selection.nodeFront.hasPseudoClassLock(":" + name);
        menuitem.checked = checked;
      } else {
        menuitem.disabled = true;
      }

      menu.append(menuitem);
    }

    menu.append(new MenuItem({
      type: "separator",
    }));

    let copySubmenu = new Menu();
    copySubmenu.append(new MenuItem({
      id: "node-menu-copyinner",
      label: strings.GetStringFromName("inspectorCopyInnerHTML.label"),
      accesskey: strings.GetStringFromName("inspectorCopyInnerHTML.accesskey"),
      disabled: !isSelectionElement,
      click: () => this.copyInnerHTML(),
    }));
    copySubmenu.append(new MenuItem({
      id: "node-menu-copyouter",
      label: strings.GetStringFromName("inspectorCopyOuterHTML.label"),
      accesskey: strings.GetStringFromName("inspectorCopyOuterHTML.accesskey"),
      disabled: !isSelectionElement,
      click: () => this.copyOuterHTML(),
    }));
    copySubmenu.append(new MenuItem({
      id: "node-menu-copyuniqueselector",
      label: strings.GetStringFromName("inspectorCopyCSSSelector.label"),
      accesskey:
        strings.GetStringFromName("inspectorCopyCSSSelector.accesskey"),
      disabled: !isSelectionElement,
      hidden: !this.canGetUniqueSelector,
      click: () => this.copyUniqueSelector(),
    }));
    copySubmenu.append(new MenuItem({
      id: "node-menu-copyimagedatauri",
      label: strings.GetStringFromName("inspectorImageDataUri.label"),
      disabled: !isSelectionElement || !markupContainer ||
                !markupContainer.isPreviewable(),
      click: () => this.copyImageDataUri(),
    }));

    menu.append(new MenuItem({
      label: strings.GetStringFromName("inspectorCopyHTMLSubmenu.label"),
      submenu: copySubmenu,
    }));

    menu.append(new MenuItem({
      label: strings.GetStringFromName("inspectorPasteHTMLSubmenu.label"),
      submenu: this._getPasteSubmenu(isEditableElement),
    }));

    menu.append(new MenuItem({
      type: "separator",
    }));

    let isNodeWithChildren = this.selection.isNode() &&
                             markupContainer.hasChildren;
    menu.append(new MenuItem({
      id: "node-menu-expand",
      label: strings.GetStringFromName("inspectorExpandNode.label"),
      disabled: !isNodeWithChildren,
      click: () => this.expandNode(),
    }));
    menu.append(new MenuItem({
      id: "node-menu-collapse",
      label: strings.GetStringFromName("inspectorCollapseNode.label"),
      disabled: !isNodeWithChildren || !markupContainer.expanded,
      click: () => this.collapseNode(),
    }));

    menu.append(new MenuItem({
      type: "separator",
    }));

    menu.append(new MenuItem({
      id: "node-menu-scrollnodeintoview",
      label: strings.GetStringFromName("inspectorScrollNodeIntoView.label"),
      accesskey:
        strings.GetStringFromName("inspectorScrollNodeIntoView.accesskey"),
      hidden: !this._supportsScrollIntoView,
      disabled: !isSelectionElement,
      click: () => this.scrollNodeIntoView(),
    }));
    menu.append(new MenuItem({
      id: "node-menu-screenshotnode",
      label: strings.GetStringFromName("inspectorScreenshotNode.label"),
      disabled: !isScreenshotable,
      click: () => this.screenshotNode(),
    }));
    menu.append(new MenuItem({
      id: "node-menu-useinconsole",
      label: strings.GetStringFromName("inspectorUseInConsole.label"),
      click: () => this.useInConsole(),
    }));
    menu.append(new MenuItem({
      id: "node-menu-showdomproperties",
      label: strings.GetStringFromName("inspectorShowDOMProperties.label"),
      click: () => this.showDOMProperties(),
    }));

    let nodeLinkMenuItems = this._getNodeLinkMenuItems();
    if (nodeLinkMenuItems.filter(item => item.visible).length > 0) {
      menu.append(new MenuItem({
        id: "node-menu-link-separator",
        type: "separator",
      }));
    }

    for (let menuitem of nodeLinkMenuItems) {
      menu.append(menuitem);
    }

    menu.popup(screenX, screenY, this._toolbox);
    return menu;
  },

  _getPasteSubmenu: function (isEditableElement) {
    let isPasteable = isEditableElement && this._getClipboardContentForPaste();
    let disableAdjacentPaste = !isPasteable ||
          !this.canPasteInnerOrAdjacentHTML || this.selection.isRoot() ||
          this.selection.isBodyNode() || this.selection.isHeadNode();
    let disableFirstLastPaste = !isPasteable ||
          !this.canPasteInnerOrAdjacentHTML || (this.selection.isHTMLNode() &&
          this.selection.isRoot());

    let pasteSubmenu = new Menu();
    pasteSubmenu.append(new MenuItem({
      id: "node-menu-pasteinnerhtml",
      label: strings.GetStringFromName("inspectorPasteInnerHTML.label"),
      accesskey: strings.GetStringFromName("inspectorPasteInnerHTML.accesskey"),
      disabled: !isPasteable || !this.canPasteInnerOrAdjacentHTML,
      click: () => this.pasteInnerHTML(),
    }));
    pasteSubmenu.append(new MenuItem({
      id: "node-menu-pasteouterhtml",
      label: strings.GetStringFromName("inspectorPasteOuterHTML.label"),
      accesskey: strings.GetStringFromName("inspectorPasteOuterHTML.accesskey"),
      disabled: !isPasteable || !this.isOuterHTMLEditable,
      click: () => this.pasteOuterHTML(),
    }));
    pasteSubmenu.append(new MenuItem({
      id: "node-menu-pastebefore",
      label: strings.GetStringFromName("inspectorHTMLPasteBefore.label"),
      accesskey:
        strings.GetStringFromName("inspectorHTMLPasteBefore.accesskey"),
      disabled: disableAdjacentPaste,
      click: () => this.pasteAdjacentHTML("beforeBegin"),
    }));
    pasteSubmenu.append(new MenuItem({
      id: "node-menu-pasteafter",
      label: strings.GetStringFromName("inspectorHTMLPasteAfter.label"),
      accesskey:
        strings.GetStringFromName("inspectorHTMLPasteAfter.accesskey"),
      disabled: disableAdjacentPaste,
      click: () => this.pasteAdjacentHTML("afterEnd"),
    }));
    pasteSubmenu.append(new MenuItem({
      id: "node-menu-pastefirstchild",
      label: strings.GetStringFromName("inspectorHTMLPasteFirstChild.label"),
      accesskey:
        strings.GetStringFromName("inspectorHTMLPasteFirstChild.accesskey"),
      disabled: disableFirstLastPaste,
      click: () => this.pasteAdjacentHTML("afterBegin"),
    }));
    pasteSubmenu.append(new MenuItem({
      id: "node-menu-pastelastchild",
      label: strings.GetStringFromName("inspectorHTMLPasteLastChild.label"),
      accesskey:
        strings.GetStringFromName("inspectorHTMLPasteLastChild.accesskey"),
      disabled: disableFirstLastPaste,
      click: () => this.pasteAdjacentHTML("beforeEnd"),
    }));

    return pasteSubmenu;
  },

  _getAttributesSubmenu: function (isEditableElement) {
    let attributesSubmenu = new Menu();
    let nodeInfo = this.nodeMenuTriggerInfo;
    let isAttributeClicked = isEditableElement && nodeInfo &&
                              nodeInfo.type === "attribute";

    attributesSubmenu.append(new MenuItem({
      id: "node-menu-add-attribute",
      label: strings.GetStringFromName("inspectorAddAttribute.label"),
      accesskey: strings.GetStringFromName("inspectorAddAttribute.accesskey"),
      disabled: !isEditableElement,
      click: () => this.onAddAttribute(),
    }));
    attributesSubmenu.append(new MenuItem({
      id: "node-menu-edit-attribute",
      label: strings.formatStringFromName("inspectorEditAttribute.label",
                 [isAttributeClicked ? `"${nodeInfo.name}"` : ""], 1),
      accesskey: strings.GetStringFromName("inspectorEditAttribute.accesskey"),
      disabled: !isAttributeClicked,
      click: () => this.onEditAttribute(),
    }));

    attributesSubmenu.append(new MenuItem({
      id: "node-menu-remove-attribute",
      label: strings.formatStringFromName("inspectorRemoveAttribute.label",
                [isAttributeClicked ? `"${nodeInfo.name}"` : ""], 1),
      accesskey:
        strings.GetStringFromName("inspectorRemoveAttribute.accesskey"),
      disabled: !isAttributeClicked,
      click: () => this.onRemoveAttribute(),
    }));

    return attributesSubmenu;
  },

  /**
   * Link menu items can be shown or hidden depending on the context and
   * selected node, and their labels can vary.
   *
   * @return {Array} list of visible menu items related to links.
   */
  _getNodeLinkMenuItems: function () {
    let linkFollow = new MenuItem({
      id: "node-menu-link-follow",
      visible: false,
      click: () => this.onFollowLink(),
    });
    let linkCopy = new MenuItem({
      id: "node-menu-link-copy",
      visible: false,
      click: () => this.onCopyLink(),
    });

    // Get information about the right-clicked node.
    let popupNode = this.contextMenuTarget;
    if (!popupNode || !popupNode.classList.contains("link")) {
      return [linkFollow, linkCopy];
    }

    let type = popupNode.dataset.type;
    if (this._supportsResolveRelativeURL &&
        (type === "uri" || type === "cssresource" || type === "jsresource")) {
      // Links can't be opened in new tabs in the browser toolbox.
      if (type === "uri" && !this.target.chrome) {
        linkFollow.visible = true;
        linkFollow.label = strings.GetStringFromName(
          "inspector.menu.openUrlInNewTab.label");
      } else if (type === "cssresource") {
        linkFollow.visible = true;
        linkFollow.label = toolboxStrings.GetStringFromName(
          "toolbox.viewCssSourceInStyleEditor.label");
      } else if (type === "jsresource") {
        linkFollow.visible = true;
        linkFollow.label = toolboxStrings.GetStringFromName(
          "toolbox.viewJsSourceInDebugger.label");
      }

      linkCopy.visible = true;
      linkCopy.label = strings.GetStringFromName(
        "inspector.menu.copyUrlToClipboard.label");
    } else if (type === "idref") {
      linkFollow.visible = true;
      linkFollow.label = strings.formatStringFromName(
        "inspector.menu.selectElement.label", [popupNode.dataset.link], 1);
    }

    return [linkFollow, linkCopy];
  },

  _initMarkup: function () {
    let doc = this.panelDoc;

    this._markupBox = doc.getElementById("markup-box");

    // create tool iframe
    this._markupFrame = doc.createElement("iframe");
    this._markupFrame.setAttribute("flex", "1");
    this._markupFrame.setAttribute("tooltip", "aHTMLTooltip");
    this._markupFrame.addEventListener("contextmenu", this._onContextMenu, true);

    // This is needed to enable tooltips inside the iframe document.
    this._markupFrame.addEventListener("load", this._onMarkupFrameLoad, true);

    this._markupBox.setAttribute("collapsed", true);
    this._markupBox.appendChild(this._markupFrame);
    this._markupFrame.setAttribute("src", "chrome://devtools/content/inspector/markup/markup.xhtml");
    this._markupFrame.setAttribute("aria-label",
      strings.GetStringFromName("inspector.panelLabel.markupView"));
  },

  _onMarkupFrameLoad: function () {
    this._markupFrame.removeEventListener("load", this._onMarkupFrameLoad, true);

    this._markupFrame.contentWindow.focus();

    this._markupBox.removeAttribute("collapsed");

    this.markup = new MarkupView(this, this._markupFrame, this._toolbox.win);

    this.emit("markuploaded");
  },

  _destroyMarkup: function () {
    let destroyPromise;

    if (this._markupFrame) {
      this._markupFrame.removeEventListener("load", this._onMarkupFrameLoad, true);
      this._markupFrame.removeEventListener("contextmenu", this._onContextMenu, true);
    }

    if (this.markup) {
      destroyPromise = this.markup.destroy();
      this.markup = null;
    } else {
      destroyPromise = promise.resolve();
    }

    if (this._markupFrame) {
      this._markupFrame.parentNode.removeChild(this._markupFrame);
      this._markupFrame = null;
    }

    this._markupBox = null;

    return destroyPromise;
  },

  /**
   * When the pane toggle button is clicked or pressed, toggle the pane, change the button
   * state and tooltip.
   */
  onPaneToggleButtonClicked: function (e) {
    let sidePaneContainer = this.panelDoc.querySelector("#inspector-sidebar-container");
    let isVisible = !this._sidebarToggle.state.collapsed;

    // Make sure the sidebar has width and height attributes before collapsing
    // because ViewHelpers needs it.
    if (isVisible) {
      let rect = sidePaneContainer.getBoundingClientRect();
      if (!sidePaneContainer.hasAttribute("width")) {
        sidePaneContainer.setAttribute("width", rect.width);
      }
      // always refresh the height attribute before collapsing, it could have
      // been modified by resizing the container.
      sidePaneContainer.setAttribute("height", rect.height);
    }

    let onAnimationDone = () => {
      if (isVisible) {
        this._sidebarToggle.setState({collapsed: true});
      } else {
        this._sidebarToggle.setState({collapsed: false});
      }
    };

    ViewHelpers.togglePane({
      visible: !isVisible,
      animated: true,
      delayed: true,
      callback: onAnimationDone
    }, sidePaneContainer);
  },

  /**
   * Create a new node as the last child of the current selection, expand the
   * parent and select the new node.
   */
  addNode: Task.async(function* () {
    if (!this.canAddHTMLChild()) {
      return;
    }

    let html = "<div></div>";

    // Insert the html and expect a childList markup mutation.
    let onMutations = this.once("markupmutation");
    let {nodes} = yield this.walker.insertAdjacentHTML(this.selection.nodeFront,
                                                       "beforeEnd", html);
    yield onMutations;

    // Select the new node (this will auto-expand its parent).
    this.selection.setNodeFront(nodes[0], "node-inserted");
  }),

  /**
   * Toggle a pseudo class.
   */
  togglePseudoClass: function (pseudo) {
    if (this.selection.isElementNode()) {
      let node = this.selection.nodeFront;
      if (node.hasPseudoClassLock(pseudo)) {
        return this.walker.removePseudoClassLock(node, pseudo, {parents: true});
      }

      let hierarchical = pseudo == ":hover" || pseudo == ":active";
      return this.walker.addPseudoClassLock(node, pseudo, {parents: hierarchical});
    }
    return promise.resolve();
  },

  /**
   * Show DOM properties
   */
  showDOMProperties: function () {
    this._toolbox.openSplitConsole().then(() => {
      let panel = this._toolbox.getPanel("webconsole");
      let jsterm = panel.hud.jsterm;

      jsterm.execute("inspect($0)");
      jsterm.focus();
    });
  },

  /**
   * Use in Console.
   *
   * Takes the currently selected node in the inspector and assigns it to a
   * temp variable on the content window.  Also opens the split console and
   * autofills it with the temp variable.
   */
  useInConsole: function () {
    this._toolbox.openSplitConsole().then(() => {
      let panel = this._toolbox.getPanel("webconsole");
      let jsterm = panel.hud.jsterm;

      let evalString = `{ let i = 0;
        while (window.hasOwnProperty("temp" + i) && i < 1000) {
          i++;
        }
        window["temp" + i] = $0;
        "temp" + i;
      }`;

      let options = {
        selectedNodeActor: this.selection.nodeFront.actorID,
      };
      jsterm.requestEvaluation(evalString, options).then((res) => {
        jsterm.setInputValue(res.result);
        this.emit("console-var-ready");
      });
    });
  },

  /**
   * Clear any pseudo-class locks applied to the current hierarchy.
   */
  clearPseudoClasses: function () {
    if (!this.walker) {
      return promise.resolve();
    }
    return this.walker.clearPseudoClassLocks().then(null, console.error);
  },

  /**
   * Edit the outerHTML of the selected Node.
   */
  editHTML: function () {
    if (!this.selection.isNode()) {
      return;
    }
    if (this.markup) {
      this.markup.beginEditingOuterHTML(this.selection.nodeFront);
    }
  },

  /**
   * Paste the contents of the clipboard into the selected Node's outer HTML.
   */
  pasteOuterHTML: function () {
    let content = this._getClipboardContentForPaste();
    if (!content) {
      return promise.reject("No clipboard content for paste");
    }

    let node = this.selection.nodeFront;
    return this.markup.getNodeOuterHTML(node).then(oldContent => {
      this.markup.updateNodeOuterHTML(node, content, oldContent);
    });
  },

  /**
   * Paste the contents of the clipboard into the selected Node's inner HTML.
   */
  pasteInnerHTML: function () {
    let content = this._getClipboardContentForPaste();
    if (!content) {
      return promise.reject("No clipboard content for paste");
    }

    let node = this.selection.nodeFront;
    return this.markup.getNodeInnerHTML(node).then(oldContent => {
      this.markup.updateNodeInnerHTML(node, content, oldContent);
    });
  },

  /**
   * Paste the contents of the clipboard as adjacent HTML to the selected Node.
   * @param position The position as specified for Element.insertAdjacentHTML
   *        (i.e. "beforeBegin", "afterBegin", "beforeEnd", "afterEnd").
   */
  pasteAdjacentHTML: function (position) {
    let content = this._getClipboardContentForPaste();
    if (!content) {
      return promise.reject("No clipboard content for paste");
    }

    let node = this.selection.nodeFront;
    return this.markup.insertAdjacentHTMLToNode(node, position, content);
  },

  /**
   * Copy the innerHTML of the selected Node to the clipboard.
   */
  copyInnerHTML: function () {
    if (!this.selection.isNode()) {
      return;
    }
    this._copyLongString(this.walker.innerHTML(this.selection.nodeFront));
  },

  /**
   * Copy the outerHTML of the selected Node to the clipboard.
   */
  copyOuterHTML: function () {
    if (!this.selection.isNode()) {
      return;
    }
    let node = this.selection.nodeFront;

    switch (node.nodeType) {
      case nodeConstants.ELEMENT_NODE :
        this._copyLongString(this.walker.outerHTML(node));
        break;
      case nodeConstants.COMMENT_NODE :
        this._getLongString(node.getNodeValue()).then(comment => {
          clipboardHelper.copyString("<!--" + comment + "-->");
        });
        break;
      case nodeConstants.DOCUMENT_TYPE_NODE :
        clipboardHelper.copyString(node.doctypeString);
        break;
    }
  },

  /**
   * Copy the data-uri for the currently selected image in the clipboard.
   */
  copyImageDataUri: function () {
    let container = this.markup.getContainer(this.selection.nodeFront);
    if (container && container.isPreviewable()) {
      container.copyImageDataUri();
    }
  },

  /**
   * Copy the content of a longString (via a promise resolving a LongStringActor) to the clipboard
   * @param  {Promise} longStringActorPromise promise expected to resolve a LongStringActor instance
   * @return {Promise} promise resolving (with no argument) when the string is sent to the clipboard
   */
  _copyLongString: function (longStringActorPromise) {
    return this._getLongString(longStringActorPromise).then(string => {
      clipboardHelper.copyString(string);
    }).catch(e => console.error(e));
  },

  /**
   * Retrieve the content of a longString (via a promise resolving a LongStringActor)
   * @param  {Promise} longStringActorPromise promise expected to resolve a LongStringActor instance
   * @return {Promise} promise resolving with the retrieved string as argument
   */
  _getLongString: function (longStringActorPromise) {
    return longStringActorPromise.then(longStringActor => {
      return longStringActor.string().then(string => {
        longStringActor.release().catch(e => console.error(e));
        return string;
      });
    }).catch(e => console.error(e));
  },

  /**
   * Copy a unique selector of the selected Node to the clipboard.
   */
  copyUniqueSelector: function () {
    if (!this.selection.isNode()) {
      return;
    }

    this.selection.nodeFront.getUniqueSelector().then((selector) => {
      clipboardHelper.copyString(selector);
    }).then(null, console.error);
  },

  /**
   * Initiate gcli screenshot command on selected node
   */
  screenshotNode: function () {
    CommandUtils.createRequisition(this._target, {
      environment: CommandUtils.createEnvironment(this, "_target")
    }).then(requisition => {
      // Bug 1180314 -  CssSelector might contain white space so need to make sure it is
      // passed to screenshot as a single parameter.  More work *might* be needed if
      // CssSelector could contain escaped single- or double-quotes, backslashes, etc.
      requisition.updateExec("screenshot --selector '" + this.selectionCssSelector + "'");
    });
  },

  /**
   * Scroll the node into view.
   */
  scrollNodeIntoView: function () {
    if (!this.selection.isNode()) {
      return;
    }

    this.selection.nodeFront.scrollIntoView();
  },

  /**
   * Duplicate the selected node
   */
  duplicateNode: function () {
    let selection = this.selection;
    if (!selection.isElementNode() ||
        selection.isRoot() ||
        selection.isAnonymousNode() ||
        selection.isPseudoElementNode()) {
      return;
    }
    this.walker.duplicateNode(selection.nodeFront).catch(e => console.error(e));
  },

  /**
   * Delete the selected node.
   */
  deleteNode: function () {
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
   * Add attribute to node.
   * Used for node context menu and shouldn't be called directly.
   */
  onAddAttribute: function () {
    let container = this.markup.getContainer(this.selection.nodeFront);
    container.addAttribute();
  },

  /**
   * Edit attribute for node.
   * Used for node context menu and shouldn't be called directly.
   */
  onEditAttribute: function () {
    let container = this.markup.getContainer(this.selection.nodeFront);
    container.editAttribute(this.nodeMenuTriggerInfo.name);
  },

  /**
   * Remove attribute from node.
   * Used for node context menu and shouldn't be called directly.
   */
  onRemoveAttribute: function () {
    let container = this.markup.getContainer(this.selection.nodeFront);
    container.removeAttribute(this.nodeMenuTriggerInfo.name);
  },

  expandNode: function () {
    this.markup.expandAll(this.selection.nodeFront);
  },

  collapseNode: function () {
    this.markup.collapseNode(this.selection.nodeFront);
  },

  /**
   * This method is here for the benefit of the node-menu-link-follow menu item
   * in the inspector contextual-menu.
   */
  onFollowLink: function () {
    let type = this.contextMenuTarget.dataset.type;
    let link = this.contextMenuTarget.dataset.link;

    this.followAttributeLink(type, link);
  },

  /**
   * Given a type and link found in a node's attribute in the markup-view,
   * attempt to follow that link (which may result in opening a new tab, the
   * style editor or debugger).
   */
  followAttributeLink: function (type, link) {
    if (!type || !link) {
      return;
    }

    if (type === "uri" || type === "cssresource" || type === "jsresource") {
      // Open link in a new tab.
      // When the inspector menu was setup on click (see _getNodeLinkMenuItems), we
      // already checked that resolveRelativeURL existed.
      this.inspector.resolveRelativeURL(
        link, this.selection.nodeFront).then(url => {
          if (type === "uri") {
            let browserWin = this.target.tab.ownerDocument.defaultView;
            browserWin.openUILinkIn(url, "tab");
          } else if (type === "cssresource") {
            return this.toolbox.viewSourceInStyleEditor(url);
          } else if (type === "jsresource") {
            return this.toolbox.viewSourceInDebugger(url);
          }
          return null;
        }).catch(e => console.error(e));
    } else if (type == "idref") {
      // Select the node in the same document.
      this.walker.document(this.selection.nodeFront).then(doc => {
        return this.walker.querySelector(doc, "#" + CSS.escape(link)).then(node => {
          if (!node) {
            this.emit("idref-attribute-link-failed");
            return;
          }
          this.selection.setNodeFront(node);
        });
      }).catch(e => console.error(e));
    }
  },

  /**
   * This method is here for the benefit of the node-menu-link-copy menu item
   * in the inspector contextual-menu.
   */
  onCopyLink: function () {
    let link = this.contextMenuTarget.dataset.link;

    this.copyAttributeLink(link);
  },

  /**
   * This method is here for the benefit of copying links.
   */
  copyAttributeLink: function (link) {
    // When the inspector menu was setup on click (see _getNodeLinkMenuItems), we
    // already checked that resolveRelativeURL existed.
    this.inspector.resolveRelativeURL(link, this.selection.nodeFront).then(url => {
      clipboardHelper.copyString(url);
    }, console.error);
  }
};
