/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is ui.js.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Ian Gilman <ian@iangilman.com>
 * Aza Raskin <aza@mozilla.com>
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>
 * Ehsan Akhgari <ehsan@mozilla.com>
 * Raymond Lee <raymond@appcoast.com>
 * Sean Dunn <seanedunn@yahoo.com>
 * Tim Taubert <tim.taubert@gmx.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// **********
// Title: ui.js

let Keys = { meta: false };

// ##########
// Class: UI
// Singleton top-level UI manager.
let UI = {
  // Constant: DBLCLICK_INTERVAL
  // Defines the maximum time (in ms) between two clicks for it to count as
  // a double click.
  DBLCLICK_INTERVAL: 500,

  // Constant: DBLCLICK_OFFSET
  // Defines the maximum offset (in pixels) between two clicks for it to count as
  // a double click.
  DBLCLICK_OFFSET: 5,

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

  // Variable: _lastClick
  // Keeps track of the time of last click event to detect double click.
  // Used to create tabs on double-click since we cannot attach 'dblclick'
  _lastClick: 0,

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
  
  // Variable: _storageBusyCount
  // Used to keep track of how many calls to storageBusy vs storageReady.
  _storageBusyCount: 0,

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
      let data = Storage.readUIData(gWindow);
      this._storageSanity(data);
      this._pageBounds = data.pageBounds;

      // ___ hook into the browser
      gWindow.addEventListener("tabviewshow", function() {
        self.showTabView(true);
      }, false);

      // ___ currentTab
      this._currentTab = gBrowser.selectedTab;

      // ___ exit button
      iQ("#exit-button").click(function() {
        self.exit();
        self.blurAll();
      });

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
        if (e.originalTarget.id == "content") {
          // Create an orphan tab on double click
          if (Date.now() - self._lastClick <= self.DBLCLICK_INTERVAL && 
              (self._lastClickPositions.x - self.DBLCLICK_OFFSET) <= e.clientX &&
              (self._lastClickPositions.x + self.DBLCLICK_OFFSET) >= e.clientX &&
              (self._lastClickPositions.y - self.DBLCLICK_OFFSET) <= e.clientY &&
              (self._lastClickPositions.y + self.DBLCLICK_OFFSET) >= e.clientY) {
            GroupItems.setActiveGroupItem(null);
            TabItems.creatingNewOrphanTab = true;

            let newTab = 
              gBrowser.loadOneTab("about:blank", { inBackground: true });

            let box = 
              new Rect(e.clientX - Math.floor(TabItems.tabWidth/2),
                       e.clientY - Math.floor(TabItems.tabHeight/2),
                       TabItems.tabWidth, TabItems.tabHeight);
            newTab._tabViewTabItem.setBounds(box, true);
            newTab._tabViewTabItem.pushAway(true);
            GroupItems.setActiveOrphanTab(newTab._tabViewTabItem);

            TabItems.creatingNewOrphanTab = false;
            newTab._tabViewTabItem.zoomIn(true);

            self._lastClick = 0;
            self._lastClickPositions = null;
          } else {
            self._lastClick = Date.now();
            self._lastClickPositions = new Point(e.clientX, e.clientY);
            self._createGroupItemOnDrag(e);
          }
        }
      });

      iQ(window).bind("unload", function() {
        self.uninit();
      });

      gWindow.addEventListener("tabviewhide", function() {
        self.exit();
      }, false);

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

      // if first time in Panorama or no group data:
      let firstTime = true;
      if (gPrefBranch.prefHasUserValue("experienced_first_run"))
        firstTime = !gPrefBranch.getBoolPref("experienced_first_run");

      if (firstTime || !hasGroupItemsData)
        this.reset(firstTime);

      // ___ resizing
      if (this._pageBounds)
        this._resize(true);
      else
        this._pageBounds = Items.getPageBounds();

      iQ(window).resize(function() {
        self._resize();
      });

      // ___ setup observer to save canvas images
      function quitObserver(subject, topic, data) {
        if (topic == "quit-application-requested") {
          if (self.isTabViewVisible())
            GroupItems.removeHiddenGroups();

          TabItems.saveAll(true);
          self._save();
        }
      }
      Services.obs.addObserver(
        quitObserver, "quit-application-requested", false);
      this._cleanupFunctions.push(function() {
        Services.obs.removeObserver(quitObserver, "quit-application-requested");
      });

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

  uninit: function UI_uninit() {
    // call our cleanup functions
    this._cleanupFunctions.forEach(function(func) {
      func();
    });
    this._cleanupFunctions = [];

    // additional clean up
    TabItems.uninit();
    GroupItems.uninit();
    Storage.uninit();

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
  // and, if firstTime == true, add the welcome video/tab
  reset: function UI_reset(firstTime) {
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
    GroupItems.setActiveGroupItem(groupItem);

    if (firstTime) {
      gPrefBranch.setBoolPref("experienced_first_run", true);
      // ensure that the first run pref is flushed to the file, in case a crash 
      // or force quit happens before the pref gets flushed automatically.
      Services.prefs.savePrefFile(null);

      /* DISABLED BY BUG 626754. To be reenabled via bug 626926.
      let url = gPrefBranch.getCharPref("welcome_url");
      let newTab = gBrowser.loadOneTab(url, {inBackground: true});
      let newTabItem = newTab._tabViewTabItem;
      let parent = newTabItem.parent;
      Utils.assert(parent, "should have a parent");

      newTabItem.parent.remove(newTabItem);
      let aspect = TabItems.tabHeight / TabItems.tabWidth;
      let welcomeBounds = new Rect(UI.rtl ? pageBounds.left : box.right, box.top,
                                   welcomeWidth, welcomeWidth * aspect);
      newTabItem.setBounds(welcomeBounds, true);

      // Remove the newly created welcome-tab from the tab bar
      if (!this.isTabViewVisible())
        GroupItems._updateTabBar();
      */
    }
  },

  // Function: blurAll
  // Blurs any currently focused element
  //
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
  //
  getActiveTab: function UI_getActiveTab() {
    return this._activeTab;
  },

  // ----------
  // Function: setActiveTab
  // Sets the currently active tab. The idea of a focused tab is useful
  // for keyboard navigation and returning to the last zoomed-in tab.
  // Hitting return/esc brings you to the focused tab, and using the
  // arrow keys lets you navigate between open tabs.
  //
  // Parameters:
  //  - Takes a <TabItem>
  setActiveTab: function UI_setActiveTab(tabItem) {
    if (tabItem == this._activeTab)
      return;

    if (this._activeTab) {
      this._activeTab.makeDeactive();
      this._activeTab.removeSubscriber(this, "close");
    }
    this._activeTab = tabItem;

    if (this._activeTab) {
      let self = this;
      this._activeTab.addSubscriber(this, "close", function(closedTabItem) {
        if (self._activeTab == closedTabItem)
          self._activeTab = null;
      });

      this._activeTab.makeActive();
    }
  },

  // ----------
  // Function: isTabViewVisible
  // Returns true if the TabView UI is currently shown.
  isTabViewVisible: function UI_isTabViewVisible() {
    return gTabViewDeck.selectedIndex == 1;
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
    if (this.isTabViewVisible())
      return;

    // initialize the direction of the page
    this._initPageDirection();

    var self = this;
    var currentTab = this._currentTab;
    var item = null;

    this._reorderTabItemsOnShow.forEach(function(groupItem) {
      groupItem.reorderTabItemsBasedOnTabOrder();
    });
    this._reorderTabItemsOnShow = [];

#ifdef XP_WIN
    // Restore the full height when showing TabView
    gTabViewFrame.style.marginTop = "";
#endif
    gTabViewDeck.selectedIndex = 1;
    gWindow.TabsInTitlebar.allowedBy("tabview-open", false);
    gTabViewFrame.contentWindow.focus();

    gBrowser.updateTitlebar();
#ifdef XP_MACOSX
    this.setTitlebarColors(true);
#endif
    let event = document.createEvent("Events");
    event.initEvent("tabviewshown", true, false);

    if (zoomOut && currentTab && currentTab._tabViewTabItem) {
      item = currentTab._tabViewTabItem;
      // If there was a previous currentTab we want to animate
      // its thumbnail (canvas) for the zoom out.
      // Note that we start the animation on the chrome thread.

      // Zoom out!
      item.zoomOut(function() {
        if (!currentTab._tabViewTabItem) // if the tab's been destroyed
          item = null;

        self.setActiveTab(item);

        if (item.parent) {
          var activeGroupItem = GroupItems.getActiveGroupItem();
          if (activeGroupItem)
            activeGroupItem.setTopChild(item);
        }

        self._resize(true);
        dispatchEvent(event);

        // Flush pending updates
        GroupItems.flushAppTabUpdates();

        TabItems.resumePainting();
      });
    } else {
      if (currentTab && currentTab._tabViewTabItem)
        currentTab._tabViewTabItem.setZoomPrep(false);

      self.setActiveTab(null);
      dispatchEvent(event);

      // Flush pending updates
      GroupItems.flushAppTabUpdates();

      TabItems.resumePainting();
    }

    Storage.saveVisibilityData(gWindow, "true");
  },

  // ----------
  // Function: hideTabView
  // Hides TabView and shows the main browser UI.
  hideTabView: function UI_hideTabView() {
    if (!this.isTabViewVisible())
      return;

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
    gTabViewDeck.selectedIndex = 0;
    gWindow.TabsInTitlebar.allowedBy("tabview-open", true);
    gBrowser.contentWindow.focus();

    gBrowser.updateTitlebar();
#ifdef XP_MACOSX
    this.setTitlebarColors(false);
#endif
    let event = document.createEvent("Events");
    event.initEvent("tabviewhidden", true, false);
    dispatchEvent(event);

    Storage.saveVisibilityData(gWindow, "false");
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
    if (!this._storageBusyCount)
      TabItems.pauseReconnecting();
    
    this._storageBusyCount++;
  },
  
  // ----------
  // Function: storageReady
  // Resumes the activity paused by storageBusy, and updates for any new group
  // information in sessionstore. Calls can be nested. 
  storageReady: function UI_storageReady() {
    this._storageBusyCount--;
    if (!this._storageBusyCount) {
      let hasGroupItemsData = GroupItems.load();
      if (!hasGroupItemsData)
        this.reset(false);
  
      TabItems.resumeReconnecting();
      GroupItems._updateTabBar();
    }
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
    function pbObserver(aSubject, aTopic, aData) {
      if (aTopic == "private-browsing") {
        // We could probably do this in private-browsing-change-granted, but
        // this seems like a nicer spot, right in the middle of the process.
        if (aData == "enter") {
          // If we are in Tab View, exit. 
          self._privateBrowsing.wasInTabView = self.isTabViewVisible();
          if (self.isTabViewVisible())
            self.goToTab(gBrowser.selectedTab);
        }
      } else if (aTopic == "private-browsing-change-granted") {
        if (aData == "enter" || aData == "exit") {
          self._privateBrowsing.transitionMode = aData;
          self.storageBusy();
        }
      } else if (aTopic == "private-browsing-transition-complete") {
        // We use .transitionMode here, as aData is empty.
        if (self._privateBrowsing.transitionMode == "exit" &&
            self._privateBrowsing.wasInTabView)
          self.showTabView(false);

        self._privateBrowsing.transitionMode = "";
        self.storageReady();
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
    this._eventListeners.open = function(tab) {
      if (tab.ownerDocument.defaultView != gWindow)
        return;

      // if it's an app tab, add it to all the group items
      if (tab.pinned)
        GroupItems.addAppTab(tab);
    };
    
    // TabClose
    this._eventListeners.close = function(tab) {
      if (tab.ownerDocument.defaultView != gWindow)
        return;

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
        if (self._privateBrowsing.transitionMode)
          return; 
          
        // if not closing the last tab
        if (gBrowser.tabs.length > 1) {
          // Don't return to TabView if there are any app tabs
          for (let a = 0; a < gBrowser.tabs.length; a++) {
            let theTab = gBrowser.tabs[a]; 
            if (theTab.pinned && gBrowser._removingTabs.indexOf(theTab) == -1) 
              return;
          }

          var groupItem = GroupItems.getActiveGroupItem();

          // 1) Only go back to the TabView tab when there you close the last
          // tab of a groupItem.
          let closingLastOfGroup = (groupItem && 
              groupItem._children.length == 1 && 
              groupItem._children[0].tab == tab);
          
          // 2) Take care of the case where you've closed the last tab in
          // an un-named groupItem, which means that the groupItem is gone (null) and
          // there are no visible tabs. 
          let closingUnnamedGroup = (groupItem == null &&
              gBrowser.visibleTabs.length <= 1); 
              
          if (closingLastOfGroup || closingUnnamedGroup) {
            // for the tab focus event to pick up.
            self._closedLastVisibleTab = true;
            // remove the zoom prep.
            if (tab && tab._tabViewTabItem)
              tab._tabViewTabItem.setZoomPrep(false);
            self.showTabView();
          }
        }
      }
    };

    // TabMove
    this._eventListeners.move = function(tab) {
      if (tab.ownerDocument.defaultView != gWindow)
        return;

      let activeGroupItem = GroupItems.getActiveGroupItem();
      if (activeGroupItem)
        self.setReorderTabItemsOnShow(activeGroupItem);
    };

    // TabSelect
    this._eventListeners.select = function(tab) {
      if (tab.ownerDocument.defaultView != gWindow)
        return;

      self.onTabSelect(tab);
    };

    // TabPinned
    this._eventListeners.pinned = function(tab) {
      if (tab.ownerDocument.defaultView != gWindow)
        return;

      TabItems.handleTabPin(tab);
      GroupItems.addAppTab(tab);
    };

    // TabUnpinned
    this._eventListeners.unpinned = function(tab) {
      if (tab.ownerDocument.defaultView != gWindow)
        return;

      TabItems.handleTabUnpin(tab);
      GroupItems.removeAppTab(tab);
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
    let currentTab = this._currentTab;
    this._currentTab = tab;

    // if the last visible tab has just been closed, don't show the chrome UI.
    if (this.isTabViewVisible() &&
        (this._closedLastVisibleTab || this._closedSelectedTabInTabView ||
         this.restoredClosedTab)) {
      if (this.restoredClosedTab) {
        // when the tab view UI is being displayed, update the thumb for the 
        // restored closed tab after the page load
        tab.linkedBrowser.addEventListener("load", function (event) {
          tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
          TabItems._update(tab);
        }, true);
      }
      this._closedLastVisibleTab = false;
      this._closedSelectedTabInTabView = false;
      this.restoredClosedTab = false;
      return;
    }
    // reset these vars, just in case.
    this._closedLastVisibleTab = false;
    this._closedSelectedTabInTabView = false;
    this.restoredClosedTab = false;

    // if TabView is visible but we didn't just close the last tab or
    // selected tab, show chrome.
    if (this.isTabViewVisible())
      this.hideTabView();

    // another tab might be selected when hideTabView() is invoked so a
    // validation is needed.
    if (this._currentTab != tab)
      return;

    let oldItem = null;
    let newItem = null;

    if (currentTab && currentTab._tabViewTabItem)
      oldItem = currentTab._tabViewTabItem;

    // update the tab bar for the new tab's group
    if (tab && tab._tabViewTabItem) {
      if (!TabItems.reconnectingPaused()) {
        newItem = tab._tabViewTabItem;
        GroupItems.updateActiveGroupItemAndTabBar(newItem);
      }
    } else {
      // No tabItem; must be an app tab. Base the tab bar on the current group.
      // If no current group or orphan tab, figure it out based on what's
      // already in the tab bar.
      if (!GroupItems.getActiveGroupItem() && !GroupItems.getActiveOrphanTab()) {
        for (let a = 0; a < gBrowser.tabs.length; a++) {
          let theTab = gBrowser.tabs[a]; 
          if (!theTab.pinned) {
            let tabItem = theTab._tabViewTabItem; 
            if (tabItem.parent) 
              GroupItems.setActiveGroupItem(tabItem.parent);
            else 
              GroupItems.setActiveOrphanTab(tabItem); 
              
            break;
          }
        }
      }

      if (GroupItems.getActiveGroupItem() || GroupItems.getActiveOrphanTab())
        GroupItems._updateTabBar();
    }

    // ___ prepare for when we return to TabView
    if (newItem != oldItem) {
      if (oldItem)
        oldItem.setZoomPrep(false);
      if (newItem)
        newItem.setZoomPrep(true);
    } else if (oldItem)
      oldItem.setZoomPrep(true);
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
  updateTabButton: function UI__updateTabButton() {
    let groupsNumber = gWindow.document.getElementById("tabviewGroupsNumber");
    let exitButton = document.getElementById("exit-button");
    let numberOfGroups = GroupItems.groupItems.length;

    groupsNumber.setAttribute("groups", numberOfGroups);
    exitButton.setAttribute("groups", numberOfGroups);
  },

  // ----------
  // Function: getClosestTab
  // Convenience function to get the next tab closest to the entered position
  getClosestTab: function UI_getClosestTab(tabCenter) {
    let cl = null;
    let clDist;
    TabItems.getItems().forEach(function (item) {
      if (item.parent && item.parent.hidden)
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
  // Function: _setTabViewFrameKeyHandlers
  // Sets up the key handlers for navigating between tabs within the TabView UI.
  _setTabViewFrameKeyHandlers: function UI__setTabViewFrameKeyHandlers() {
    var self = this;

    iQ(window).keyup(function(event) {
      if (!event.metaKey) 
        Keys.meta = false;
    });

    iQ(window).keydown(function(event) {
      if (event.metaKey) 
        Keys.meta = true;

      if ((iQ(":focus").length > 0 && iQ(":focus")[0].nodeName == "INPUT") || 
          isSearchEnabled())
        return;

      function getClosestTabBy(norm) {
        if (!self.getActiveTab())
          return null;
        var centers =
          [[item.bounds.center(), item]
             for each(item in TabItems.getItems()) if (!item.parent || !item.parent.hidden)];
        var myCenter = self.getActiveTab().bounds.center();
        var matches = centers
          .filter(function(item){return norm(item[0], myCenter)})
          .sort(function(a,b){
            return myCenter.distance(a[0]) - myCenter.distance(b[0]);
          });
        if (matches.length > 0)
          return matches[0][1];
        return null;
      }

      var norm = null;
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
        var nextTab = getClosestTabBy(norm);
        if (nextTab) {
          if (nextTab.isStacked && !nextTab.parent.expanded)
            nextTab = nextTab.parent.getChild(0);
          self.setActiveTab(nextTab);
        }
        event.stopPropagation();
        event.preventDefault();
      } else if (event.keyCode == KeyEvent.DOM_VK_ESCAPE) {
        let activeGroupItem = GroupItems.getActiveGroupItem();
        if (activeGroupItem && activeGroupItem.expanded)
          activeGroupItem.collapse();
        else 
          self.exit();

        event.stopPropagation();
        event.preventDefault();
      } else if (event.keyCode == KeyEvent.DOM_VK_RETURN ||
                 event.keyCode == KeyEvent.DOM_VK_ENTER) {
        let activeTab = self.getActiveTab();
        if (activeTab)
          activeTab.zoomIn();

        event.stopPropagation();
        event.preventDefault();
      } else if (event.keyCode == KeyEvent.DOM_VK_TAB) {
        // tab/shift + tab to go to the next tab.
        var activeTab = self.getActiveTab();
        if (activeTab) {
          var tabItems = (activeTab.parent ? activeTab.parent.getChildren() :
                          [activeTab]);
          var length = tabItems.length;
          var currentIndex = tabItems.indexOf(activeTab);

          if (length > 1) {
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
            self.setActiveTab(tabItems[newIndex]);
          }
        }
        event.stopPropagation();
        event.preventDefault();
      } else if (event.keyCode == KeyEvent.DOM_VK_SLASH) {
        // the / event handler for find bar is defined in the findbar.xml
        // binding.  To keep things in its own module, we handle our slash here.
        self.enableSearch(event);
      }
    });
  },

  // ----------
  // Function: enableSearch
  // Enables the search feature.
  // Parameters:
  //   event - the event triggers this action.
  enableSearch: function UI_enableSearch(event) {
    if (!isSearchEnabled()) {
      ensureSearchShown();
      SearchEventHandler.switchToInMode();
      
      if (event) {
        event.stopPropagation();
        event.preventDefault();
      }
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
    GroupItems.setActiveGroupItem(null);

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
      GroupItems.setActiveGroupItem(lastActiveGroupItem);
    }

    function finalize(e) {
      iQ(window).unbind("mousemove", updateSize);
      item.container.removeClass("dragRegion");
      dragOutInfo.stop();
      let box = item.getBounds();
      if (box.width > minMinSize && box.height > minMinSize &&
         (box.width > minSize || box.height > minSize)) {
        var bounds = item.getBounds();

        // Add all of the orphaned tabs that are contained inside the new groupItem
        // to that groupItem.
        var tabs = GroupItems.getOrphanedTabs();
        var insideTabs = [];
        for each(let tab in tabs) {
          if (bounds.contains(tab.bounds))
            insideTabs.push(tab);
        }

        var groupItem = new GroupItem(insideTabs,{bounds:bounds});
        GroupItems.setActiveGroupItem(groupItem);
        phantom.remove();
        dragOutInfo = null;
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
      if (item.locked.bounds)
        return;

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
      if (item.locked.bounds)
        return;

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

    if (isSearchEnabled()) {
      let matcher = createSearchTabMacher();
      let matches = matcher.matched();

      if (matches.length > 0) {
        matches[0].zoomIn();
        zoomedIn = true;
      }
      hideSearch(null);
    }

    if (!zoomedIn) {
      let unhiddenGroups = GroupItems.groupItems.filter(function(groupItem) {
        return (!groupItem.hidden && groupItem.getChildren().length > 0);
      });
      // no pinned tabs, no visible groups and no orphaned tabs: open a new
      // group. open a blank tab and return
      if (!unhiddenGroups.length && !GroupItems.getOrphanedTabs().length) {
        let emptyGroups = GroupItems.groupItems.filter(function (groupItem) {
          return (!groupItem.hidden && !groupItem.getChildren().length);
        });
        let group = (emptyGroups.length ? emptyGroups[0] : GroupItems.newGroup());
        if (!gBrowser._numPinnedTabs) {
          group.newTab();
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
  // TODO: Save info items
  _saveAll: function UI__saveAll() {
    this._save();
    GroupItems.saveAll();
    TabItems.saveAll();
  },
};

// ----------
UI.init();
