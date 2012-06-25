/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// **********
// Title: ui.js

let Keys = { meta: false };

// ##########
// Class: UI
// Singleton top-level UI manager.
let UI = {
  // Variable: _frameInitialized
  // True if the Tab View UI frame has been initialized.
  _frameInitialized: false,

  // Variable: _pageBounds
  // Stores the page bounds.
  _pageBounds: null,

  // Variable: _closedLastVisibleTab
  // If true, the last visible tab has just been closed in the tab strip.
  _closedLastVisibleTab: false,

  // Variable: _closedSelectedTabInTabView
  // If true, a select tab has just been closed in TabView.
  _closedSelectedTabInTabView: false,

  // Variable: restoredClosedTab
  // If true, a closed tab has just been restored.
  restoredClosedTab: false,

  // Variable: _isChangingVisibility
  // Tracks whether we're currently in the process of showing/hiding the tabview.
  _isChangingVisibility: false,

  // Variable: _reorderTabItemsOnShow
  // Keeps track of the <GroupItem>s which their tab items' tabs have been moved
  // and re-orders the tab items when switching to TabView.
  _reorderTabItemsOnShow: [],

  // Variable: _reorderTabsOnHide
  // Keeps track of the <GroupItem>s which their tab items have been moved in
  // TabView UI and re-orders the tabs when switcing back to main browser.
  _reorderTabsOnHide: [],

  // Variable: _currentTab
  // Keeps track of which xul:tab we are currently on.
  // Used to facilitate zooming down from a previous tab.
  _currentTab: null,

  // Variable: _eventListeners
  // Keeps track of event listeners added to the AllTabs object.
  _eventListeners: {},

  // Variable: _cleanupFunctions
  // An array of functions to be called at uninit time
  _cleanupFunctions: [],
  
  // Constant: _maxInteractiveWait
  // If the UI is in the middle of an operation, this is the max amount of
  // milliseconds to wait between input events before we no longer consider
  // the operation interactive.
  _maxInteractiveWait: 250,

  // Variable: _privateBrowsing
  // Keeps track of info related to private browsing, including: 
  //   transitionMode - whether we're entering or exiting PB
  //   wasInTabView - whether TabView was visible before we went into PB
  _privateBrowsing: {
    transitionMode: "",
    wasInTabView: false 
  },
  
  // Variable: _storageBusy
  // Tells whether the storage is currently busy or not.
  _storageBusy: false,

  // Variable: isDOMWindowClosing
  // Tells wether the parent window is about to close
  isDOMWindowClosing: false,

  // Variable: _browserKeys
  // Used to keep track of allowed browser keys.
  _browserKeys: null,

  // Variable: _browserKeysWithShift
  // Used to keep track of allowed browser keys with Shift key combination.
  _browserKeysWithShift: null,

  // Variable: ignoreKeypressForSearch
  // Used to prevent keypress being handled after quitting search mode.
  ignoreKeypressForSearch: false,

  // Variable: _lastOpenedTab
  // Used to keep track of the last opened tab.
  _lastOpenedTab: null,

  // Variable: _originalSmoothScroll
  // Used to keep track of the tab strip smooth scroll value.
  _originalSmoothScroll: null,

  // ----------
  // Function: toString
  // Prints [UI] for debug use
  toString: function UI_toString() {
    return "[UI]";
  },

  // ----------
  // Function: init
  // Must be called after the object is created.
  init: function UI_init() {
    try {
      let self = this;

      // initialize the direction of the page
      this._initPageDirection();

      // ___ storage
      Storage.init();

      if (Storage.readWindowBusyState(gWindow))
        this.storageBusy();

      let data = Storage.readUIData(gWindow);
      this._storageSanity(data);
      this._pageBounds = data.pageBounds;

      // ___ search
      Search.init();

      Telemetry.init();

      // ___ currentTab
      this._currentTab = gBrowser.selectedTab;

      // ___ exit button
      iQ("#exit-button").click(function() {
        self.exit();
        self.blurAll();
      })
      .attr("title", tabviewString("button.exitTabGroups"));

      // When you click on the background/empty part of TabView,
      // we create a new groupItem.
      iQ(gTabViewFrame.contentDocument).mousedown(function(e) {
        if (iQ(":focus").length > 0) {
          iQ(":focus").each(function(element) {
            // don't fire blur event if the same input element is clicked.
            if (e.target != element && element.nodeName == "INPUT")
              element.blur();
          });
        }
        if (e.originalTarget.id == "content" &&
            Utils.isLeftClick(e) &&
            e.detail == 1) {
          self._createGroupItemOnDrag(e);
        }
      });

      iQ(gTabViewFrame.contentDocument).dblclick(function(e) {
        if (e.originalTarget.id != "content")
          return;

        // Create a group with one tab on double click
        let box =
          new Rect(e.clientX - Math.floor(TabItems.tabWidth/2),
                   e.clientY - Math.floor(TabItems.tabHeight/2),
                   TabItems.tabWidth, TabItems.tabHeight);
        box.inset(-30, -30);

        let opts = {immediately: true, bounds: box};
        let groupItem = new GroupItem([], opts);
        groupItem.newTab();

        gTabView.firstUseExperienced = true;
      });

      iQ(window).bind("unload", function() {
        self.uninit();
      });

      // ___ setup DOMWillOpenModalDialog message handler
      let mm = gWindow.messageManager;
      let callback = this._onDOMWillOpenModalDialog.bind(this);
      mm.addMessageListener("Panorama:DOMWillOpenModalDialog", callback);

      this._cleanupFunctions.push(function () {
        mm.removeMessageListener("Panorama:DOMWillOpenModalDialog", callback);
      });

      // ___ setup key handlers
      this._setTabViewFrameKeyHandlers();

      // ___ add tab action handlers
      this._addTabActionHandlers();

      // ___ groups
      GroupItems.init();
      GroupItems.pauseArrange();
      let hasGroupItemsData = GroupItems.load();

      // ___ tabs
      TabItems.init();
      TabItems.pausePainting();

      // ___ favicons
      FavIcons.init();

      if (!hasGroupItemsData)
        this.reset();

      // ___ resizing
      if (this._pageBounds)
        this._resize(true);
      else
        this._pageBounds = Items.getPageBounds();

      iQ(window).resize(function() {
        self._resize();
      });

      // ___ setup event listener to save canvas images
      gWindow.addEventListener("SSWindowClosing", function onWindowClosing() {
        gWindow.removeEventListener("SSWindowClosing", onWindowClosing, false);

        // XXX bug #635975 - don't unlink the tab if the dom window is closing.
        self.isDOMWindowClosing = true;

        if (self.isTabViewVisible())
          GroupItems.removeHiddenGroups();

        TabItems.saveAll();

        self._save();
      }, false);

      // ___ load frame script
      let frameScript = "chrome://browser/content/tabview-content.js";
      gWindow.messageManager.loadFrameScript(frameScript, true);

      // ___ Done
      this._frameInitialized = true;
      this._save();

      // fire an iframe initialized event so everyone knows tab view is 
      // initialized.
      let event = document.createEvent("Events");
      event.initEvent("tabviewframeinitialized", true, false);
      dispatchEvent(event);
    } catch(e) {
      Utils.log(e);
    } finally {
      GroupItems.resumeArrange();
    }
  },

  // Function: uninit
  // Should be called when window is unloaded.
  uninit: function UI_uninit() {
    // call our cleanup functions
    this._cleanupFunctions.forEach(function(func) {
      func();
    });
    this._cleanupFunctions = [];

    // additional clean up
    TabItems.uninit();
    GroupItems.uninit();
    FavIcons.uninit();
    Storage.uninit();
    Telemetry.uninit();

    this._removeTabActionHandlers();
    this._currentTab = null;
    this._pageBounds = null;
    this._reorderTabItemsOnShow = null;
    this._reorderTabsOnHide = null;
    this._frameInitialized = false;
  },

  // Property: rtl
  // Returns true if we are in RTL mode, false otherwise
  rtl: false,

  // Function: reset
  // Resets the Panorama view to have just one group with all tabs
  reset: function UI_reset() {
    let padding = Trenches.defaultRadius;
    let welcomeWidth = 300;
    let pageBounds = Items.getPageBounds();
    pageBounds.inset(padding, padding);

    let $actions = iQ("#actions");
    if ($actions) {
      pageBounds.width -= $actions.width();
      if (UI.rtl)
        pageBounds.left += $actions.width() - padding;
    }

    // ___ make a fresh groupItem
    let box = new Rect(pageBounds);
    box.width = Math.min(box.width * 0.667,
                         pageBounds.width - (welcomeWidth + padding));
    box.height = box.height * 0.667;
    if (UI.rtl) {
      box.left = pageBounds.left + welcomeWidth + 2 * padding;
    }

    GroupItems.groupItems.forEach(function(group) {
      group.close();
    });
    
    let options = {
      bounds: box,
      immediately: true
    };
    let groupItem = new GroupItem([], options);
    let items = TabItems.getItems();
    items.forEach(function(item) {
      if (item.parent)
        item.parent.remove(item);
      groupItem.add(item, {immediately: true});
    });
    this.setActive(groupItem);
  },

  // ----------
  // Function: blurAll
  // Blurs any currently focused element
  blurAll: function UI_blurAll() {
    iQ(":focus").each(function(element) {
      element.blur();
    });
  },

  // ----------
  // Function: isIdle
  // Returns true if the last interaction was long enough ago to consider the
  // UI idle. Used to determine whether interactivity would be sacrificed if 
  // the CPU was to become busy.
  //
  isIdle: function UI_isIdle() {
    let time = Date.now();
    let maxEvent = Math.max(drag.lastMoveTime, resize.lastMoveTime);
    return (time - maxEvent) > this._maxInteractiveWait;
  },

  // ----------
  // Function: getActiveTab
  // Returns the currently active tab as a <TabItem>
  getActiveTab: function UI_getActiveTab() {
    return this._activeTab;
  },

  // ----------
  // Function: _setActiveTab
  // Sets the currently active tab. The idea of a focused tab is useful
  // for keyboard navigation and returning to the last zoomed-in tab.
  // Hitting return/esc brings you to the focused tab, and using the
  // arrow keys lets you navigate between open tabs.
  //
  // Parameters:
  //  - Takes a <TabItem>
  _setActiveTab: function UI__setActiveTab(tabItem) {
    if (tabItem == this._activeTab)
      return;

    if (this._activeTab) {
      this._activeTab.makeDeactive();
      this._activeTab.removeSubscriber("close", this._onActiveTabClosed);
    }

    this._activeTab = tabItem;

    if (this._activeTab) {
      this._activeTab.addSubscriber("close", this._onActiveTabClosed);
      this._activeTab.makeActive();
    }
  },

  // ----------
  // Function: _onActiveTabClosed
  // Handles when the currently active tab gets closed.
  //
  // Parameters:
  //  - the <TabItem> that is closed
  _onActiveTabClosed: function UI__onActiveTabClosed(tabItem){
    if (UI._activeTab == tabItem)
      UI._setActiveTab(null);
  },

  // ----------
  // Function: setActive
  // Sets the active tab item or group item
  // Parameters:
  //
  // options
  //  dontSetActiveTabInGroup bool for not setting active tab in group
  setActive: function UI_setActive(item, options) {
    Utils.assert(item, "item must be given");

    if (item.isATabItem) {
      if (item.parent)
        GroupItems.setActiveGroupItem(item.parent);
      if (!options || !options.dontSetActiveTabInGroup)
        this._setActiveTab(item);
    } else {
      GroupItems.setActiveGroupItem(item);
      if (!options || !options.dontSetActiveTabInGroup) {
        let activeTab = item.getActiveTab();
        if (activeTab)
          this._setActiveTab(activeTab);
      }
    }
  },

  // ----------
  // Function: clearActiveTab
  // Sets the active tab to 'null'.
  clearActiveTab: function UI_clearActiveTab() {
    this._setActiveTab(null);
  },

  // ----------
  // Function: isTabViewVisible
  // Returns true if the TabView UI is currently shown.
  isTabViewVisible: function UI_isTabViewVisible() {
    return gTabViewDeck.selectedPanel == gTabViewFrame;
  },

  // ---------
  // Function: _initPageDirection
  // Initializes the page base direction
  _initPageDirection: function UI__initPageDirection() {
    let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].
                    getService(Ci.nsIXULChromeRegistry);
    let dir = chromeReg.isLocaleRTL("global");
    document.documentElement.setAttribute("dir", dir ? "rtl" : "ltr");
    this.rtl = dir;
  },

  // ----------
  // Function: showTabView
  // Shows TabView and hides the main browser UI.
  // Parameters:
  //   zoomOut - true for zoom out animation, false for nothing.
  showTabView: function UI_showTabView(zoomOut) {
    if (this.isTabViewVisible() || this._isChangingVisibility)
      return;

    this._isChangingVisibility = true;

    // store tab strip smooth scroll value and disable it.
    let tabStrip = gBrowser.tabContainer.mTabstrip;
    this._originalSmoothScroll = tabStrip.smoothScroll;
    tabStrip.smoothScroll = false;

    // initialize the direction of the page
    this._initPageDirection();

    var self = this;
    var currentTab = this._currentTab;

    this._reorderTabItemsOnShow.forEach(function(groupItem) {
      groupItem.reorderTabItemsBasedOnTabOrder();
    });
    this._reorderTabItemsOnShow = [];

#ifdef XP_WIN
    // Restore the full height when showing TabView
    gTabViewFrame.style.marginTop = "";
#endif
    gTabViewDeck.selectedPanel = gTabViewFrame;
    gWindow.TabsInTitlebar.allowedBy("tabview-open", false);
    gTabViewFrame.contentWindow.focus();

    gBrowser.updateTitlebar();
#ifdef XP_MACOSX
    this.setTitlebarColors(true);
#endif
    let event = document.createEvent("Events");
    event.initEvent("tabviewshown", true, false);

    Storage.saveVisibilityData(gWindow, "true");

    if (zoomOut && currentTab && currentTab._tabViewTabItem) {
      let item = currentTab._tabViewTabItem;
      // If there was a previous currentTab we want to animate
      // its thumbnail (canvas) for the zoom out.
      // Note that we start the animation on the chrome thread.

      // Zoom out!
      item.zoomOut(function() {
        if (!currentTab._tabViewTabItem) // if the tab's been destroyed
          item = null;

        self.setActive(item);

        self._resize(true);
        self._isChangingVisibility = false;
        dispatchEvent(event);

        // Flush pending updates
        GroupItems.flushAppTabUpdates();

        TabItems.resumePainting();
      });
    } else {
      if (!currentTab || !currentTab._tabViewTabItem)
        self.clearActiveTab();
      self._isChangingVisibility = false;
      dispatchEvent(event);

      // Flush pending updates
      GroupItems.flushAppTabUpdates();

      TabItems.resumePainting();
    }

    if (gTabView.firstUseExperienced)
      gTabView.enableSessionRestore();
  },

  // ----------
  // Function: hideTabView
  // Hides TabView and shows the main browser UI.
  hideTabView: function UI_hideTabView() {
    if (!this.isTabViewVisible() || this._isChangingVisibility)
      return;

    this._isChangingVisibility = true;

    // another tab might be select if user decides to stay on a page when
    // a onclose confirmation prompts.
    GroupItems.removeHiddenGroups();
    TabItems.pausePainting();

    this._reorderTabsOnHide.forEach(function(groupItem) {
      groupItem.reorderTabsBasedOnTabItemOrder();
    });
    this._reorderTabsOnHide = [];

#ifdef XP_WIN
    // Push the top of TabView frame to behind the tabbrowser, so glass can show
    // XXX bug 586679: avoid shrinking the iframe and squishing iframe contents
    // as well as avoiding the flash of black as we animate out
    gTabViewFrame.style.marginTop = gBrowser.boxObject.y + "px";
#endif
    gTabViewDeck.selectedPanel = gBrowserPanel;
    gWindow.TabsInTitlebar.allowedBy("tabview-open", true);
    gBrowser.selectedBrowser.focus();

    gBrowser.updateTitlebar();
    gBrowser.tabContainer.mTabstrip.smoothScroll = this._originalSmoothScroll;
#ifdef XP_MACOSX
    this.setTitlebarColors(false);
#endif
    Storage.saveVisibilityData(gWindow, "false");

    this._isChangingVisibility = false;

    let event = document.createEvent("Events");
    event.initEvent("tabviewhidden", true, false);
    dispatchEvent(event);
  },

#ifdef XP_MACOSX
  // ----------
  // Function: setTitlebarColors
  // Used on the Mac to make the title bar match the gradient in the rest of the
  // TabView UI.
  //
  // Parameters:
  //   colors - (bool or object) true for the special TabView color, false for
  //         the normal color, and an object with "active" and "inactive"
  //         properties to specify directly.
  setTitlebarColors: function UI_setTitlebarColors(colors) {
    // Mac Only
    var mainWindow = gWindow.document.getElementById("main-window");
    if (colors === true) {
      mainWindow.setAttribute("activetitlebarcolor", "#C4C4C4");
      mainWindow.setAttribute("inactivetitlebarcolor", "#EDEDED");
    } else if (colors && "active" in colors && "inactive" in colors) {
      mainWindow.setAttribute("activetitlebarcolor", colors.active);
      mainWindow.setAttribute("inactivetitlebarcolor", colors.inactive);
    } else {
      mainWindow.removeAttribute("activetitlebarcolor");
      mainWindow.removeAttribute("inactivetitlebarcolor");
    }
  },
#endif

  // ----------
  // Function: storageBusy
  // Pauses the storage activity that conflicts with sessionstore updates and 
  // private browsing mode switches. Calls can be nested. 
  storageBusy: function UI_storageBusy() {
    if (this._storageBusy)
      return;

    this._storageBusy = true;

    TabItems.pauseReconnecting();
    GroupItems.pauseAutoclose();
  },
  
  // ----------
  // Function: storageReady
  // Resumes the activity paused by storageBusy, and updates for any new group
  // information in sessionstore. Calls can be nested. 
  storageReady: function UI_storageReady() {
    if (!this._storageBusy)
      return;

    this._storageBusy = false;

    let hasGroupItemsData = GroupItems.load();
    if (!hasGroupItemsData)
      this.reset();

    TabItems.resumeReconnecting();
    GroupItems._updateTabBar();
    GroupItems.resumeAutoclose();
  },

  // ----------
  // Function: _addTabActionHandlers
  // Adds handlers to handle tab actions.
  _addTabActionHandlers: function UI__addTabActionHandlers() {
    var self = this;

    // session restore events
    function handleSSWindowStateBusy() {
      self.storageBusy();
    }
    
    function handleSSWindowStateReady() {
      self.storageReady();
    }
    
    gWindow.addEventListener("SSWindowStateBusy", handleSSWindowStateBusy, false);
    gWindow.addEventListener("SSWindowStateReady", handleSSWindowStateReady, false);

    this._cleanupFunctions.push(function() {
      gWindow.removeEventListener("SSWindowStateBusy", handleSSWindowStateBusy, false);
      gWindow.removeEventListener("SSWindowStateReady", handleSSWindowStateReady, false);
    });

    // Private Browsing:
    // When transitioning to PB, we exit Panorama if necessary (making note of the
    // fact that we were there so we can return after PB) and make sure we
    // don't reenter Panorama due to all of the session restore tab
    // manipulation (which otherwise we might). When transitioning away from
    // PB, we reenter Panorama if we had been there directly before PB.
    function pbObserver(subject, topic, data) {
      if (topic == "private-browsing") {
        // We could probably do this in private-browsing-change-granted, but
        // this seems like a nicer spot, right in the middle of the process.
        if (data == "enter") {
          // If we are in Tab View, exit. 
          self._privateBrowsing.wasInTabView = self.isTabViewVisible();
          if (self.isTabViewVisible())
            self.goToTab(gBrowser.selectedTab);
        }
      } else if (topic == "private-browsing-change-granted") {
        if (data == "enter" || data == "exit") {
          Search.hide();
          self._privateBrowsing.transitionMode = data;
        }
      } else if (topic == "private-browsing-transition-complete") {
        // We use .transitionMode here, as aData is empty.
        if (self._privateBrowsing.transitionMode == "exit" &&
            self._privateBrowsing.wasInTabView)
          self.showTabView(false);

        self._privateBrowsing.transitionMode = "";
      }
    }

    Services.obs.addObserver(pbObserver, "private-browsing", false);
    Services.obs.addObserver(pbObserver, "private-browsing-change-granted", false);
    Services.obs.addObserver(pbObserver, "private-browsing-transition-complete", false);

    this._cleanupFunctions.push(function() {
      Services.obs.removeObserver(pbObserver, "private-browsing");
      Services.obs.removeObserver(pbObserver, "private-browsing-change-granted");
      Services.obs.removeObserver(pbObserver, "private-browsing-transition-complete");
    });

    // TabOpen
    this._eventListeners.open = function (event) {
      let tab = event.target;

      // if it's an app tab, add it to all the group items
      if (tab.pinned)
        GroupItems.addAppTab(tab);
      else if (self.isTabViewVisible() && !self._storageBusyCount)
        self._lastOpenedTab = tab;
    };
    
    // TabClose
    this._eventListeners.close = function (event) {
      let tab = event.target;

      // if it's an app tab, remove it from all the group items
      if (tab.pinned)
        GroupItems.removeAppTab(tab);
        
      if (self.isTabViewVisible()) {
        // just closed the selected tab in the TabView interface.
        if (self._currentTab == tab)
          self._closedSelectedTabInTabView = true;
      } else {
        // If we're currently in the process of entering private browsing,
        // we don't want to go to the Tab View UI. 
        if (self._storageBusy)
          return;

        // if not closing the last tab
        if (gBrowser.tabs.length > 1) {
          // Don't return to TabView if there are any app tabs
          for (let a = 0; a < gBrowser._numPinnedTabs; a++) {
            if (Utils.isValidXULTab(gBrowser.tabs[a]))
              return;
          }

          let groupItem = GroupItems.getActiveGroupItem();

          // 1) Only go back to the TabView tab when there you close the last
          // tab of a groupItem.
          let closingLastOfGroup = (groupItem && 
              groupItem._children.length == 1 && 
              groupItem._children[0].tab == tab);

          // 2) When a blank tab is active while restoring a closed tab the
          // blank tab gets removed. The active group is not closed as this is
          // where the restored tab goes. So do not show the TabView.
          let tabItem = tab && tab._tabViewTabItem;
          let closingBlankTabAfterRestore =
            (tabItem && tabItem.isRemovedAfterRestore);

          if (closingLastOfGroup && !closingBlankTabAfterRestore) {
            // for the tab focus event to pick up.
            self._closedLastVisibleTab = true;
            self.showTabView();
          }
        }
      }
    };

    // TabMove
    this._eventListeners.move = function (event) {
      let tab = event.target;

      if (GroupItems.groupItems.length > 0) {
        if (tab.pinned) {
          if (gBrowser._numPinnedTabs > 1)
            GroupItems.arrangeAppTab(tab);
        } else {
          let activeGroupItem = GroupItems.getActiveGroupItem();
          if (activeGroupItem)
            self.setReorderTabItemsOnShow(activeGroupItem);
        }
      }
    };

    // TabSelect
    this._eventListeners.select = function (event) {
      self.onTabSelect(event.target);
    };

    // TabPinned
    this._eventListeners.pinned = function (event) {
      let tab = event.target;

      TabItems.handleTabPin(tab);
      GroupItems.addAppTab(tab);
    };

    // TabUnpinned
    this._eventListeners.unpinned = function (event) {
      let tab = event.target;

      TabItems.handleTabUnpin(tab);
      GroupItems.removeAppTab(tab);

      let groupItem = tab._tabViewTabItem.parent;
      if (groupItem)
        self.setReorderTabItemsOnShow(groupItem);
    };

    // Actually register the above handlers
    for (let name in this._eventListeners)
      AllTabs.register(name, this._eventListeners[name]);
  },

  // ----------
  // Function: _removeTabActionHandlers
  // Removes handlers to handle tab actions.
  _removeTabActionHandlers: function UI__removeTabActionHandlers() {
    for (let name in this._eventListeners)
      AllTabs.unregister(name, this._eventListeners[name]);
  },

  // ----------
  // Function: goToTab
  // Selects the given xul:tab in the browser.
  goToTab: function UI_goToTab(xulTab) {
    // If it's not focused, the onFocus listener would handle it.
    if (gBrowser.selectedTab == xulTab)
      this.onTabSelect(xulTab);
    else
      gBrowser.selectedTab = xulTab;
  },

  // ----------
  // Function: onTabSelect
  // Called when the user switches from one tab to another outside of the TabView UI.
  onTabSelect: function UI_onTabSelect(tab) {
    this._currentTab = tab;

    if (this.isTabViewVisible()) {
      // We want to zoom in if:
      // 1) we didn't just restore a tab via Ctrl+Shift+T
      // 2) we're not in the middle of switching from/to private browsing
      // 3) the currently selected tab is the last created tab and has a tabItem
      if (!this.restoredClosedTab && !this._privateBrowsing.transitionMode &&
          this._lastOpenedTab == tab && tab._tabViewTabItem) {
        tab._tabViewTabItem.zoomIn(true);
        this._lastOpenedTab = null;
        return;
      }
      if (this._closedLastVisibleTab ||
          (this._closedSelectedTabInTabView && !this.closedLastTabInTabView) ||
          this.restoredClosedTab) {
        if (this.restoredClosedTab) {
          // when the tab view UI is being displayed, update the thumb for the 
          // restored closed tab after the page load
          tab.linkedBrowser.addEventListener("load", function onLoad(event) {
            tab.linkedBrowser.removeEventListener("load", onLoad, true);
            TabItems._update(tab);
          }, true);
        }
        this._closedLastVisibleTab = false;
        this._closedSelectedTabInTabView = false;
        this.closedLastTabInTabView = false;
        this.restoredClosedTab = false;
        return;
      }
    }
    // reset these vars, just in case.
    this._closedLastVisibleTab = false;
    this._closedSelectedTabInTabView = false;
    this.closedLastTabInTabView = false;
    this.restoredClosedTab = false;
    this._lastOpenedTab = null;

    // if TabView is visible but we didn't just close the last tab or
    // selected tab, show chrome.
    if (this.isTabViewVisible()) {
      // Unhide the group of the tab the user is activating.
      if (tab && tab._tabViewTabItem && tab._tabViewTabItem.parent &&
          tab._tabViewTabItem.parent.hidden)
        tab._tabViewTabItem.parent._unhide({immediately: true});

      this.hideTabView();
    }

    // another tab might be selected when hideTabView() is invoked so a
    // validation is needed.
    if (this._currentTab != tab)
      return;

    let newItem = null;
    // update the tab bar for the new tab's group
    if (tab && tab._tabViewTabItem) {
      if (!TabItems.reconnectingPaused()) {
        newItem = tab._tabViewTabItem;
        GroupItems.updateActiveGroupItemAndTabBar(newItem);
      }
    } else {
      // No tabItem; must be an app tab. Base the tab bar on the current group.
      // If no current group, figure it out based on what's already in the tab
      // bar.
      if (!GroupItems.getActiveGroupItem()) {
        for (let a = 0; a < gBrowser.tabs.length; a++) {
          let theTab = gBrowser.tabs[a];
          if (!theTab.pinned) {
            let tabItem = theTab._tabViewTabItem;
            this.setActive(tabItem.parent);
            break;
          }
        }
      }

      if (GroupItems.getActiveGroupItem())
        GroupItems._updateTabBar();
    }
  },

  // ----------
  // Function: _onDOMWillOpenModalDialog
  // Called when a web page is about to show a modal dialog.
  _onDOMWillOpenModalDialog: function UI__onDOMWillOpenModalDialog(cx) {
    if (!this.isTabViewVisible())
      return;

    let index = gBrowser.browsers.indexOf(cx.target);
    if (index == -1)
      return;

    let tab = gBrowser.tabs[index];

    // When TabView is visible, we need to call onTabSelect to make sure that
    // TabView is hidden and that the correct group is activated. When a modal
    // dialog is shown for currently selected tab the onTabSelect event handler
    // is not called, so we need to do it.
    if (gBrowser.selectedTab == tab && this._currentTab == tab)
      this.onTabSelect(tab);
  },

  // ----------
  // Function: setReorderTabsOnHide
  // Sets the groupItem which the tab items' tabs should be re-ordered when
  // switching to the main browser UI.
  // Parameters:
  //   groupItem - the groupItem which would be used for re-ordering tabs.
  setReorderTabsOnHide: function UI_setReorderTabsOnHide(groupItem) {
    if (this.isTabViewVisible()) {
      var index = this._reorderTabsOnHide.indexOf(groupItem);
      if (index == -1)
        this._reorderTabsOnHide.push(groupItem);
    }
  },

  // ----------
  // Function: setReorderTabItemsOnShow
  // Sets the groupItem which the tab items should be re-ordered when
  // switching to the tab view UI.
  // Parameters:
  //   groupItem - the groupItem which would be used for re-ordering tab items.
  setReorderTabItemsOnShow: function UI_setReorderTabItemsOnShow(groupItem) {
    if (!this.isTabViewVisible()) {
      var index = this._reorderTabItemsOnShow.indexOf(groupItem);
      if (index == -1)
        this._reorderTabItemsOnShow.push(groupItem);
    }
  },
  
  // ----------
  updateTabButton: function UI_updateTabButton() {
    let exitButton = document.getElementById("exit-button");
    let numberOfGroups = GroupItems.groupItems.length;

    exitButton.setAttribute("groups", numberOfGroups);
    gTabView.updateGroupNumberBroadcaster(numberOfGroups);
  },

  // ----------
  // Function: getClosestTab
  // Convenience function to get the next tab closest to the entered position
  getClosestTab: function UI_getClosestTab(tabCenter) {
    let cl = null;
    let clDist;
    TabItems.getItems().forEach(function (item) {
      if (!item.parent || item.parent.hidden)
        return;
      let testDist = tabCenter.distance(item.bounds.center());
      if (cl==null || testDist < clDist) {
        cl = item;
        clDist = testDist;
      }
    });
    return cl;
  },

  // ----------
  // Function: _setupBrowserKeys
  // Sets up the allowed browser keys using key elements.
  _setupBrowserKeys: function UI__setupKeyWhiteList() {
    let keys = {};

    [
#ifdef XP_UNIX
      "quitApplication",
#else
      "redo",
#endif
#ifdef XP_MACOSX
      "preferencesCmdMac", "minimizeWindow", "hideThisAppCmdMac",
#endif
      "newNavigator", "newNavigatorTab", "undo", "cut", "copy", "paste", 
      "selectAll", "find"
    ].forEach(function(key) {
      let element = gWindow.document.getElementById("key_" + key);
      let code = element.getAttribute("key").toLocaleLowerCase().charCodeAt(0);
      keys[code] = key;
    });
    this._browserKeys = keys;

    keys = {};
    // The lower case letters are passed to processBrowserKeys() even with shift 
    // key when stimulating a key press using EventUtils.synthesizeKey() so need 
    // to handle both upper and lower cases here.
    [
#ifdef XP_UNIX
      "redo",
#endif
#ifdef XP_MACOSX
      "fullScreen",
#endif
      "closeWindow", "tabview", "undoCloseTab", "undoCloseWindow",
      "privatebrowsing"
    ].forEach(function(key) {
      let element = gWindow.document.getElementById("key_" + key);
      let code = element.getAttribute("key").toLocaleLowerCase().charCodeAt(0);
      keys[code] = key;
    });
    this._browserKeysWithShift = keys;
  },

  // ----------
  // Function: _setTabViewFrameKeyHandlers
  // Sets up the key handlers for navigating between tabs within the TabView UI.
  _setTabViewFrameKeyHandlers: function UI__setTabViewFrameKeyHandlers() {
    let self = this;

    this._setupBrowserKeys();

    iQ(window).keyup(function(event) {
      if (!event.metaKey)
        Keys.meta = false;
    });

    iQ(window).keypress(function(event) {
      if (event.metaKey)
        Keys.meta = true;

      function processBrowserKeys(evt) {
        // let any keys with alt to pass through
        if (evt.altKey)
          return;

#ifdef XP_MACOSX
        if (evt.metaKey) {
#else
        if (evt.ctrlKey) {
#endif
          let preventDefault = true;
          if (evt.shiftKey) {
            // when a user presses ctrl+shift+key, upper case letter charCode 
            // is passed to processBrowserKeys() so converting back to lower 
            // case charCode before doing the check
            let lowercaseCharCode =
              String.fromCharCode(evt.charCode).toLocaleLowerCase().charCodeAt(0);
            if (lowercaseCharCode in self._browserKeysWithShift) {
              let key = self._browserKeysWithShift[lowercaseCharCode];
              if (key == "tabview")
                self.exit();
              else
                preventDefault = false;
            }
          } else {
            if (evt.charCode in self._browserKeys) {
              let key = self._browserKeys[evt.charCode];
              if (key == "find")
                self.enableSearch();
              else
                preventDefault = false;
            }
          }
          if (preventDefault) {
            evt.stopPropagation();
            evt.preventDefault();
          }
        }
      }
      if ((iQ(":focus").length > 0 && iQ(":focus")[0].nodeName == "INPUT") ||
          Search.isEnabled() || self.ignoreKeypressForSearch) {
        self.ignoreKeypressForSearch = false;
        processBrowserKeys(event);
        return;
      }

      function getClosestTabBy(norm) {
        if (!self.getActiveTab())
          return null;

        let activeTab = self.getActiveTab();
        let activeTabGroup = activeTab.parent;
        let myCenter = activeTab.bounds.center();
        let match;

        TabItems.getItems().forEach(function (item) {
          if (!item.parent.hidden &&
              (!activeTabGroup.expanded || activeTabGroup.id == item.parent.id)) {
            let itemCenter = item.bounds.center();

            if (norm(itemCenter, myCenter)) {
              let itemDist = myCenter.distance(itemCenter);
              if (!match || match[0] > itemDist)
                match = [itemDist, item];
            }
          }
        });

        return match && match[1];
      }

      let preventDefault = true;
      let activeTab;
      let activeGroupItem;
      let norm = null;
      switch (event.keyCode) {
        case KeyEvent.DOM_VK_RIGHT:
          norm = function(a, me){return a.x > me.x};
          break;
        case KeyEvent.DOM_VK_LEFT:
          norm = function(a, me){return a.x < me.x};
          break;
        case KeyEvent.DOM_VK_DOWN:
          norm = function(a, me){return a.y > me.y};
          break;
        case KeyEvent.DOM_VK_UP:
          norm = function(a, me){return a.y < me.y}
          break;
      }

      if (norm != null) {
        let nextTab = getClosestTabBy(norm);
        if (nextTab) {
          if (nextTab.isStacked && !nextTab.parent.expanded)
            nextTab = nextTab.parent.getChild(0);
          self.setActive(nextTab);
        }
      } else {
        switch(event.keyCode) {
          case KeyEvent.DOM_VK_ESCAPE:
            activeGroupItem = GroupItems.getActiveGroupItem();
            if (activeGroupItem && activeGroupItem.expanded)
              activeGroupItem.collapse();
            else
              self.exit();
            break;
          case KeyEvent.DOM_VK_RETURN:
          case KeyEvent.DOM_VK_ENTER:
            activeGroupItem = GroupItems.getActiveGroupItem();
            if (activeGroupItem) {
              activeTab = self.getActiveTab();

              if (!activeTab || activeTab.parent != activeGroupItem)
                activeTab = activeGroupItem.getActiveTab();

              if (activeTab)
                activeTab.zoomIn();
              else
                activeGroupItem.newTab();
            }
            break;
          case KeyEvent.DOM_VK_TAB:
            // tab/shift + tab to go to the next tab.
            activeTab = self.getActiveTab();
            if (activeTab) {
              let tabItems = (activeTab.parent ? activeTab.parent.getChildren() :
                              [activeTab]);
              let length = tabItems.length;
              let currentIndex = tabItems.indexOf(activeTab);

              if (length > 1) {
                let newIndex;
                if (event.shiftKey) {
                  if (currentIndex == 0)
                    newIndex = (length - 1);
                  else
                    newIndex = (currentIndex - 1);
                } else {
                  if (currentIndex == (length - 1))
                    newIndex = 0;
                  else
                    newIndex = (currentIndex + 1);
                }
                self.setActive(tabItems[newIndex]);
              }
            }
            break;
          default:
            processBrowserKeys(event);
            preventDefault = false;
        }
        if (preventDefault) {
          event.stopPropagation();
          event.preventDefault();
        }
      }
    });
  },

  // ----------
  // Function: enableSearch
  // Enables the search feature.
  enableSearch: function UI_enableSearch() {
    if (!Search.isEnabled()) {
      Search.ensureShown();
      Search.switchToInMode();
    }
  },

  // ----------
  // Function: _createGroupItemOnDrag
  // Called in response to a mousedown in empty space in the TabView UI;
  // creates a new groupItem based on the user's drag.
  _createGroupItemOnDrag: function UI__createGroupItemOnDrag(e) {
    const minSize = 60;
    const minMinSize = 15;

    let lastActiveGroupItem = GroupItems.getActiveGroupItem();

    var startPos = { x: e.clientX, y: e.clientY };
    var phantom = iQ("<div>")
      .addClass("groupItem phantom activeGroupItem dragRegion")
      .css({
        position: "absolute",
        zIndex: -1,
        cursor: "default"
      })
      .appendTo("body");

    var item = { // a faux-Item
      container: phantom,
      isAFauxItem: true,
      bounds: {},
      getBounds: function FauxItem_getBounds() {
        return this.container.bounds();
      },
      setBounds: function FauxItem_setBounds(bounds) {
        this.container.css(bounds);
      },
      setZ: function FauxItem_setZ(z) {
        // don't set a z-index because we want to force it to be low.
      },
      setOpacity: function FauxItem_setOpacity(opacity) {
        this.container.css("opacity", opacity);
      },
      // we don't need to pushAway the phantom item at the end, because
      // when we create a new GroupItem, it'll do the actual pushAway.
      pushAway: function () {},
    };
    item.setBounds(new Rect(startPos.y, startPos.x, 0, 0));

    var dragOutInfo = new Drag(item, e);

    function updateSize(e) {
      var box = new Rect();
      box.left = Math.min(startPos.x, e.clientX);
      box.right = Math.max(startPos.x, e.clientX);
      box.top = Math.min(startPos.y, e.clientY);
      box.bottom = Math.max(startPos.y, e.clientY);
      item.setBounds(box);

      // compute the stationaryCorner
      var stationaryCorner = "";

      if (startPos.y == box.top)
        stationaryCorner += "top";
      else
        stationaryCorner += "bottom";

      if (startPos.x == box.left)
        stationaryCorner += "left";
      else
        stationaryCorner += "right";

      dragOutInfo.snap(stationaryCorner, false, false); // null for ui, which we don't use anyway.

      box = item.getBounds();
      if (box.width > minMinSize && box.height > minMinSize &&
         (box.width > minSize || box.height > minSize))
        item.setOpacity(1);
      else
        item.setOpacity(0.7);

      e.preventDefault();
    }

    let self = this;
    function collapse() {
      let center = phantom.bounds().center();
      phantom.animate({
        width: 0,
        height: 0,
        top: center.y,
        left: center.x
      }, {
        duration: 300,
        complete: function() {
          phantom.remove();
        }
      });
      self.setActive(lastActiveGroupItem);
    }

    function finalize(e) {
      iQ(window).unbind("mousemove", updateSize);
      item.container.removeClass("dragRegion");
      dragOutInfo.stop();
      let box = item.getBounds();
      if (box.width > minMinSize && box.height > minMinSize &&
         (box.width > minSize || box.height > minSize)) {
        let opts = {bounds: item.getBounds(), focusTitle: true};
        let groupItem = new GroupItem([], opts);
        self.setActive(groupItem);
        phantom.remove();
        dragOutInfo = null;
        gTabView.firstUseExperienced = true;
      } else {
        collapse();
      }
    }

    iQ(window).mousemove(updateSize)
    iQ(gWindow).one("mouseup", finalize);
    e.preventDefault();
    return false;
  },

  // ----------
  // Function: _resize
  // Update the TabView UI contents in response to a window size change.
  // Won't do anything if it doesn't deem the resize necessary.
  // Parameters:
  //   force - true to update even when "unnecessary"; default false
  _resize: function UI__resize(force) {
    if (!this._pageBounds)
      return;

    // Here are reasons why we *won't* resize:
    // 1. Panorama isn't visible (in which case we will resize when we do display)
    // 2. the screen dimensions haven't changed
    // 3. everything on the screen fits and nothing feels cramped
    if (!force && !this.isTabViewVisible())
      return;

    let oldPageBounds = new Rect(this._pageBounds);
    let newPageBounds = Items.getPageBounds();
    if (newPageBounds.equals(oldPageBounds))
      return;

    if (!this.shouldResizeItems())
      return;

    var items = Items.getTopLevelItems();

    // compute itemBounds: the union of all the top-level items' bounds.
    var itemBounds = new Rect(this._pageBounds);
    // We start with pageBounds so that we respect the empty space the user
    // has left on the page.
    itemBounds.width = 1;
    itemBounds.height = 1;
    items.forEach(function(item) {
      var bounds = item.getBounds();
      itemBounds = (itemBounds ? itemBounds.union(bounds) : new Rect(bounds));
    });

    if (newPageBounds.width < this._pageBounds.width &&
        newPageBounds.width > itemBounds.width)
      newPageBounds.width = this._pageBounds.width;

    if (newPageBounds.height < this._pageBounds.height &&
        newPageBounds.height > itemBounds.height)
      newPageBounds.height = this._pageBounds.height;

    var wScale;
    var hScale;
    if (Math.abs(newPageBounds.width - this._pageBounds.width)
         > Math.abs(newPageBounds.height - this._pageBounds.height)) {
      wScale = newPageBounds.width / this._pageBounds.width;
      hScale = newPageBounds.height / itemBounds.height;
    } else {
      wScale = newPageBounds.width / itemBounds.width;
      hScale = newPageBounds.height / this._pageBounds.height;
    }

    var scale = Math.min(hScale, wScale);
    var self = this;
    var pairs = [];
    items.forEach(function(item) {
      var bounds = item.getBounds();
      bounds.left += (UI.rtl ? -1 : 1) * (newPageBounds.left - self._pageBounds.left);
      bounds.left *= scale;
      bounds.width *= scale;

      bounds.top += newPageBounds.top - self._pageBounds.top;
      bounds.top *= scale;
      bounds.height *= scale;

      pairs.push({
        item: item,
        bounds: bounds
      });
    });

    Items.unsquish(pairs);

    pairs.forEach(function(pair) {
      pair.item.setBounds(pair.bounds, true);
      pair.item.snap();
    });

    this._pageBounds = Items.getPageBounds();
    this._save();
  },
  
  // ----------
  // Function: shouldResizeItems
  // Returns whether we should resize the items on the screen, based on whether
  // the top-level items fit in the screen or not and whether they feel
  // "cramped" or not.
  // These computations may be done using cached values. The cache can be
  // cleared with UI.clearShouldResizeItems().
  shouldResizeItems: function UI_shouldResizeItems() {
    let newPageBounds = Items.getPageBounds();
    
    // If we don't have cached cached values...
    if (this._minimalRect === undefined || this._feelsCramped === undefined) {

      // Loop through every top-level Item for two operations:
      // 1. check if it is feeling "cramped" due to squishing (a technical term),
      // 2. union its bounds with the minimalRect
      let feelsCramped = false;
      let minimalRect = new Rect(0, 0, 1, 1);
      
      Items.getTopLevelItems()
        .forEach(function UI_shouldResizeItems_checkItem(item) {
          let bounds = new Rect(item.getBounds());
          feelsCramped = feelsCramped || (item.userSize &&
            (item.userSize.x > bounds.width || item.userSize.y > bounds.height));
          bounds.inset(-Trenches.defaultRadius, -Trenches.defaultRadius);
          minimalRect = minimalRect.union(bounds);
        });
      
      // ensure the minimalRect extends to, but not beyond, the origin
      minimalRect.left = 0;
      minimalRect.top  = 0;
  
      this._minimalRect = minimalRect;
      this._feelsCramped = feelsCramped;
    }

    return this._minimalRect.width > newPageBounds.width ||
      this._minimalRect.height > newPageBounds.height ||
      this._feelsCramped;
  },
  
  // ----------
  // Function: clearShouldResizeItems
  // Clear the cache of whether we should resize the items on the Panorama
  // screen, forcing a recomputation on the next UI.shouldResizeItems()
  // call.
  clearShouldResizeItems: function UI_clearShouldResizeItems() {
    delete this._minimalRect;
    delete this._feelsCramped;
  },

  // ----------
  // Function: exit
  // Exits TabView UI.
  exit: function UI_exit() {
    let self = this;
    let zoomedIn = false;

    if (Search.isEnabled()) {
      let matcher = Search.createSearchTabMatcher();
      let matches = matcher.matched();

      if (matches.length > 0) {
        matches[0].zoomIn();
        zoomedIn = true;
      }
      Search.hide();
    }

    if (!zoomedIn) {
      let unhiddenGroups = GroupItems.groupItems.filter(function(groupItem) {
        return (!groupItem.hidden && groupItem.getChildren().length > 0);
      });
      // no pinned tabs and no visible groups: open a new group. open a blank
      // tab and return
      if (!unhiddenGroups.length) {
        let emptyGroups = GroupItems.groupItems.filter(function (groupItem) {
          return (!groupItem.hidden && !groupItem.getChildren().length);
        });
        let group = (emptyGroups.length ? emptyGroups[0] : GroupItems.newGroup());
        if (!gBrowser._numPinnedTabs) {
          group.newTab(null, { closedLastTab: true });
          return;
        }
      }

      // If there's an active TabItem, zoom into it. If not (for instance when the
      // selected tab is an app tab), just go there.
      let activeTabItem = this.getActiveTab();
      if (!activeTabItem) {
        let tabItem = gBrowser.selectedTab._tabViewTabItem;
        if (tabItem) {
          if (!tabItem.parent || !tabItem.parent.hidden) {
            activeTabItem = tabItem;
          } else { // set active tab item if there is at least one unhidden group
            if (unhiddenGroups.length > 0)
              activeTabItem = unhiddenGroups[0].getActiveTab();
          }
        }
      }

      if (activeTabItem) {
        activeTabItem.zoomIn();
      } else {
        if (gBrowser._numPinnedTabs > 0) {
          if (gBrowser.selectedTab.pinned) {
            self.goToTab(gBrowser.selectedTab);
          } else {
            Array.some(gBrowser.tabs, function(tab) {
              if (tab.pinned) {
                self.goToTab(tab);
                return true;
              }
              return false
            });
          }
        }
      }
    }
  },

  // ----------
  // Function: storageSanity
  // Given storage data for this object, returns true if it looks valid.
  _storageSanity: function UI__storageSanity(data) {
    if (Utils.isEmptyObject(data))
      return true;

    if (!Utils.isRect(data.pageBounds)) {
      Utils.log("UI.storageSanity: bad pageBounds", data.pageBounds);
      data.pageBounds = null;
      return false;
    }

    return true;
  },

  // ----------
  // Function: _save
  // Saves the data for this object to persistent storage
  _save: function UI__save() {
    if (!this._frameInitialized)
      return;

    var data = {
      pageBounds: this._pageBounds
    };

    if (this._storageSanity(data))
      Storage.saveUIData(gWindow, data);
  },

  // ----------
  // Function: _saveAll
  // Saves all data associated with TabView.
  _saveAll: function UI__saveAll() {
    this._save();
    GroupItems.saveAll();
    TabItems.saveAll();
  },

  // ----------
  // Function: notifySessionRestoreEnabled
  // Notify the user that session restore has been automatically enabled
  // by showing a banner that expects no user interaction. It fades out after
  // some seconds.
  notifySessionRestoreEnabled: function UI_notifySessionRestoreEnabled() {
    let brandBundle = gWindow.document.getElementById("bundle_brand");
    let brandShortName = brandBundle.getString("brandShortName");
    let notificationText = tabviewBundle.formatStringFromName(
      "tabview.notification.sessionStore", [brandShortName], 1);

    let banner = iQ("<div>")
      .text(notificationText)
      .addClass("banner")
      .appendTo("body");

    let onFadeOut = function () {
      banner.remove();
    };

    let onFadeIn = function () {
      setTimeout(function () {
        banner.animate({opacity: 0}, {duration: 1500, complete: onFadeOut});
      }, 5000);
    };

    banner.animate({opacity: 0.7}, {duration: 1500, complete: onFadeIn});
  }
};

// ----------
UI.init();
