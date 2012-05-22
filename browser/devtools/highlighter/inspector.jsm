/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Cu = Components.utils;
const Ci = Components.interfaces;
const Cr = Components.results;

var EXPORTED_SYMBOLS = ["InspectorUI"];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/TreePanel.jsm");
Cu.import("resource:///modules/highlighter.jsm");
Cu.import("resource:///modules/devtools/LayoutView.jsm");
Cu.import("resource:///modules/devtools/LayoutHelpers.jsm");

// Inspector notifications dispatched through the nsIObserverService.
const INSPECTOR_NOTIFICATIONS = {
  // Fires once the Inspector completes the initialization and opens up on
  // screen.
  OPENED: "inspector-opened",

  // Fires once the Inspector is closed.
  CLOSED: "inspector-closed",

  // Fires once the Inspector is destroyed. Not fired on tab switch.
  DESTROYED: "inspector-destroyed",

  // Fires when the Inspector is reopened after tab-switch.
  STATE_RESTORED: "inspector-state-restored",

  // Fires when the Tree Panel is opened and initialized.
  TREEPANELREADY: "inspector-treepanel-ready",

  // Event notifications for the attribute-value editor
  EDITOR_OPENED: "inspector-editor-opened",
  EDITOR_CLOSED: "inspector-editor-closed",
  EDITOR_SAVED: "inspector-editor-saved",
};

const PSEUDO_CLASSES = [":hover", ":active", ":focus"];

/**
 * Represents an open instance of the Inspector for a tab.
 * This is the object handed out to sidebars and other API consumers.
 *
 * Right now it's a thin layer over InspectorUI, but we will
 * start moving per-tab state into this object soon, eventually
 * replacing the per-winID InspectorStore objects.
 *
 * The lifetime of this object is also not yet correct.  This object
 * is currently destroyed when the inspector is torn down, either by user
 * closing the inspector or by user switching the tab.  This should
 * only be destroyed when user closes the inspector.
 */
function Inspector(aIUI)
{
  this._IUI = aIUI;
  this._winID = aIUI.winID;
  this._listeners = {};
}

Inspector.prototype = {
  /**
   * True if the highlighter is locked on a node.
   */
  get locked() {
    return !this._IUI.inspecting;
  },

  /**
   * The currently selected node in the highlighter.
   */
  get selection() {
    return this._IUI.selection;
  },

  /**
   * Indicate that a tool has modified the state of the page.  Used to
   * decide whether to show the "are you sure you want to navigate"
   * notification.
   */
  markDirty: function Inspector_markDirty()
  {
    this._IUI.isDirty = true;
  },

  /**
   * The chrome window the inspector lives in.
   */
  get chromeWindow() {
    return this._IUI.chromeWin;
  },

  /**
   * Notify the inspector that the current selection has changed.
   *
   * @param string aContext
   *        An string that will be passed to the change event.  Allows
   *        a tool to recognize when it sent a change notification itself
   *        to avoid unnecessary refresh.
   */
  change: function Inspector_change(aContext)
  {
    this._IUI.nodeChanged(aContext);
  },

  /**
   * Called by the InspectorUI when the inspector is being destroyed.
   */
  _destroy: function Inspector__destroy()
  {
    delete this._IUI;
    delete this._listeners;
  },

  /// Event stuff.  Would like to refactor this eventually.
  /// Emulates the jetpack event source, which has a nice API.

  /**
   * Connect a listener to this object.
   *
   * @param string aEvent
   *        The event name to which we're connecting.
   * @param function aListener
   *        Called when the event is fired.
   */
  on: function Inspector_on(aEvent, aListener)
  {
    if (!(aEvent in this._listeners)) {
      this._listeners[aEvent] = [];
    }
    this._listeners[aEvent].push(aListener);
  },

  /**
   * Listen for the next time an event is fired.
   *
   * @param string aEvent
   *        The event name to which we're connecting.
   * @param function aListener
   *        Called when the event is fired.  Will be called at most one time.
   */
  once: function Inspector_once(aEvent, aListener)
  {
    let handler = function() {
      this.removeListener(aEvent, handler);
      aListener();
    }.bind(this);
    this.on(aEvent, handler);
  },

  /**
   * Remove a previously-registered event listener.  Works for events
   * registered with either on or once.
   *
   * @param string aEvent
   *        The event name whose listener we're disconnecting.
   * @param function aListener
   *        The listener to remove.
   */
  removeListener: function Inspector_removeListener(aEvent, aListener)
  {
    this._listeners[aEvent] = this._listeners[aEvent].filter(function(l) aListener != l);
  },

  /**
   * Emit an event on the inspector.  All arguments to this method will
   * be sent to listner functions.
   */
  _emit: function Inspector__emit(aEvent)
  {
    if (!(aEvent in this._listeners))
      return;
    for each (let listener in this._listeners[aEvent]) {
      listener.apply(null, arguments);
    }
  }
}

///////////////////////////////////////////////////////////////////////////
//// InspectorUI

/**
 * Main controller class for the Inspector.
 *
 * @constructor
 * @param nsIDOMWindow aWindow
 *        The chrome window for which the Inspector instance is created.
 */
function InspectorUI(aWindow)
{
  // Let style inspector tools register themselves.
  let tmp = {};
  Cu.import("resource:///modules/devtools/StyleInspector.jsm", tmp);

  this.chromeWin = aWindow;
  this.chromeDoc = aWindow.document;
  this.tabbrowser = aWindow.gBrowser;
  this.tools = {};
  this.toolEvents = {};
  this.store = new InspectorStore();
  this.INSPECTOR_NOTIFICATIONS = INSPECTOR_NOTIFICATIONS;
  this.buildButtonsTooltip();
}

InspectorUI.prototype = {
  browser: null,
  tools: null,
  toolEvents: null,
  inspecting: false,
  ruleViewEnabled: true,
  isDirty: false,
  store: null,

  _currentInspector: null,
  _sidebar: null,

  /**
   * The Inspector object for the current tab.
   */
  get currentInspector() this._currentInspector,

  /**
   * The InspectorStyleSidebar for the current tab.
   */
  get sidebar() this._sidebar,

  /**
   * Toggle the inspector interface elements on or off.
   *
   * @param aEvent
   *        The event that requested the UI change. Toolbar button or menu.
   */
  toggleInspectorUI: function IUI_toggleInspectorUI(aEvent)
  {
    if (this.isInspectorOpen) {
      this.closeInspectorUI();
    } else {
      this.openInspectorUI();
    }
  },

  /**
   * Add a tooltip to the Inspect and Markup buttons.
   * The tooltips include the related keyboard shortcut.
   */
  buildButtonsTooltip: function IUI_buildButtonsTooltip()
  {
    let keysbundle = Services.strings.createBundle("chrome://global-platform/locale/platformKeys.properties");
    let separator = keysbundle.GetStringFromName("MODIFIER_SEPARATOR");

    let button, tooltip;

    // Inspect Button - the shortcut string is built from the <key> element

    let key = this.chromeDoc.getElementById("key_inspect");

    if (key) {
      let modifiersAttr = key.getAttribute("modifiers");

      let combo = [];

      if (modifiersAttr.match("accel"))
#ifdef XP_MACOSX
        combo.push(keysbundle.GetStringFromName("VK_META"));
#else
        combo.push(keysbundle.GetStringFromName("VK_CONTROL"));
#endif
      if (modifiersAttr.match("shift"))
        combo.push(keysbundle.GetStringFromName("VK_SHIFT"));
      if (modifiersAttr.match("alt"))
        combo.push(keysbundle.GetStringFromName("VK_ALT"));
      if (modifiersAttr.match("ctrl"))
        combo.push(keysbundle.GetStringFromName("VK_CONTROL"));
      if (modifiersAttr.match("meta"))
        combo.push(keysbundle.GetStringFromName("VK_META"));

      combo.push(key.getAttribute("key"));

      tooltip = this.strings.formatStringFromName("inspectButtonWithShortcutKey.tooltip",
        [combo.join(separator)], 1);
    } else {
      tooltip = this.strings.GetStringFromName("inspectButton.tooltip");
    }

    button = this.chromeDoc.getElementById("inspector-inspect-toolbutton");
    button.setAttribute("tooltiptext", tooltip);

    // Markup Button - the shortcut string is built from the accesskey attribute

    button = this.chromeDoc.getElementById("inspector-treepanel-toolbutton");
#ifdef XP_MACOSX
    // On Mac, no accesskey
    tooltip = this.strings.GetStringFromName("markupButton.tooltip");
#else
    let altString = keysbundle.GetStringFromName("VK_ALT");
    let accesskey = button.getAttribute("accesskey");
    let shortcut = altString + separator + accesskey;
    tooltip = this.strings.formatStringFromName("markupButton.tooltipWithAccesskey",
      [shortcut], 1);
#endif
    button.setAttribute("tooltiptext", tooltip);

  },

  /**
   * Toggle the status of the inspector, starting or stopping it. Invoked
   * from the toolbar's Inspect button.
   */
  toggleInspection: function IUI_toggleInspection()
  {
    if (!this.isInspectorOpen) {
      this.openInspectorUI();
      return;
    }

    if (this.inspecting) {
      this.stopInspecting();
    } else {
      this.startInspecting();
    }
  },

  /**
   * Show or hide the sidebar. Called from the Styling button on the
   * highlighter toolbar.
   */
  toggleSidebar: function IUI_toggleSidebar()
  {
    if (!this.sidebar.visible) {
      this.sidebar.show();
    } else {
      this.sidebar.hide();
    }
  },

  /**
   * Toggle the TreePanel.
   */
  toggleHTMLPanel: function TP_toggleHTMLPanel()
  {
    if (this.treePanel.isOpen()) {
      this.treePanel.close();
      Services.prefs.setBoolPref("devtools.inspector.htmlPanelOpen", false);
      this.currentInspector._htmlPanelOpen = false;
    } else {
      this.treePanel.open();
      Services.prefs.setBoolPref("devtools.inspector.htmlPanelOpen", true);
      this.currentInspector._htmlPanelOpen = true;
    }
  },

  /**
   * Is the inspector UI open? Simply check if the toolbar is visible or not.
   *
   * @returns boolean
   */
  get isInspectorOpen()
  {
    return !!(this.toolbar && !this.toolbar.hidden && this.highlighter);
  },

  /**
   * Toggle highlighter veil.
   */
  toggleVeil: function IUI_toggleVeil()
  {
    if (this.currentInspector._highlighterShowVeil) {
      this.highlighter.hideVeil();
      this.currentInspector._highlighterShowVeil = false;
      Services.prefs.setBoolPref("devtools.inspector.highlighterShowVeil", false);
    } else {
      this.highlighter.showVeil();
      this.currentInspector._highlighterShowVeil = true;
      Services.prefs.setBoolPref("devtools.inspector.highlighterShowVeil", true);
    }
  },

  /**
   * Toggle highlighter infobar.
   */
  toggleInfobar: function IUI_toggleInfobar()
  {
    if (this.currentInspector._highlighterShowInfobar) {
      this.highlighter.hideInfobar();
      this.currentInspector._highlighterShowInfobar = false;
      Services.prefs.setBoolPref("devtools.inspector.highlighterShowInfobar", false);
    } else {
      this.highlighter.showInfobar();
      this.currentInspector._highlighterShowInfobar = true;
      Services.prefs.setBoolPref("devtools.inspector.highlighterShowInfobar", true);
    }
  },

  /**
   * Return the default selection element for the inspected document.
   */
  get defaultSelection()
  {
    let doc = this.win.document;
    return doc.documentElement ? doc.documentElement.lastElementChild : null;
  },

  /**
   * Open inspector UI and HTML tree. Add listeners for document scrolling,
   * resize, tabContainer.TabSelect and others. If a node is provided, then
   * start inspecting it.
   *
   * @param [optional] aNode
   *        The node to inspect.
   */
  openInspectorUI: function IUI_openInspectorUI(aNode)
  {
    // InspectorUI is already up and running. Lock a node if asked (via context).
    if (this.isInspectorOpen) {
      if (aNode) {
        this.inspectNode(aNode);
        this.stopInspecting();
      }
      return;
    }

    // Observer used to inspect the specified element from content after the
    // inspector UI has been opened (via the content context menu).
    function inspectObserver(aElement) {
      Services.obs.removeObserver(boundInspectObserver,
                                  INSPECTOR_NOTIFICATIONS.OPENED,
                                  false);
      this.inspectNode(aElement);
      this.stopInspecting();
    };

    var boundInspectObserver = inspectObserver.bind(this, aNode);

    if (aNode) {
      // Add the observer to inspect the node after initialization finishes.
      Services.obs.addObserver(boundInspectObserver,
                               INSPECTOR_NOTIFICATIONS.OPENED,
                               false);
    }
    // Start initialization.
    this.browser = this.tabbrowser.selectedBrowser;
    this.win = this.browser.contentWindow;
    this.winID = this.getWindowID(this.win);
    this.toolbar = this.chromeDoc.getElementById("inspector-toolbar");
    this.inspectMenuitem = this.chromeDoc.getElementById("Tools:Inspect");
    this.inspectToolbutton =
      this.chromeDoc.getElementById("inspector-inspect-toolbutton");

    this.chromeWin.Tilt.setup();

    this.treePanel = new TreePanel(this.chromeWin, this);
    this.toolbar.hidden = false;
    this.inspectMenuitem.setAttribute("checked", true);

    // initialize the HTML Breadcrumbs
    this.breadcrumbs = new HTMLBreadcrumbs(this);

    this.isDirty = false;

    this.progressListener = new InspectorProgressListener(this);

    this.chromeWin.addEventListener("keypress", this, false);

    // initialize the highlighter
    this.highlighter = new Highlighter(this.chromeWin);

    this.initializeStore();

    this._sidebar = new InspectorStyleSidebar({
      document: this.chromeDoc,
      inspector: this._currentInspector,
    });

    // Create UI for any sidebars registered with
    // InspectorUI.registerSidebar()
    for each (let tool in InspectorUI._registeredSidebars) {
      this._sidebar.addTool(tool);
    }

    this.setupNavigationKeys();
    this.highlighterReady();

    // Focus the first focusable element in the toolbar
    this.chromeDoc.commandDispatcher.advanceFocusIntoSubtree(this.toolbar);

    // If nothing is focused in the toolbar, it means that the focus manager
    // is limited to some specific elements and has moved the focus somewhere else.
    // So in this case, we want to focus the content window.
    // See: https://developer.mozilla.org/en/XUL_Tutorial/Focus_and_Selection#Platform_Specific_Behaviors
    if (!this.toolbar.querySelector(":-moz-focusring")) {
      this.win.focus();
    }

  },

  /**
   * Initialize the InspectorStore.
   */
  initializeStore: function IUI_initializeStore()
  {
    // First time opened, add the TabSelect listener
    if (this.store.isEmpty()) {
      this.tabbrowser.tabContainer.addEventListener("TabSelect", this, false);
    }

    // Has this windowID been inspected before?
    if (this.store.hasID(this.winID)) {
      this._currentInspector = this.store.getInspector(this.winID);
      let selectedNode = this.currentInspector._selectedNode;
      if (selectedNode) {
        this.inspectNode(selectedNode);
      }
      this.isDirty = this.currentInspector._isDirty;
    } else {
      // First time inspecting, set state to no selection + live inspection.
      let inspector = new Inspector(this);
      this.store.addInspector(this.winID, inspector);
      inspector._selectedNode = null;
      inspector._inspecting = true;
      inspector._isDirty = this.isDirty;

      inspector._htmlPanelOpen =
        Services.prefs.getBoolPref("devtools.inspector.htmlPanelOpen");

      inspector._sidebarOpen =
        Services.prefs.getBoolPref("devtools.inspector.sidebarOpen");

      inspector._activeSidebar =
        Services.prefs.getCharPref("devtools.inspector.activeSidebar");

      inspector._highlighterShowVeil =
        Services.prefs.getBoolPref("devtools.inspector.highlighterShowVeil");

      inspector._highlighterShowInfobar =
        Services.prefs.getBoolPref("devtools.inspector.highlighterShowInfobar");

      this.win.addEventListener("pagehide", this, true);

      this._currentInspector = inspector;
    }
  },

  /**
   * Browse nodes according to the breadcrumbs layout, only for some specific
   * elements of the UI.
   */
   setupNavigationKeys: function IUI_setupNavigationKeys()
   {
     // UI elements that are arrow keys sensitive:
     // - highlighter veil;
     // - content window (when the highlighter `veil is pointer-events:none`;
     // - the Inspector toolbar.

     this.onKeypress = this.onKeypress.bind(this);

     this.highlighter.highlighterContainer.addEventListener("keypress",
       this.onKeypress, true);
     this.win.addEventListener("keypress", this.onKeypress, true);
     this.toolbar.addEventListener("keypress", this.onKeypress, true);
   },

  /**
   * Remove the event listeners for the arrowkeys.
   */
   removeNavigationKeys: function IUI_removeNavigationKeys()
   {
      this.highlighter.highlighterContainer.removeEventListener("keypress",
        this.onKeypress, true);
      this.win.removeEventListener("keypress", this.onKeypress, true);
      this.toolbar.removeEventListener("keypress", this.onKeypress, true);
   },

  /**
   * Close inspector UI and associated panels. Unhighlight and stop inspecting.
   * Remove event listeners for document scrolling, resize,
   * tabContainer.TabSelect and others.
   *
   * @param boolean aKeepInspector
   *        Tells if you want the inspector associated to the current tab/window to
   *        be cleared or not. Set this to true to save the inspector, or false
   *        to destroy it.
   */
  closeInspectorUI: function IUI_closeInspectorUI(aKeepInspector)
  {
    // if currently editing an attribute value, closing the
    // highlighter/HTML panel dismisses the editor
    if (this.treePanel && this.treePanel.editingContext)
      this.treePanel.closeEditor();

    this.treePanel.destroy();

    if (this.closing || !this.win || !this.browser) {
      return;
    }

    let winId = new String(this.winID); // retain this to notify observers.

    this.closing = true;
    this.toolbar.hidden = true;

    this.removeNavigationKeys();

    this.progressListener.destroy();
    delete this.progressListener;

    if (!aKeepInspector) {
      this.win.removeEventListener("pagehide", this, true);
      this.clearPseudoClassLocks();
    } else {
      // Update the inspector before closing.
      if (this.selection) {
        this.currentInspector._selectedNode = this.selection;
      }
      this.currentInspector._inspecting = this.inspecting;
      this.currentInspector._isDirty = this.isDirty;
    }

    if (this.store.isEmpty()) {
      this.tabbrowser.tabContainer.removeEventListener("TabSelect", this, false);
    }

    this.chromeWin.removeEventListener("keypress", this, false);

    this.stopInspecting();

    // close the sidebar
    if (this._sidebar) {
      this._sidebar.destroy();
      this._sidebar = null;
    }

    if (this.highlighter) {
      this.highlighter.destroy();
      this.highlighter = null;
    }

    if (this.breadcrumbs) {
      this.breadcrumbs.destroy();
      this.breadcrumbs = null;
    }

    delete this._currentInspector;
    if (!aKeepInspector)
      this.store.deleteInspector(this.winID);

    this.inspectMenuitem.removeAttribute("checked");
    this.browser = this.win = null; // null out references to browser and window
    this.winID = null;
    this.selection = null;
    this.closing = false;
    this.isDirty = false;

    delete this.treePanel;
    delete this.stylePanel;
    delete this.toolbar;

    Services.obs.notifyObservers(null, INSPECTOR_NOTIFICATIONS.CLOSED, null);

    if (!aKeepInspector)
      Services.obs.notifyObservers(null, INSPECTOR_NOTIFICATIONS.DESTROYED, winId);
  },

  /**
   * Begin inspecting webpage, attach page event listeners, activate
   * highlighter event listeners.
   */
  startInspecting: function IUI_startInspecting()
  {
    // if currently editing an attribute value, starting
    // "live inspection" mode closes the editor
    if (this.treePanel && this.treePanel.editingContext)
      this.treePanel.closeEditor();

    this.inspectToolbutton.checked = true;

    this.inspecting = true;
    this.highlighter.unlock();
    this._notifySelected();
    this._currentInspector._emit("unlocked");
  },

  _notifySelected: function IUI__notifySelected(aFrom)
  {
    this._currentInspector._emit("select", aFrom);
  },

  /**
   * Stop inspecting webpage, detach page listeners, disable highlighter
   * event listeners.
   * @param aPreventScroll
   *        Prevent scroll in the HTML tree?
   */
  stopInspecting: function IUI_stopInspecting(aPreventScroll)
  {
    if (!this.inspecting) {
      return;
    }

    this.inspectToolbutton.checked = false;

    this.inspecting = false;
    if (this.highlighter.getNode()) {
      this.select(this.highlighter.getNode(), true, !aPreventScroll);
    } else {
      this.select(null, true, true);
    }

    this.highlighter.lock();
    this._notifySelected();
    this._currentInspector._emit("locked");
  },

  /**
   * Select an object in the inspector.
   * @param aNode
   *        node to inspect
   * @param forceUpdate
   *        force an update?
   * @param aScroll boolean
   *        scroll the tree panel?
   * @param aFrom [optional] string
   *        which part of the UI the selection occured from
   */
  select: function IUI_select(aNode, forceUpdate, aScroll, aFrom)
  {
    // if currently editing an attribute value, using the
    // highlighter dismisses the editor
    if (this.treePanel && this.treePanel.editingContext)
      this.treePanel.closeEditor();

    if (!aNode)
      aNode = this.defaultSelection;

    if (forceUpdate || aNode != this.selection) {
      if (aFrom != "breadcrumbs") {
        this.clearPseudoClassLocks();
      }

      this.selection = aNode;
      if (!this.inspecting) {
        this.highlighter.highlight(this.selection);
      }
    }

    this.breadcrumbs.update();
    this.chromeWin.Tilt.update(aNode);
    this.treePanel.select(aNode, aScroll);

    this._notifySelected(aFrom);
  },

  /**
   * Toggle the pseudo-class lock on the currently inspected element. If the
   * pseudo-class is :hover or :active, that pseudo-class will also be toggled
   * on every ancestor of the element, mirroring real :hover and :active
   * behavior.
   * 
   * @param aPseudo the pseudo-class lock to toggle, e.g. ":hover"
   */
  togglePseudoClassLock: function IUI_togglePseudoClassLock(aPseudo)
  {
    if (DOMUtils.hasPseudoClassLock(this.selection, aPseudo)) {
      this.breadcrumbs.nodeHierarchy.forEach(function(crumb) {
        DOMUtils.removePseudoClassLock(crumb.node, aPseudo);
      });
    } else {
      let hierarchical = aPseudo == ":hover" || aPseudo == ":active";
      let node = this.selection;
      do {
        DOMUtils.addPseudoClassLock(node, aPseudo);
        node = node.parentNode;
      } while (hierarchical && node.parentNode)
    }
    this.nodeChanged("pseudoclass");
  },

  /**
   * Clear all pseudo-class locks applied to elements in the node hierarchy
   */
  clearPseudoClassLocks: function IUI_clearPseudoClassLocks()
  {
    this.breadcrumbs.nodeHierarchy.forEach(function(crumb) {
      DOMUtils.clearPseudoClassLocks(crumb.node);
    });
  },

  /**
   * Called when the highlighted node is changed by a tool.
   *
   * @param object aUpdater
   *        The tool that triggered the update (if any), that tool's
   *        onChanged will not be called.
   */
  nodeChanged: function IUI_nodeChanged(aUpdater)
  {
    this.highlighter.invalidateSize();
    this.breadcrumbs.updateSelectors();
    this._currentInspector._emit("change", aUpdater);
  },

  /////////////////////////////////////////////////////////////////////////
  //// Event Handling

  highlighterReady: function IUI_highlighterReady()
  {
    let self = this;

    this.highlighter.addListener("locked", function() {
      self.stopInspecting();
    });

    this.highlighter.addListener("unlocked", function() {
      self.startInspecting();
    });

    this.highlighter.addListener("nodeselected", function() {
      self.select(self.highlighter.getNode(), false, false);
    });

    this.highlighter.addListener("pseudoclasstoggled", function(aPseudo) {
      self.togglePseudoClassLock(aPseudo);
    });

    if (this.currentInspector._inspecting) {
      this.startInspecting();
      this.highlighter.unlock();
    } else {
      this.highlighter.lock();
    }

    Services.obs.notifyObservers(null, INSPECTOR_NOTIFICATIONS.STATE_RESTORED, null);

    this.highlighter.highlight();

    if (this.currentInspector._htmlPanelOpen) {
      this.treePanel.open();
    }

    if (this.currentInspector._sidebarOpen) {
      this._sidebar.show();
    }

    let menu = this.chromeDoc.getElementById("inspectorToggleVeil");
    if (this.currentInspector._highlighterShowVeil) {
      menu.setAttribute("checked", "true");
    } else {
      menu.removeAttribute("checked");
      this.highlighter.hideVeil();
    }

    menu = this.chromeDoc.getElementById("inspectorToggleInfobar");
    if (this.currentInspector._highlighterShowInfobar) {
      menu.setAttribute("checked", "true");
      this.highlighter.showInfobar();
    } else {
      menu.removeAttribute("checked");
      this.highlighter.hideInfobar();
    }

    Services.obs.notifyObservers({wrappedJSObject: this},
                                 INSPECTOR_NOTIFICATIONS.OPENED, null);
  },

  /**
   * Main callback handler for events.
   *
   * @param event
   *        The event to be handled.
   */
  handleEvent: function IUI_handleEvent(event)
  {
    let winID = null;
    let win = null;
    let inspectorClosed = false;

    switch (event.type) {
      case "TabSelect":
        winID = this.getWindowID(this.tabbrowser.selectedBrowser.contentWindow);
        if (this.isInspectorOpen && winID != this.winID) {
          this.closeInspectorUI(true);
          inspectorClosed = true;
        }

        if (winID && this.store.hasID(winID)) {
          if (inspectorClosed && this.closing) {
            Services.obs.addObserver(function reopenInspectorForTab() {
              Services.obs.removeObserver(reopenInspectorForTab,
                INSPECTOR_NOTIFICATIONS.CLOSED, false);

              this.openInspectorUI();
            }.bind(this), INSPECTOR_NOTIFICATIONS.CLOSED, false);
          } else {
            this.openInspectorUI();
          }
        }

        if (this.store.isEmpty()) {
          this.tabbrowser.tabContainer.removeEventListener("TabSelect", this,
                                                         false);
        }
        break;
      case "keypress":
        switch (event.keyCode) {
          case this.chromeWin.KeyEvent.DOM_VK_ESCAPE:
            this.closeInspectorUI(false);
            event.preventDefault();
            event.stopPropagation();
            break;
      }
      case "pagehide":
        win = event.originalTarget.defaultView;
        // Skip iframes/frames.
        if (!win || win.frameElement || win.top != win) {
          break;
        }

        win.removeEventListener(event.type, this, true);

        winID = this.getWindowID(win);
        if (winID && winID != this.winID) {
          this.store.deleteInspector(winID);
        }

        if (this.store.isEmpty()) {
          this.tabbrowser.tabContainer.removeEventListener("TabSelect", this,
                                                         false);
        }
        break;
    }
  },

  /*
   * handles "keypress" events.
  */
  onKeypress: function IUI_onKeypress(event)
  {
    let node = null;
    let bc = this.breadcrumbs;
    switch (event.keyCode) {
      case this.chromeWin.KeyEvent.DOM_VK_LEFT:
        if (bc.currentIndex != 0)
          node = bc.nodeHierarchy[bc.currentIndex - 1].node;
        if (node && this.highlighter.isNodeHighlightable(node))
          this.highlighter.highlight(node);
        event.preventDefault();
        event.stopPropagation();
        break;
      case this.chromeWin.KeyEvent.DOM_VK_RIGHT:
        if (bc.currentIndex < bc.nodeHierarchy.length - 1)
          node = bc.nodeHierarchy[bc.currentIndex + 1].node;
        if (node && this.highlighter.isNodeHighlightable(node)) {
          this.highlighter.highlight(node);
        }
        event.preventDefault();
        event.stopPropagation();
        break;
      case this.chromeWin.KeyEvent.DOM_VK_UP:
        if (this.selection) {
          // Find a previous sibling that is highlightable.
          node = this.selection.previousSibling;
          while (node && !this.highlighter.isNodeHighlightable(node)) {
            node = node.previousSibling;
          }
        }
        if (node && this.highlighter.isNodeHighlightable(node)) {
          this.highlighter.highlight(node, true);
        }
        event.preventDefault();
        event.stopPropagation();
        break;
      case this.chromeWin.KeyEvent.DOM_VK_DOWN:
        if (this.selection) {
          // Find a next sibling that is highlightable.
          node = this.selection.nextSibling;
          while (node && !this.highlighter.isNodeHighlightable(node)) {
            node = node.nextSibling;
          }
        }
        if (node && this.highlighter.isNodeHighlightable(node)) {
          this.highlighter.highlight(node, true);
        }
        event.preventDefault();
        event.stopPropagation();
        break;
    }
  },

  /**
   * Copy the innerHTML of the selected Node to the clipboard. Called via the
   * Inspector:CopyInner command.
   */
  copyInnerHTML: function IUI_copyInnerHTML()
  {
    clipboardHelper.copyString(this.selection.innerHTML);
  },

  /**
   * Copy the outerHTML of the selected Node to the clipboard. Called via the
   * Inspector:CopyOuter command.
   */
  copyOuterHTML: function IUI_copyOuterHTML()
  {
    clipboardHelper.copyString(this.selection.outerHTML);
  },

  /**
   * Delete the selected node. Called via the Inspector:DeleteNode command.
   */
  deleteNode: function IUI_deleteNode()
  {
    let selection = this.selection;
    let parent = this.selection.parentNode;

    // remove the node from the treepanel
    if (this.treePanel.isOpen())
      this.treePanel.deleteChildBox(selection);

    // remove the node from content
    parent.removeChild(selection);
    this.breadcrumbs.invalidateHierarchy();

    // select the parent node in the highlighter, treepanel, breadcrumbs
    this.inspectNode(parent);
  },

  /////////////////////////////////////////////////////////////////////////
  //// Utility Methods

  /**
   * inspect the given node, highlighting it on the page and selecting the
   * correct row in the tree panel
   *
   * @param aNode
   *        the element in the document to inspect
   * @param aScroll
   *        force scroll?
   */
  inspectNode: function IUI_inspectNode(aNode, aScroll)
  {
    this.select(aNode, true, true);
    this.highlighter.highlight(aNode, aScroll);
  },

  ///////////////////////////////////////////////////////////////////////////
  //// Utility functions

  /**
   * Retrieve the unique ID of a window object.
   *
   * @param nsIDOMWindow aWindow
   * @returns integer ID
   */
  getWindowID: function IUI_getWindowID(aWindow)
  {
    if (!aWindow) {
      return null;
    }

    let util = {};

    try {
      util = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).
        getInterface(Ci.nsIDOMWindowUtils);
    } catch (ex) { }

    return util.currentInnerWindowID;
  },

  /**
   * @param msg
   *        text message to send to the log
   */
  _log: function LOG(msg)
  {
    Services.console.logStringMessage(msg);
  },

  /**
   * Debugging function.
   * @param msg
   *        text to show with the stack trace.
   */
  _trace: function TRACE(msg)
  {
    this._log("TRACE: " + msg);
    let frame = Components.stack.caller;
    while (frame = frame.caller) {
      if (frame.language == Ci.nsIProgrammingLanguage.JAVASCRIPT ||
          frame.language == Ci.nsIProgrammingLanguage.JAVASCRIPT2) {
        this._log("filename: " + frame.filename + " lineNumber: " + frame.lineNumber +
          " functionName: " + frame.name);
      }
    }
    this._log("END TRACE");
  },

  /**
   * Get the toolbar button name for a given id string. Used by the
   * registerTools API to retrieve a consistent name for toolbar buttons
   * based on the ID of the tool.
   * @param anId String
   *        id of the tool to be buttonized
   * @returns String
   */
  getToolbarButtonId: function IUI_createButtonId(anId)
  {
    return "inspector-" + anId + "-toolbutton";
  },

  /**
   * Destroy the InspectorUI instance. This is called by the InspectorUI API
   * "user", see BrowserShutdown() in browser.js.
   */
  destroy: function IUI_destroy()
  {
    if (this.isInspectorOpen) {
      this.closeInspectorUI();
    }

    delete this.store;
    delete this.chromeDoc;
    delete this.chromeWin;
    delete this.tabbrowser;
  },
};

/**
 * The Inspector store is used for storing data specific to each tab window.
 * @constructor
 */
function InspectorStore()
{
  this.store = {};
}
InspectorStore.prototype = {
  length: 0,

  /**
   * Check if there is any data recorded for any tab/window.
   *
   * @returns boolean True if there are no stores for any window/tab, or false
   * otherwise.
   */
  isEmpty: function IS_isEmpty()
  {
    return this.length == 0 ? true : false;
  },

  /**
   * Add a new inspector.
   *
   * @param string aID The Store ID you want created.
   * @param Inspector aInspector The inspector to add.
   * @returns boolean True if the store was added successfully, or false
   * otherwise.
   */
  addInspector: function IS_addInspector(aID, aInspector)
  {
    let result = false;

    if (!(aID in this.store)) {
      this.store[aID] = aInspector;
      this.length++;
      result = true;
    }

    return result;
  },

  /**
   * Get the inspector for a window, if any.
   *
   * @param string aID The Store ID you want created.
   */
  getInspector: function IS_getInspector(aID)
  {
    return this.store[aID] || null;
  },

  /**
   * Delete an inspector by ID.
   *
   * @param string aID The store ID you want deleted.
   * @returns boolean True if the store was removed successfully, or false
   * otherwise.
   */
  deleteInspector: function IS_deleteInspector(aID)
  {
    let result = false;

    if (aID in this.store) {
      this.store[aID]._destroy();
      delete this.store[aID];
      this.length--;
      result = true;
    }

    return result;
  },

  /**
   * Check store existence.
   *
   * @param string aID The store ID you want to check.
   * @returns boolean True if the store ID is registered, or false otherwise.
   */
  hasID: function IS_hasID(aID)
  {
    return (aID in this.store);
  },
};

/**
 * The InspectorProgressListener object is an nsIWebProgressListener which
 * handles onStateChange events for the inspected browser. If the user makes
 * changes to the web page and he tries to navigate away, he is prompted to
 * confirm page navigation, such that he's given the chance to prevent the loss
 * of edits.
 *
 * @constructor
 * @param object aInspector
 *        InspectorUI instance object.
 */
function InspectorProgressListener(aInspector)
{
  this.IUI = aInspector;
  this.IUI.tabbrowser.addProgressListener(this);
}

InspectorProgressListener.prototype = {
  onStateChange:
  function IPL_onStateChange(aProgress, aRequest, aFlag, aStatus)
  {
    // Remove myself if the Inspector is no longer open.
    if (!this.IUI.isInspectorOpen) {
      this.destroy();
      return;
    }

    let isStart = aFlag & Ci.nsIWebProgressListener.STATE_START;
    let isDocument = aFlag & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT;
    let isNetwork = aFlag & Ci.nsIWebProgressListener.STATE_IS_NETWORK;
    let isRequest = aFlag & Ci.nsIWebProgressListener.STATE_IS_REQUEST;

    // Skip non-interesting states.
    if (!isStart || !isDocument || !isRequest || !isNetwork) {
      return;
    }

    // If the request is about to happen in a new window, we are not concerned
    // about the request.
    if (aProgress.DOMWindow != this.IUI.win) {
      return;
    }

    if (this.IUI.isDirty) {
      this.showNotification(aRequest);
    } else {
      this.IUI.closeInspectorUI();
    }
  },

  /**
   * Show an asynchronous notification which asks the user to confirm or cancel
   * the page navigation request.
   *
   * @param nsIRequest aRequest
   *        The request initiated by the user or by the page itself.
   * @returns void
   */
  showNotification: function IPL_showNotification(aRequest)
  {
    aRequest.suspend();

    let notificationBox = this.IUI.tabbrowser.getNotificationBox(this.IUI.browser);
    let notification = notificationBox.
      getNotificationWithValue("inspector-page-navigation");

    if (notification) {
      notificationBox.removeNotification(notification, true);
    }

    let cancelRequest = function onCancelRequest() {
      if (aRequest) {
        aRequest.cancel(Cr.NS_BINDING_ABORTED);
        aRequest.resume(); // needed to allow the connection to be cancelled.
        aRequest = null;
      }
    };

    let eventCallback = function onNotificationCallback(aEvent) {
      if (aEvent == "removed") {
        cancelRequest();
      }
    };

    let buttons = [
      {
        id: "inspector.confirmNavigationAway.buttonLeave",
        label: this.IUI.strings.
          GetStringFromName("confirmNavigationAway.buttonLeave"),
        accessKey: this.IUI.strings.
          GetStringFromName("confirmNavigationAway.buttonLeaveAccesskey"),
        callback: function onButtonLeave() {
          if (aRequest) {
            aRequest.resume();
            aRequest = null;
            this.IUI.closeInspectorUI();
            return true;
          }
          return false;
        }.bind(this),
      },
      {
        id: "inspector.confirmNavigationAway.buttonStay",
        label: this.IUI.strings.
          GetStringFromName("confirmNavigationAway.buttonStay"),
        accessKey: this.IUI.strings.
          GetStringFromName("confirmNavigationAway.buttonStayAccesskey"),
        callback: cancelRequest
      },
    ];

    let message = this.IUI.strings.
      GetStringFromName("confirmNavigationAway.message");

    notification = notificationBox.appendNotification(message,
      "inspector-page-navigation", "chrome://browser/skin/Info.png",
      notificationBox.PRIORITY_WARNING_HIGH, buttons, eventCallback);

    // Make sure this not a transient notification, to avoid the automatic
    // transient notification removal.
    notification.persistence = -1;
  },

  /**
   * Destroy the progress listener instance.
   */
  destroy: function IPL_destroy()
  {
    this.IUI.tabbrowser.removeProgressListener(this);

    let notificationBox = this.IUI.tabbrowser.getNotificationBox(this.IUI.browser);
    let notification = notificationBox.
      getNotificationWithValue("inspector-page-navigation");

    if (notification) {
      notificationBox.removeNotification(notification, true);
    }

    delete this.IUI;
  },
};

InspectorUI._registeredSidebars = [];

/**
 * Register an inspector sidebar template.
 * Already running sidebars will not be affected, see bug 740665.
 *
 * @param aRegistration Object
 * {
 *   id: "toolname",
 *   label: "Button or tab label",
 *   icon: "chrome://somepath.png",
 *   tooltiptext: "Button tooltip",
 *   accesskey: "S",
 *   contentURL: string URI, source of the tool's iframe content.
 *   load: Called when the sidebar has been created and the contentURL loaded.
 *         Passed an Inspector object and an iframe object.
 *   destroy: Called when the sidebar is destroyed by the inspector.
 *     Passed whatever was returned by the tool's create function.
 * }
 */
InspectorUI.registerSidebar = function IUI_registerSidebar(aRegistration)
{
  // Only allow a given tool ID to be registered once.
  if (InspectorUI._registeredSidebars.some(function(elt) elt.id == aRegistration.id))
    return false;

  InspectorUI._registeredSidebars.push(aRegistration);

  return true;
}

/**
 * Unregister a previously-registered inspector sidebar.
 * Already running sidebars will not be affected, see bug 740665.
 *
 * @param aID string
 */
InspectorUI.unregisterSidebar = function IUI_unregisterSidebar(aID)
{
  InspectorUI._registeredSidebars = InspectorUI._registeredSidebars.filter(function(aReg) aReg.id != aID);
}

///////////////////////////////////////////////////////////////////////////
//// Style Sidebar

/**
 * Manages the UI and loading of registered sidebar tools.
 * @param aOptions object
 *   Initialization information for the style sidebar, including:
 *     document: The chrome document in which the style sidebar
 *             should be created.
 *     inspector: The Inspector object tied to this sidebar.
 */
function InspectorStyleSidebar(aOptions)
{
  this._tools = {};
  this._chromeDoc = aOptions.document;
  this._inspector = aOptions.inspector;
}

InspectorStyleSidebar.prototype = {

  get visible() !this._box.hasAttribute("hidden"),
  get activePanel() this._deck.selectedPanel._toolID,

  destroy: function ISS_destroy()
  {
    // close the Layout View
    if (this._layoutview) {
      this._layoutview.destroy();
      this._layoutview = null;
    }

    for each (let toolID in Object.getOwnPropertyNames(this._tools)) {
      this.removeTool(toolID);
    }
    delete this._tools;
    this._teardown();
  },

  /**
   * Called by InspectorUI to create the UI for a registered sidebar tool.
   * Will create a toolbar button and an iframe for the tool.
   * @param aRegObj object
   *        See the documentation for InspectorUI.registerSidebar().
   */
  addTool: function ISS_addTool(aRegObj)
  {
    if (aRegObj.id in this._tools) {
      return;
    }

    let btn = this._chromeDoc.createElement("toolbarbutton");
    btn.setAttribute("label", aRegObj.label);
    btn.setAttribute("class", "devtools-toolbarbutton");
    btn.setAttribute("tooltiptext", aRegObj.tooltiptext);
    btn.setAttribute("accesskey", aRegObj.accesskey);
    btn.setAttribute("image", aRegObj.icon || "");
    btn.setAttribute("type", "radio");
    btn.setAttribute("group", "sidebar-tools");
    this._toolbar.appendChild(btn);

    // create tool iframe
    let frame = this._chromeDoc.createElement("iframe");
    frame.setAttribute("flex", "1");
    frame._toolID = aRegObj.id;

    // This is needed to enable tooltips inside the iframe document.
    frame.setAttribute("tooltip", "aHTMLTooltip");

    this._deck.appendChild(frame);

    // wire up button to show the iframe
    let onClick = function() {
      this.activatePanel(aRegObj.id);
    }.bind(this);
    btn.addEventListener("click", onClick, true);

    this._tools[aRegObj.id] = {
      id: aRegObj.id,
      registration: aRegObj,
      button: btn,
      frame: frame,
      loaded: false,
      context: null,
      onClick: onClick
    };
  },

  /**
   * Remove a tool from the sidebar.
   *
   * @param aID string
   *        The string ID of the tool to remove.
   */
  removeTool: function ISS_removeTool(aID)
  {
    if (!aID in this._tools) {
      return;
    }
    let tool = this._tools[aID];
    delete this._tools[aID];

    if (tool.loaded && tool.registration.destroy) {
      tool.registration.destroy(tool.context);
    }

    if (tool.onLoad) {
      tool.frame.removeEventListener("load", tool.onLoad, true);
      delete tool.onLoad;
    }

    if (tool.onClick) {
      tool.button.removeEventListener("click", tool.onClick, true);
      delete tool.onClick;
    }

    tool.button.parentNode.removeChild(tool.button);
    tool.frame.parentNode.removeChild(tool.frame);
  },

  /**
   * Hide or show the sidebar.
   */
  toggle: function ISS_toggle()
  {
    if (!this.visible) {
      this.show();
    } else {
      this.hide();
    }
  },

  /**
   * Shows the sidebar, updating the stored visibility pref.
   */
  show: function ISS_show()
  {
    this._box.removeAttribute("hidden");
    this._splitter.removeAttribute("hidden");
    this._toggleButton.checked = true;

    this._showDefault();

    this._inspector._sidebarOpen = true;
    Services.prefs.setBoolPref("devtools.inspector.sidebarOpen", true);

    // Instantiate the Layout View if needed.
    if (Services.prefs.getBoolPref("devtools.layoutview.enabled")
        && !this._layoutview) {
      this._layoutview = new LayoutView({
        document: this._chromeDoc,
        inspector: this._inspector,
      });
    }
  },

  /**
   * Hides the sidebar, updating the stored visibility pref.
   */
  hide: function ISS_hide()
  {
    this._teardown();
    this._inspector._sidebarOpen = false;
    Services.prefs.setBoolPref("devtools.inspector.sidebarOpen", false);
  },

  /**
   * Hides the sidebar UI elements.
   */
  _teardown: function ISS__teardown()
  {
    this._toggleButton.checked = false;
    this._box.setAttribute("hidden", true);
    this._splitter.setAttribute("hidden", true);
  },

  /**
   * Sets the current sidebar panel.
   *
   * @param aID string
   *        The ID of the panel to make visible.
   */
  activatePanel: function ISS_activatePanel(aID) {
    let tool = this._tools[aID];
    Services.prefs.setCharPref("devtools.inspector.activeSidebar", aID);
    this._inspector._activeSidebar = aID;
    this._deck.selectedPanel = tool.frame;
    this._showContent(tool);
    tool.button.setAttribute("checked", "true");
    let hasSelected = Array.forEach(this._toolbar.children, function(btn) {
      if (btn != tool.button) {
        btn.removeAttribute("checked");
      }
    });
  },

  /**
   * Make the iframe content of a given tool visible.  If this is the first
   * time the tool has been shown, load its iframe content and call the
   * registration object's load method.
   *
   * @param aTool object
   *        The tool object we're loading.
   */
  _showContent: function ISS__showContent(aTool)
  {
    // If the current tool is already loaded, notify that we're
    // showing this sidebar.
    if (aTool.loaded) {
      this._inspector._emit("sidebaractivated", aTool.id);
      this._inspector._emit("sidebaractivated-" + aTool.id);
      return;
    }

    // If we're already loading, we're done.
    if (aTool.onLoad) {
      return;
    }

    // This will be canceled in removeTool if necessary.
    aTool.onLoad = function(evt) {
      if (evt.target.location != aTool.registration.contentURL) {
        return;
      }
      aTool.frame.removeEventListener("load", aTool.onLoad, true);
      delete aTool.onLoad;
      aTool.loaded = true;
      aTool.context = aTool.registration.load(this._inspector, aTool.frame);

      this._inspector._emit("sidebaractivated", aTool.id);
      this._inspector._emit("sidebaractivated-" + aTool.id);
    }.bind(this);
    aTool.frame.addEventListener("load", aTool.onLoad, true);
    aTool.frame.setAttribute("src", aTool.registration.contentURL);
  },

  /**
   * For testing purposes, mostly - return the tool-provided context
   * for a given tool.  Will only work after the tool has been loaded
   * and instantiated.
   */
  _toolContext: function ISS__toolContext(aID) {
    return aID in this._tools ? this._tools[aID].context : null;
  },

  /**
   * Also mostly for testing, return the list of tool objects stored in
   * the sidebar.
   */
  _toolObjects: function ISS__toolObjects() {
    return [this._tools[i] for each (i in Object.getOwnPropertyNames(this._tools))];
  },

  /**
   * If no tool is already selected, show the last-used sidebar.  If there
   * was no last-used sidebar, just show the first one.
   */
  _showDefault: function ISS__showDefault()
  {
    let hasSelected = Array.some(this._toolbar.children,
      function(btn) btn.hasAttribute("checked"));

    // Make sure the selected panel is loaded...
    this._showContent(this._tools[this.activePanel]);

    if (hasSelected) {
      return;
    }

    let activeID = this._inspector._activeSidebar;
    if (!activeID || !(activeID in this._tools)) {
      activeID = Object.getOwnPropertyNames(this._tools)[0];
    }
    this.activatePanel(activeID);
  },

  // DOM elements
  get _toggleButton() this._chromeDoc.getElementById("inspector-style-button"),
  get _box() this._chromeDoc.getElementById("devtools-sidebar-box"),
  get _splitter() this._chromeDoc.getElementById("devtools-side-splitter"),
  get _toolbar() this._chromeDoc.getElementById("devtools-sidebar-toolbar"),
  get _deck() this._chromeDoc.getElementById("devtools-sidebar-deck"),
};

///////////////////////////////////////////////////////////////////////////
//// HTML Breadcrumbs

/**
 * Display the ancestors of the current node and its children.
 * Only one "branch" of children are displayed (only one line).
 *
 * Mechanism:
 * . If no nodes displayed yet:
 *    then display the ancestor of the selected node and the selected node;
 *   else select the node;
 * . If the selected node is the last node displayed, append its first (if any).
 *
 * @param object aInspector
 *        The InspectorUI instance.
 */
function HTMLBreadcrumbs(aInspector)
{
  this.IUI = aInspector;
  this.DOMHelpers = new DOMHelpers(this.IUI.win);
  this._init();
}

HTMLBreadcrumbs.prototype = {
  _init: function BC__init()
  {
    this.container = this.IUI.chromeDoc.getElementById("inspector-breadcrumbs");
    this.container.addEventListener("mousedown", this, true);

    // We will save a list of already displayed nodes in this array.
    this.nodeHierarchy = [];

    // Last selected node in nodeHierarchy.
    this.currentIndex = -1;

    // Siblings menu
    this.menu = this.IUI.chromeDoc.createElement("menupopup");
    this.menu.id = "inspector-breadcrumbs-menu";

    let popupSet = this.IUI.chromeDoc.getElementById("mainPopupSet");
    popupSet.appendChild(this.menu);

    // By default, hide the arrows. We let the <scrollbox> show them
    // in case of overflow.
    this.container.removeAttribute("overflows");
    this.container._scrollButtonUp.collapsed = true;
    this.container._scrollButtonDown.collapsed = true;

    this.onscrollboxreflow = function() {
      if (this.container._scrollButtonDown.collapsed)
        this.container.removeAttribute("overflows");
      else
        this.container.setAttribute("overflows", true);
    }.bind(this);

    this.container.addEventListener("underflow", this.onscrollboxreflow, false);
    this.container.addEventListener("overflow", this.onscrollboxreflow, false);

    this.menu.addEventListener("popuphiding", (function() {
      while (this.menu.hasChildNodes()) {
        this.menu.removeChild(this.menu.firstChild);
      }
      let button = this.container.querySelector("button[siblings-menu-open]");
      button.removeAttribute("siblings-menu-open");
    }).bind(this), false);
  },

  /**
   * Build a string that represents the node: tagName#id.class1.class2.
   *
   * @param aNode The node to pretty-print
   * @returns a string
   */
  prettyPrintNodeAsText: function BC_prettyPrintNodeText(aNode)
  {
    let text = aNode.tagName.toLowerCase();
    if (aNode.id) {
      text += "#" + aNode.id;
    }
    for (let i = 0; i < aNode.classList.length; i++) {
      text += "." + aNode.classList[i];
    }
    for (let i = 0; i < PSEUDO_CLASSES.length; i++) {
      let pseudo = PSEUDO_CLASSES[i];
      if (DOMUtils.hasPseudoClassLock(aNode, pseudo)) {
        text += pseudo;  
      }      
    }

    return text;
  },


  /**
   * Build <label>s that represent the node:
   *   <label class="inspector-breadcrumbs-tag">tagName</label>
   *   <label class="inspector-breadcrumbs-id">#id</label>
   *   <label class="inspector-breadcrumbs-classes">.class1.class2</label>
   *
   * @param aNode The node to pretty-print
   * @returns a document fragment.
   */
  prettyPrintNodeAsXUL: function BC_prettyPrintNodeXUL(aNode)
  {
    let fragment = this.IUI.chromeDoc.createDocumentFragment();

    let tagLabel = this.IUI.chromeDoc.createElement("label");
    tagLabel.className = "inspector-breadcrumbs-tag plain";

    let idLabel = this.IUI.chromeDoc.createElement("label");
    idLabel.className = "inspector-breadcrumbs-id plain";

    let classesLabel = this.IUI.chromeDoc.createElement("label");
    classesLabel.className = "inspector-breadcrumbs-classes plain";
    
    let pseudosLabel = this.IUI.chromeDoc.createElement("label");
    pseudosLabel.className = "inspector-breadcrumbs-pseudo-classes plain";

    tagLabel.textContent = aNode.tagName.toLowerCase();
    idLabel.textContent = aNode.id ? ("#" + aNode.id) : "";

    let classesText = "";
    for (let i = 0; i < aNode.classList.length; i++) {
      classesText += "." + aNode.classList[i];
    }
    classesLabel.textContent = classesText;

    let pseudos = PSEUDO_CLASSES.filter(function(pseudo) {
      return DOMUtils.hasPseudoClassLock(aNode, pseudo);
    }, this);
    pseudosLabel.textContent = pseudos.join("");

    fragment.appendChild(tagLabel);
    fragment.appendChild(idLabel);
    fragment.appendChild(classesLabel);
    fragment.appendChild(pseudosLabel);

    return fragment;
  },

  /**
   * Open the sibling menu.
   *
   * @param aButton the button representing the node.
   * @param aNode the node we want the siblings from.
   */
  openSiblingMenu: function BC_openSiblingMenu(aButton, aNode)
  {
    let title = this.IUI.chromeDoc.createElement("menuitem");
    title.setAttribute("label",
      this.IUI.strings.GetStringFromName("breadcrumbs.siblings"));
    title.setAttribute("disabled", "true");

    let separator = this.IUI.chromeDoc.createElement("menuseparator");

    this.menu.appendChild(title);
    this.menu.appendChild(separator);

    let fragment = this.IUI.chromeDoc.createDocumentFragment();

    let nodes = aNode.parentNode.childNodes;
    for (let i = 0; i < nodes.length; i++) {
      if (nodes[i].nodeType == aNode.ELEMENT_NODE) {
        let item = this.IUI.chromeDoc.createElement("menuitem");
        let inspector = this.IUI;
        if (nodes[i] === aNode) {
          item.setAttribute("disabled", "true");
          item.setAttribute("checked", "true");
        }

        item.setAttribute("type", "radio");
        item.setAttribute("label", this.prettyPrintNodeAsText(nodes[i]));

        item.onmouseup = (function(aNode) {
          return function() {
            inspector.select(aNode, true, true, "breadcrumbs");
          }
        })(nodes[i]);

        fragment.appendChild(item);
      }
    }
    this.menu.appendChild(fragment);
    this.menu.openPopup(aButton, "before_start", 0, 0, true, false);
    aButton.setAttribute("siblings-menu-open", "true");
  },

  /**
   * Generic event handler.
   *
   * @param nsIDOMEvent aEvent
   *        The DOM event object.
   */
  handleEvent: function BC_handleEvent(aEvent)
  {
    if (aEvent.type == "mousedown" && aEvent.button == 0) {
      // on Click and Hold, open the Siblings menu

      let timer;
      let container = this.container;
      let window = this.IUI.win;

      function openMenu(aEvent) {
        cancelHold();
        let target = aEvent.originalTarget;
        if (target.tagName == "button") {
          target.onBreadcrumbsHold();
        }
      }

      function handleClick(aEvent) {
        cancelHold();
        let target = aEvent.originalTarget;
        if (target.tagName == "button") {
          target.onBreadcrumbsClick();
        }
      }

      function cancelHold(aEvent) {
        window.clearTimeout(timer);
        container.removeEventListener("mouseout", cancelHold, false);
        container.removeEventListener("mouseup", handleClick, false);
      }

      container.addEventListener("mouseout", cancelHold, false);
      container.addEventListener("mouseup", handleClick, false);
      timer = window.setTimeout(openMenu, 500, aEvent);
    }
  },

  /**
   * Remove nodes and delete properties.
   */
  destroy: function BC_destroy()
  {
    this.container.removeEventListener("underflow", this.onscrollboxreflow, false);
    this.container.removeEventListener("overflow", this.onscrollboxreflow, false);
    this.onscrollboxreflow = null;

    this.empty();
    this.container.removeEventListener("mousedown", this, true);
    this.menu.parentNode.removeChild(this.menu);
    this.container = null;
    this.nodeHierarchy = null;
  },

  /**
   * Empty the breadcrumbs container.
   */
  empty: function BC_empty()
  {
    while (this.container.hasChildNodes()) {
      this.container.removeChild(this.container.firstChild);
    }
  },

  /**
   * Re-init the cache and remove all the buttons.
   */
  invalidateHierarchy: function BC_invalidateHierarchy()
  {
    this.menu.hidePopup();
    this.nodeHierarchy = [];
    this.empty();
  },

  /**
   * Set which button represent the selected node.
   *
   * @param aIdx Index of the displayed-button to select
   */
  setCursor: function BC_setCursor(aIdx)
  {
    // Unselect the previously selected button
    if (this.currentIndex > -1 && this.currentIndex < this.nodeHierarchy.length) {
      this.nodeHierarchy[this.currentIndex].button.removeAttribute("checked");
    }
    if (aIdx > -1) {
      this.nodeHierarchy[aIdx].button.setAttribute("checked", "true");
      if (this.hadFocus)
        this.nodeHierarchy[aIdx].button.focus();
    }
    this.currentIndex = aIdx;
  },

  /**
   * Get the index of the node in the cache.
   *
   * @param aNode
   * @returns integer the index, -1 if not found
   */
  indexOf: function BC_indexOf(aNode)
  {
    let i = this.nodeHierarchy.length - 1;
    for (let i = this.nodeHierarchy.length - 1; i >= 0; i--) {
      if (this.nodeHierarchy[i].node === aNode) {
        return i;
      }
    }
    return -1;
  },

  /**
   * Remove all the buttons and their references in the cache
   * after a given index.
   *
   * @param aIdx
   */
  cutAfter: function BC_cutAfter(aIdx)
  {
    while (this.nodeHierarchy.length > (aIdx + 1)) {
      let toRemove = this.nodeHierarchy.pop();
      this.container.removeChild(toRemove.button);
    }
  },

  /**
   * Build a button representing the node.
   *
   * @param aNode The node from the page.
   * @returns aNode The <button>.
   */
  buildButton: function BC_buildButton(aNode)
  {
    let button = this.IUI.chromeDoc.createElement("button");
    let inspector = this.IUI;
    button.appendChild(this.prettyPrintNodeAsXUL(aNode));
    button.className = "inspector-breadcrumbs-button";

    button.setAttribute("tooltiptext", this.prettyPrintNodeAsText(aNode));

    button.onkeypress = function onBreadcrumbsKeypress(e) {
      if (e.charCode == Ci.nsIDOMKeyEvent.DOM_VK_SPACE ||
          e.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_RETURN)
        button.click();
    }

    button.onBreadcrumbsClick = function onBreadcrumbsClick() {
      inspector.stopInspecting();
      inspector.select(aNode, true, true, "breadcrumbs");
    };

    button.onclick = (function _onBreadcrumbsRightClick(aEvent) {
      if (aEvent.button == 2) {
        this.openSiblingMenu(button, aNode);
      }
    }).bind(this);

    button.onBreadcrumbsHold = (function _onBreadcrumbsHold() {
      this.openSiblingMenu(button, aNode);
    }).bind(this);
    return button;
  },

  /**
   * Connecting the end of the breadcrumbs to a node.
   *
   * @param aNode The node to reach.
   */
  expand: function BC_expand(aNode)
  {
      let fragment = this.IUI.chromeDoc.createDocumentFragment();
      let toAppend = aNode;
      let lastButtonInserted = null;
      let originalLength = this.nodeHierarchy.length;
      let stopNode = null;
      if (originalLength > 0) {
        stopNode = this.nodeHierarchy[originalLength - 1].node;
      }
      while (toAppend && toAppend.tagName && toAppend != stopNode) {
        let button = this.buildButton(toAppend);
        fragment.insertBefore(button, lastButtonInserted);
        lastButtonInserted = button;
        this.nodeHierarchy.splice(originalLength, 0, {node: toAppend, button: button});
        toAppend = this.DOMHelpers.getParentObject(toAppend);
      }
      this.container.appendChild(fragment, this.container.firstChild);
  },

  /**
   * Get a child of a node that can be displayed in the breadcrumbs.
   * By default, we want a node that can highlighted by the highlighter.
   * If no highlightable child is found, we return the first node of type
   * ELEMENT_NODE.
   *
   * @param aNode The parent node.
   * @returns nsIDOMNode|null
   */
  getFirstHighlightableChild: function BC_getFirstHighlightableChild(aNode)
  {
    let nextChild = this.DOMHelpers.getChildObject(aNode, 0);
    let fallback = null;

    while (nextChild) {
      if (this.IUI.highlighter.isNodeHighlightable(nextChild)) {
        return nextChild;
      }
      if (!fallback && nextChild.nodeType == aNode.ELEMENT_NODE) {
        fallback = nextChild;
      }
      nextChild = this.DOMHelpers.getNextSibling(nextChild);
    }
    return fallback;
  },

  /**
   * Find the "youngest" ancestor of a node which is already in the breadcrumbs.
   *
   * @param aNode
   * @returns Index of the ancestor in the cache
   */
  getCommonAncestor: function BC_getCommonAncestor(aNode)
  {
    let node = aNode;
    while (node) {
      let idx = this.indexOf(node);
      if (idx > -1) {
        return idx;
      } else {
        node = this.DOMHelpers.getParentObject(node);
      }
    }
    return -1;
  },

  /**
   * Make sure that the latest node in the breadcrumbs is not the selected node
   * if the selected node still has children.
   */
  ensureFirstChild: function BC_ensureFirstChild()
  {
    // If the last displayed node is the selected node
    if (this.currentIndex == this.nodeHierarchy.length - 1) {
      let node = this.nodeHierarchy[this.currentIndex].node;
      let child = this.getFirstHighlightableChild(node);
      // If the node has a child
      if (child) {
        // Show this child
        this.expand(child);
      }
    }
  },

  /**
   * Ensure the selected node is visible.
   */
  scroll: function BC_scroll()
  {
    // FIXME bug 684352: make sure its immediate neighbors are visible too.

    let scrollbox = this.container;
    let element = this.nodeHierarchy[this.currentIndex].button;
    scrollbox.ensureElementIsVisible(element);
  },
  
  updateSelectors: function BC_updateSelectors()
  {
    for (let i = this.nodeHierarchy.length - 1; i >= 0; i--) {
      let crumb = this.nodeHierarchy[i];
      let button = crumb.button;

      while(button.hasChildNodes()) {
        button.removeChild(button.firstChild);
      }
      button.appendChild(this.prettyPrintNodeAsXUL(crumb.node));
      button.setAttribute("tooltiptext", this.prettyPrintNodeAsText(crumb.node));
    }
  },

  /**
   * Update the breadcrumbs display when a new node is selected.
   */
  update: function BC_update()
  {
    this.menu.hidePopup();

    let cmdDispatcher = this.IUI.chromeDoc.commandDispatcher;
    this.hadFocus = (cmdDispatcher.focusedElement &&
                     cmdDispatcher.focusedElement.parentNode == this.container);

    let selection = this.IUI.selection;
    let idx = this.indexOf(selection);

    // Is the node already displayed in the breadcrumbs?
    if (idx > -1) {
      // Yes. We select it.
      this.setCursor(idx);
    } else {
      // No. Is the breadcrumbs display empty?
      if (this.nodeHierarchy.length > 0) {
        // No. We drop all the element that are not direct ancestors
        // of the selection
        let parent = this.DOMHelpers.getParentObject(selection);
        let idx = this.getCommonAncestor(parent);
        this.cutAfter(idx);
      }
      // we append the missing button between the end of the breadcrumbs display
      // and the current node.
      this.expand(selection);

      // we select the current node button
      idx = this.indexOf(selection);
      this.setCursor(idx);
    }
    // Add the first child of the very last node of the breadcrumbs if possible.
    this.ensureFirstChild();

    // Make sure the selected node and its neighbours are visible.
    this.scroll();

    this.updateSelectors();
  },

}

/////////////////////////////////////////////////////////////////////////
//// Initializers

XPCOMUtils.defineLazyGetter(InspectorUI.prototype, "strings",
  function () {
    return Services.strings.createBundle(
            "chrome://browser/locale/devtools/inspector.properties");
  });

XPCOMUtils.defineLazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

XPCOMUtils.defineLazyGetter(this, "clipboardHelper", function() {
  return Cc["@mozilla.org/widget/clipboardhelper;1"].
    getService(Ci.nsIClipboardHelper);
});
