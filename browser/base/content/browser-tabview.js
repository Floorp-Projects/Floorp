# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

let TabView = {
  _deck: null,
  _iframe: null,
  _window: null,
  _initialized: false,
  _browserKeyHandlerInitialized: false,
  _closedLastVisibleTabBeforeFrameInitialized: false,
  _isFrameLoading: false,
  _initFrameCallbacks: [],
  _lastSessionGroupName: null,
  PREF_BRANCH: "browser.panorama.",
  PREF_FIRST_RUN: "browser.panorama.experienced_first_run",
  PREF_STARTUP_PAGE: "browser.startup.page",
  PREF_RESTORE_ENABLED_ONCE: "browser.panorama.session_restore_enabled_once",
  GROUPS_IDENTIFIER: "tabview-groups",
  VISIBILITY_IDENTIFIER: "tabview-visibility",

  // ----------
  get windowTitle() {
    delete this.windowTitle;
    let brandBundle = document.getElementById("bundle_brand");
    let brandShortName = brandBundle.getString("brandShortName");
    let title = gNavigatorBundle.getFormattedString("tabView2.title", [brandShortName]);
    return this.windowTitle = title;
  },

  // ----------
  get firstUseExperienced() {
    let pref = this.PREF_FIRST_RUN;
    if (Services.prefs.prefHasUserValue(pref))
      return Services.prefs.getBoolPref(pref);

    return false;
  },

  // ----------
  set firstUseExperienced(val) {
    Services.prefs.setBoolPref(this.PREF_FIRST_RUN, val);
  },

  // ----------
  get sessionRestoreEnabledOnce() {
    let pref = this.PREF_RESTORE_ENABLED_ONCE;
    if (Services.prefs.prefHasUserValue(pref))
      return Services.prefs.getBoolPref(pref);

    return false;
  },

  // ----------
  set sessionRestoreEnabledOnce(val) {
    Services.prefs.setBoolPref(this.PREF_RESTORE_ENABLED_ONCE, val);
  },

  // ----------
  init: function TabView_init() {
    // disable the ToggleTabView command for popup windows
    goSetCommandEnabled("Browser:ToggleTabView", window.toolbar.visible);
    if (!window.toolbar.visible)
      return;

    if (this._initialized)
      return;

    if (this.firstUseExperienced) {
      // ___ visibility
      let sessionstore =
        Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

      let data = sessionstore.getWindowValue(window, this.VISIBILITY_IDENTIFIER);
      if (data && data == "true") {
        this.show();
      } else {
        try {
          data = sessionstore.getWindowValue(window, this.GROUPS_IDENTIFIER);
          if (data) {
            let parsedData = JSON.parse(data);
            this.updateGroupNumberBroadcaster(parsedData.totalNumber || 1);
          }
        } catch (e) { }

        let self = this;
        // if a tab is changed from hidden to unhidden and the iframe is not
        // initialized, load the iframe and setup the tab.
        this._tabShowEventListener = function(event) {
          if (!self._window)
            self._initFrame(function() {
              self._window.UI.onTabSelect(gBrowser.selectedTab);
              if (self._closedLastVisibleTabBeforeFrameInitialized) {
                self._closedLastVisibleTabBeforeFrameInitialized = false;
                self._window.UI.showTabView(false);
              }
            });
        };
        this._tabCloseEventListener = function(event) {
          if (!self._window && gBrowser.visibleTabs.length == 0)
            self._closedLastVisibleTabBeforeFrameInitialized = true;
        };
        gBrowser.tabContainer.addEventListener(
          "TabShow", this._tabShowEventListener, false);
        gBrowser.tabContainer.addEventListener(
          "TabClose", this._tabCloseEventListener, false);

       if (this._tabBrowserHasHiddenTabs()) {
         this._setBrowserKeyHandlers();
       } else {
         // for restoring last session and undoing recently closed window
         this._SSWindowStateReadyListener = function (event) {
           if (this._tabBrowserHasHiddenTabs())
             this._setBrowserKeyHandlers();
         }.bind(this);
         window.addEventListener(
           "SSWindowStateReady", this._SSWindowStateReadyListener, false);
        }
      }
    }

    Services.prefs.addObserver(this.PREF_BRANCH, this, false);

    this._initialized = true;
  },

  // ----------
  // Observes topic changes.
  observe: function TabView_observe(subject, topic, data) {
    if (data == this.PREF_FIRST_RUN && this.firstUseExperienced) {
      this._addToolbarButton();
      this.enableSessionRestore();
    }
  },

  // ----------
  // Uninitializes TabView.
  uninit: function TabView_uninit() {
    if (!this._initialized)
      return;

    Services.prefs.removeObserver(this.PREF_BRANCH, this);

    if (this._tabShowEventListener)
      gBrowser.tabContainer.removeEventListener(
        "TabShow", this._tabShowEventListener, false);

    if (this._tabCloseEventListener)
      gBrowser.tabContainer.removeEventListener(
        "TabClose", this._tabCloseEventListener, false);

    if (this._SSWindowStateReadyListener)
      window.removeEventListener(
        "SSWindowStateReady", this._SSWindowStateReadyListener, false);

    this._initialized = false;
  },

  // ----------
  // Creates the frame and calls the callback once it's loaded. 
  // If the frame already exists, calls the callback immediately. 
  _initFrame: function TabView__initFrame(callback) {
    let hasCallback = typeof callback == "function";

    // prevent frame to be initialized for popup windows
    if (!window.toolbar.visible)
      return;

    if (this._window) {
      if (hasCallback)
        callback();
      return;
    }

    if (hasCallback)
      this._initFrameCallbacks.push(callback);

    if (this._isFrameLoading)
      return;

    this._isFrameLoading = true;

    TelemetryStopwatch.start("PANORAMA_INITIALIZATION_TIME_MS");

    // ___ find the deck
    this._deck = document.getElementById("tab-view-deck");

    // ___ create the frame
    this._iframe = document.createElement("iframe");
    this._iframe.id = "tab-view";
    this._iframe.setAttribute("transparent", "true");
    this._iframe.setAttribute("tooltip", "tab-view-tooltip");
    this._iframe.flex = 1;

    let self = this;

    window.addEventListener("tabviewframeinitialized", function onInit() {
      window.removeEventListener("tabviewframeinitialized", onInit, false);

      TelemetryStopwatch.finish("PANORAMA_INITIALIZATION_TIME_MS");

      self._isFrameLoading = false;
      self._window = self._iframe.contentWindow;
      self._setBrowserKeyHandlers();

      if (self._tabShowEventListener) {
        gBrowser.tabContainer.removeEventListener(
          "TabShow", self._tabShowEventListener, false);
        self._tabShowEventListener = null;
      }
      if (self._tabCloseEventListener) {
        gBrowser.tabContainer.removeEventListener(
          "TabClose", self._tabCloseEventListener, false);
        self._tabCloseEventListener = null;
      }
      if (self._SSWindowStateReadyListener) {
        window.removeEventListener(
          "SSWindowStateReady", self._SSWindowStateReadyListener, false);
        self._SSWindowStateReadyListener = null;
      }

      self._initFrameCallbacks.forEach(function (cb) cb());
      self._initFrameCallbacks = [];
    }, false);

    this._iframe.setAttribute("src", "chrome://browser/content/tabview.html");
    this._deck.appendChild(this._iframe);

    // ___ create tooltip
    let tooltip = document.createElement("tooltip");
    tooltip.id = "tab-view-tooltip";
    tooltip.setAttribute("onpopupshowing", "return TabView.fillInTooltip(document.tooltipNode);");
    document.getElementById("mainPopupSet").appendChild(tooltip);
  },

  // ----------
  getContentWindow: function TabView_getContentWindow() {
    return this._window;
  },

  // ----------
  isVisible: function TabView_isVisible() {
    return (this._deck ? this._deck.selectedPanel == this._iframe : false);
  },

  // ----------
  show: function TabView_show() {
    if (this.isVisible())
      return;

    let self = this;
    this._initFrame(function() {
      self._window.UI.showTabView(true);
    });
  },

  // ----------
  hide: function TabView_hide() {
    if (!this.isVisible())
      return;

    this._window.UI.exit();
  },

  // ----------
  toggle: function TabView_toggle() {
    if (this.isVisible())
      this.hide();
    else 
      this.show();
  },

  // ----------
  _tabBrowserHasHiddenTabs: function TabView_tabBrowserHasHiddenTabs() {
    return (gBrowser.tabs.length - gBrowser.visibleTabs.length) > 0;
  },

  // ----------
  updateContextMenu: function TabView_updateContextMenu(tab, popup) {
    let separator = document.getElementById("context_tabViewNamedGroups");
    let isEmpty = true;

    while (popup.firstChild && popup.firstChild != separator)
      popup.removeChild(popup.firstChild);

    let self = this;
    this._initFrame(function() {
      let activeGroup = tab._tabViewTabItem.parent;
      let groupItems = self._window.GroupItems.groupItems;

      groupItems.forEach(function(groupItem) {
        // if group has title, it's not hidden and there is no active group or
        // the active group id doesn't match the group id, a group menu item
        // would be added.
        if (!groupItem.hidden && groupItem.getChildren().length &&
            (!activeGroup || activeGroup.id != groupItem.id)) {
          let menuItem = self._createGroupMenuItem(groupItem);
          popup.insertBefore(menuItem, separator);
          isEmpty = false;
        }
      });
      separator.hidden = isEmpty;
    });
  },

  // ----------
  _createGroupMenuItem: function TabView__createGroupMenuItem(groupItem) {
    let menuItem = document.createElement("menuitem");
    let title = groupItem.getTitle();

    if (!title.trim()) {
      let topChildLabel = groupItem.getTopChild().tab.label;

      if (groupItem.getChildren().length > 1) {
        title =
          gNavigatorBundle.getFormattedString("tabview2.moveToUnnamedGroup.label",
            [topChildLabel, groupItem.getChildren().length - 1]);
      } else {
        title = topChildLabel;
      }
    }

    menuItem.setAttribute("label", title);
    menuItem.setAttribute("tooltiptext", title);
    menuItem.setAttribute("crop", "center");
    menuItem.setAttribute("class", "tabview-menuitem");
    menuItem.setAttribute(
      "oncommand",
      "TabView.moveTabTo(TabContextMenu.contextTab,'" + groupItem.id + "')");

    return menuItem;
  },

  // ----------
  moveTabTo: function TabView_moveTabTo(tab, groupItemId) {
    if (this._window) {
      this._window.GroupItems.moveTabToGroupItem(tab, groupItemId);
    } else {
      let self = this;
      this._initFrame(function() {
        self._window.GroupItems.moveTabToGroupItem(tab, groupItemId);
      });
    }
  },

  // ----------
  // Adds new key commands to the browser, for invoking the Tab Candy UI
  // and for switching between groups of tabs when outside of the Tab Candy UI.
  _setBrowserKeyHandlers: function TabView__setBrowserKeyHandlers() {
    if (this._browserKeyHandlerInitialized)
      return;

    this._browserKeyHandlerInitialized = true;

    let self = this;
    window.addEventListener("keypress", function(event) {
      if (self.isVisible() || !self._tabBrowserHasHiddenTabs())
        return;

      let charCode = event.charCode;
      // Control (+ Shift) + `
      if (event.ctrlKey && !event.metaKey && !event.altKey &&
          (charCode == 96 || charCode == 126)) {
        event.stopPropagation();
        event.preventDefault();

        self._initFrame(function() {
          let groupItems = self._window.GroupItems;
          let tabItem = groupItems.getNextGroupItemTab(event.shiftKey);
          if (!tabItem)
            return;

          if (gBrowser.selectedTab.pinned)
            groupItems.updateActiveGroupItemAndTabBar(tabItem, {dontSetActiveTabInGroup: true});
          else
            gBrowser.selectedTab = tabItem.tab;
        });
      }
    }, true);
  },

  // ----------
  // Prepares the tab view for undo close tab.
  prepareUndoCloseTab: function TabView_prepareUndoCloseTab(blankTabToRemove) {
    if (this._window) {
      this._window.UI.restoredClosedTab = true;

      if (blankTabToRemove && blankTabToRemove._tabViewTabItem)
        blankTabToRemove._tabViewTabItem.isRemovedAfterRestore = true;
    }
  },

  // ----------
  // Cleans up the tab view after undo close tab.
  afterUndoCloseTab: function TabView_afterUndoCloseTab() {
    if (this._window)
      this._window.UI.restoredClosedTab = false;
  },

  // ----------
  // On move to group pop showing.
  moveToGroupPopupShowing: function TabView_moveToGroupPopupShowing(event) {
    // Update the context menu only if Panorama was already initialized or if
    // there are hidden tabs.
    let numHiddenTabs = gBrowser.tabs.length - gBrowser.visibleTabs.length;
    if (this._window || numHiddenTabs > 0)
      this.updateContextMenu(TabContextMenu.contextTab, event.target);
  },

  // ----------
  // Function: _addToolbarButton
  // Adds the TabView button to the TabsToolbar.
  _addToolbarButton: function TabView__addToolbarButton() {
    let buttonId = "tabview-button";

    if (document.getElementById(buttonId))
      return;

    let toolbar = document.getElementById("TabsToolbar");
    let currentSet = toolbar.currentSet.split(",");

    let alltabsPos = currentSet.indexOf("alltabs-button");
    if (-1 == alltabsPos)
      return;

    currentSet[alltabsPos] += "," + buttonId;
    currentSet = currentSet.join(",");
    toolbar.currentSet = currentSet;
    toolbar.setAttribute("currentset", currentSet);
    document.persist(toolbar.id, "currentset");
  },

  // ----------
  // Function: updateGroupNumberBroadcaster
  // Updates the group number broadcaster.
  updateGroupNumberBroadcaster: function TabView_updateGroupNumberBroadcaster(number) {
    let groupsNumber = document.getElementById("tabviewGroupsNumber");
    groupsNumber.setAttribute("groups", number);
  },

  // ----------
  // Function: enableSessionRestore
  // Enables automatic session restore when the browser is started. Does
  // nothing if we already did that once in the past.
  enableSessionRestore: function TabView_enableSessionRestore() {
    if (!this._window || !this.firstUseExperienced)
      return;

    // do nothing if we already enabled session restore once
    if (this.sessionRestoreEnabledOnce)
      return;

    this.sessionRestoreEnabledOnce = true;

    // enable session restore if necessary
    if (Services.prefs.getIntPref(this.PREF_STARTUP_PAGE) != 3) {
      Services.prefs.setIntPref(this.PREF_STARTUP_PAGE, 3);

      // show banner
      this._window.UI.notifySessionRestoreEnabled();
    }
  },

  // ----------
  // Function: fillInTooltip
  // Fills in the tooltip text.
  fillInTooltip: function fillInTooltip(tipElement) {
    let retVal = false;
    let titleText = null;
    let direction = tipElement.ownerDocument.dir;

    while (!titleText && tipElement) {
      if (tipElement.nodeType == Node.ELEMENT_NODE)
        titleText = tipElement.getAttribute("title");
      tipElement = tipElement.parentNode;
    }
    let tipNode = document.getElementById("tab-view-tooltip");
    tipNode.style.direction = direction;

    if (titleText) {
      tipNode.setAttribute("label", titleText);
      retVal = true;
    }

    return retVal;
  }
};
