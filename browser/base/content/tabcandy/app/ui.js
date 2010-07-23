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
 * Ian Gilman <ian@iangilman.com>.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Aza Raskin <aza@mozilla.com>
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>
 * Ehsan Akhgari <ehsan@mozilla.com>
 * Raymond Lee <raymond@appcoast.com>
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

(function() {

window.Keys = { meta: false };

// ##########
// Class: Tabbar
// Singleton for managing the tabbar of the browser.
var Tabbar = {
  // ----------
  // Function: showOnlyTheseTabs
  // Hides all of the tabs in the tab bar which are not passed into this function.
  //
  // Paramaters
  //  - An array of <TabItem> objects.
  //  - Some options
  showOnlyTheseTabs: function(tabs, options) {
    try {
      if (!options)
        options = {};

      let tabBarTabs = Array.slice(gBrowser.tabs);
      let visibleTabs = tabs.map(function(tab) tab.tab);

      // Show all of the tabs in the group.
      tabBarTabs.forEach(function(tab){
        let hidden = true;
        visibleTabs.some(function(visibleTab, i) {
          if (visibleTab == tab) {
            hidden = false;
            // remove the element to speed up the next loop.
            if (options.dontReorg)
              visibleTabs.splice(i, 1);
            return true;
          }
        });
        tab.hidden = hidden;
      });

      // Move them (in order) that they appear in the group to the end of the
      // tab strip. This way the tab order is matched up to the group's
      // thumbnail order.
      if (!options.dontReorg) {
        visibleTabs.forEach(function(visibleTab) {
          gBrowser.moveTabTo(visibleTab, tabBarTabs.length - 1);
        });
      }
    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: showAllTabs
  // Shows all of the tabs in the tab bar.
  showAllTabs: function() {
    Array.forEach(gBrowser.tabs, function(tab) {
      tab.hidden = false;
    });
  }
}

// ##########
// Class: UIManager
// Singleton top-level UI manager.
var UIManager = {
  // Variable: _devMode
  // If true (set by an url parameter), adds extra features to the screen.
  // TODO: Integrate with the dev menu
  _devMode : true,

  // Variable: _pageBounds
  // Stores the page bounds.
  _pageBounds : null,

  // Variable: _closedLastVisibleTab
  // If true, the last visible tab has just been closed in the tab strip.
  _closedLastVisibleTab : false,

  // Variable: _closedSelectedTabInTabCandy
  // If true, a select tab has just been closed in tab candy.
  _closedSelectedTabInTabCandy : false,

  // Variable: _stopZoomPreparation
  // If true, prevent the next zoom preparation.
  _stopZoomPreparation : false,

  // Variable: _currentTab
  // Keeps track of which <Tabs> tab we are currently on.
  // Used to facilitate zooming down from a previous tab.
  _currentTab : gBrowser.selectedTab,

  // Variable: tabBar
  // A reference to the <Tabbar>, for manipulating the browser's tab bar.
  get tabBar() { return Tabbar; },

  // ----------
  // Function: init
  // Must be called after the object is created.
  init: function() {
    try {
      if (window.Tabs)
        this._secondaryInit();
      else {
        var self = this;
        TabsManager.addSubscriber(this, "load", function() {
          self._secondaryInit();
        });
      }
    } catch(e) {
      Utils.log(e);
    }
  },

  // -----------
  // Function: _secondaryInit
  // This is the bulk of the initialization, kicked off automatically by init
  // once the system is ready.
  _secondaryInit: function() {
    try {
      var self = this;

      // ___ Dev Menu
      if (this._devMode)
        this._addDevMenu();

      // ___ add event listeners
      iQ("#reset").click(function() {
        self._reset();
      });

      // When you click on the background/empty part of TabCandy,
      // we create a new group.
      iQ(gTabViewFrame.contentDocument).mousedown(function(e){
        if ( e.originalTarget.id == "content" )
          self._createGroupOnDrag(e)
      });

      iQ(window).bind("beforeunload", function() {
        self.tabBar.showAllTabs();
      });

      gWindow.addEventListener("tabcandyshow", function() {
        self.showTabCandy(true);
      }, false);

      gWindow.addEventListener("tabcandyhide", function() {
        var activeTab = self.getActiveTab();
        if (activeTab)
          activeTab.zoomIn();
      }, false);

      // ___ setup key handlers
      this._setBrowserKeyHandlers();
      this._setTabViewFrameKeyHandlers();

      // ___ add tab action handlers
      this._addTabActionHandlers();

      // ___ delay init
      Storage.onReady(function() {
        self._delayInit();
      });
    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: _delayInit
  // Called automatically by _secondaryInit once sessionstore is online.
  _delayInit : function() {
    try {
      var self = this;

      // ___ Storage
      var data = Storage.readUIData(gWindow);
      this._storageSanity(data);

      var groupsData = Storage.readGroupsData(gWindow);
      var firstTime = !groupsData || Utils.isEmptyObject(groupsData);
      var groupData = Storage.readGroupData(gWindow);
      Groups.reconstitute(groupsData, groupData);

      TabItems.init();

      if (firstTime) {
        var padding = 10;
        var infoWidth = 350;
        var infoHeight = 350;
        var pageBounds = Items.getPageBounds();
        pageBounds.inset(padding, padding);

        // ___ make a fresh group
        var box = new Rect(pageBounds);
        box.width =
          Math.min(box.width * 0.667, pageBounds.width - (infoWidth + padding));
        box.height = box.height * 0.667;
        var options = {
          bounds: box
        };

        var group = new Group([], options);

        var items = TabItems.getItems();
        items.forEach(function(item) {
          if (item.parent)
            item.parent.remove(item);

          group.add(item);
        });

        // ___ make info item
        var html =
          "<div class='intro'>"
            + "<h1>Welcome to Firefox Tab Sets</h1>"
            + "<div>(more goes here)</div><br>"
            + "<video src='http://html5demos.com/assets/dizzy.ogv' "
            + "width='100%' preload controls>"
          + "</div>";

        box.left = box.right + padding;
        box.width = infoWidth;
        box.height = infoHeight;
        var infoItem = new InfoItem(box);
        infoItem.html(html);
      }

      // ___ resizing
      if (data.pageBounds) {
        this._pageBounds = data.pageBounds;
        this._resize(true);
      } else
        this._pageBounds = Items.getPageBounds();

      iQ(window).resize(function() {
        self._resize();
      });

      // ___ show Tab Candy at startup based on last session.
      if (data.tabCandyVisible) {
        var currentTab = self._currentTab;
        var item;

        if (currentTab && currentTab.mirror)
          item = TabItems.getItemByTabElement(currentTab.mirror.el);

        if (item)
          item.setZoomPrep(false);
        else
          self._stopZoomPreparation = true;

        self.showTabCandy();
      } else
        self.hideTabCandy();

      // ___ setup observer to save canvas images
      Components.utils.import("resource://gre/modules/Services.jsm");
      var observer = {
        observe : function(subject, topic, data) {
          if (topic == "quit-application-requested") {
            if (self._isTabCandyVisible())
              TabItems.saveAll(true);
            self._save();
          }
        }
      };
      Services.obs.addObserver(observer, "quit-application-requested", false);

      // ___ Done
      this._initialized = true;
      this._save();
    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: getActiveTab
  // Returns the currently active tab as a <TabItem>
  //
  getActiveTab: function() {
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
  setActiveTab: function(tab) {
    if (tab == this._activeTab)
      return;

    if (this._activeTab) {
      this._activeTab.makeDeactive();
      this._activeTab.removeOnClose(this);
    }
    this._activeTab = tab;

    if (this._activeTab) {
      var self = this;
      this._activeTab.addOnClose(this, function() {
        self._activeTab = null;
      });

      this._activeTab.makeActive();
    }
  },

  // ----------
  // Function: _isTabCandyVisible
  // Returns true if the TabCandy UI is currently shown.
  _isTabCandyVisible: function() {
    return gTabViewDeck.selectedIndex == 1;
  },

  // ----------
  // Function: showTabCandy
  // Shows TabCandy and hides the main browser UI.
  // Parameters:
  //   zoomOut - true for zoom out animation, false for nothing.
  showTabCandy: function(zoomOut) {
    var self = this;
    var currentTab = this._currentTab;
    var item = null;

    gTabViewDeck.selectedIndex = 1;
    gTabViewFrame.contentWindow.focus();

    gBrowser.updateTitlebar();
#ifdef XP_MACOSX
    this._setActiveTitleColor(true);
#endif

    if (zoomOut) {
      if (currentTab && currentTab.mirror)
        item = TabItems.getItemByTabElement(currentTab.mirror.el);

      if (item) {
        // If there was a previous currentTab we want to animate
        // its mirror for the zoom out.
        // Note that we start the animation on the chrome thread.

        // Zoom out!
        item.zoomOut(function() {
          if (!currentTab.mirror) // if the tab's been destroyed
            item = null;

          self.setActiveTab(item);

          var activeGroup = Groups.getActiveGroup();
          if (activeGroup)
            activeGroup.setTopChild(item);

          window.Groups.setActiveGroup(null);
          self._resize(true);
        });
      }
    }
  },

  // ----------
  // Function: hideTabCandy
  // Hides Tab candy and shows the main browser UI .
  hideTabCandy: function() {
    gTabViewDeck.selectedIndex = 0;
    gBrowser.contentWindow.focus();

    // set the close button on tab
/*     Utils.timeout(function() { // Marshal event from chrome thread to DOM thread    */
      gBrowser.tabContainer.adjustTabstrip();
/*     }, 1); */

    gBrowser.updateTitlebar();
#ifdef XP_MACOSX
    this._setActiveTitleColor(false);
#endif
  },

#ifdef XP_MACOSX
  // ----------
  // Function: _setActiveTitleColor
  // Used on the Mac to make the title bar match the gradient in the rest of the
  // TabCandy UI.
  //
  // Parameters:
  //   set - true for the special TabCandy color, false for the normal color.
  _setActiveTitleColor: function(set) {
    // Mac Only
    var mainWindow = gWindow.document.getElementById("main-window");
    if (set)
      mainWindow.setAttribute("activetitlebarcolor", "#C4C4C4");
    else
      mainWindow.removeAttribute("activetitlebarcolor");
  },
#endif

  // ----------
  // Function: _addTabActionHandlers
  // Adds handlers to handle tab actions.
  _addTabActionHandlers: function() {
    var self = this;

    Tabs.onClose(function(){
      if (self._isTabCandyVisible()) {
        // just closed the selected tab in the tab candy interface.
        if (self._currentTab == this)
          self._closedSelectedTabInTabCandy = true;
      } else {
        var group = Groups.getActiveGroup();
        // 1) Only go back to the TabCandy tab when there you close the last
        // tab of a group.
        // 2) ake care of the case where you've closed the last tab in
        // an un-named group, which means that the group is gone (null) and
        // there are no visible tabs.
        if ((group && group._children.length == 1) ||
            (group == null &&
             gBrowser.visibleTabs.length == 1)) {
          self._closedLastVisibleTab = true;
          // remove the zoom prep.
          if (this && this.mirror) {
            var item = TabItems.getItemByTabElement(this.mirror.el);
            if (item)
              item.setZoomPrep(false);
          }
          self.showTabCandy();
        }
      }
      return false;
    });

    Tabs.onMove(function() {
      Utils.timeout(function() { // Marshal event from chrome thread to DOM thread
        var activeGroup = Groups.getActiveGroup();
        if ( activeGroup )
          activeGroup.reorderBasedOnTabOrder();
      }, 1);
    });

    Tabs.onFocus(function() {
      self.tabOnFocus(this);
    });
  },

  // ----------
  // Function: tabOnFocus
  // Called when the user switches from one tab to another outside of the TabCandy UI.
  tabOnFocus: function(tab) {
    var self = this;
    var focusTab = tab;
    var currentTab = this._currentTab;

    this._currentTab = focusTab;
    // if the last visible tab has just been closed, don't show the chrome UI.
    if (this._isTabCandyVisible() &&
        (this._closedLastVisibleTab || this._closedSelectedTabInTabCandy)) {
      this._closedLastVisibleTab = false;
      this._closedSelectedTabInTabCandy = false;
      return;
    }

    // if TabCandy is visible but we didn't just close the last tab or
    // selected tab, show chrome.
    if (this._isTabCandyVisible())
      this.hideTabCandy();

    // reset these vars, just in case.
    this._closedLastVisibleTab = false;
    this._closedSelectedTabInTabCandy = false;

    Utils.timeout(function() { // Marshal event from chrome thread to DOM thread
      // this value is true when tabcandy is open at browser startup.
      if (self._stopZoomPreparation) {
        self._stopZoomPreparation = false;
        if (focusTab && focusTab.mirror) {
          var item = TabItems.getItemByTabElement(focusTab.mirror.el);
          if (item)
            self.setActiveTab(item);
        }
        return;
      }

      if (focusTab != self._currentTab) {
        // things have changed while we were in timeout
        return;
      }

      var visibleTabCount = gBrowser.visibleTabs.length;

      var newItem = null;
      if (focusTab && focusTab.mirror)
        newItem = TabItems.getItemByTabElement(focusTab.mirror.el);

      if (newItem)
        Groups.setActiveGroup(newItem.parent);

      // ___ prepare for when we return to TabCandy
      var oldItem = null;
      if (currentTab && currentTab.mirror)
        oldItem = TabItems.getItemByTabElement(currentTab.mirror.el);

      if (newItem != oldItem) {
        if (oldItem)
          oldItem.setZoomPrep(false);

        // if the last visible tab is removed, don't set zoom prep because
        // we shoud be in the Tab Candy interface.
        if (visibleTabCount > 0 && newItem)
          newItem.setZoomPrep(true);
      } else {
        // the tab is already focused so the new and old items are the
        // same.
        if (oldItem)
          oldItem.setZoomPrep(true);
      }
    }, 1);
  },

  // ----------
  // Function: _setBrowserKeyHandlers
  // Overrides the browser's keys for navigating between tab (outside of the
  // TabCandy UI) so they do the right thing in respect to groups.
  _setBrowserKeyHandlers : function() {
    var self = this;
    var tabbox = gBrowser.mTabBox;

    gWindow.addEventListener("keypress", function(event) {
      if (self._isTabCandyVisible())
        return;

      var charCode = event.charCode;
#ifdef XP_MACOSX
      // if a text box in a webpage has the focus, the event.altKey would
      // return false so we are depending on the charCode here.
      if (!event.ctrlKey && !event.metaKey && !event.shiftKey &&
          charCode == 160) { // alt + space
#else
      if (event.ctrlKey && !event.metaKey && !event.shiftKey &&
          !event.altKey && charCode == 32) { // ctrl + space
#endif
        event.stopPropagation();
        event.preventDefault();
        self.showTabCandy(true);
        return;
      }

      // Control (+ Shift) + `
      if (event.ctrlKey && !event.metaKey && !event.altKey &&
          (charCode == 96 || charCode == 126)) {
        event.stopPropagation();
        event.preventDefault();
        var tabItem = Groups.getNextGroupTab(event.shiftKey);
        if (tabItem)
          gBrowser.selectedTab = tabItem.tab;
      }
    }, true);
  },

  // ----------
  // Function: _setTabViewFrameKeyHandlers
  // Sets up the key handlers for navigating between tabs within the TabCandy UI.
  _setTabViewFrameKeyHandlers: function(){
    var self = this;

    iQ(window).keyup(function(event) {
      if (!event.metaKey) window.Keys.meta = false;
    });

    iQ(window).keydown(function(event) {
      if (event.metaKey) window.Keys.meta = true;

      if (!self.getActiveTab() || iQ(":focus").length > 0) {
        // prevent the default action when tab is pressed so it doesn't gives
        // us problem with content focus.
        if (event.which == 9) {
          event.stopPropagation();
          event.preventDefault();
        }
        return;
      }

      function getClosestTabBy(norm){
        var centers =
          [[item.bounds.center(), item] for each(item in TabItems.getItems())];
        var myCenter = self.getActiveTab().bounds.center();
        var matches = centers
          .filter(function(item){return norm(item[0], myCenter)})
          .sort(function(a,b){
            return myCenter.distance(a[0]) - myCenter.distance(b[0]);
          });
        if ( matches.length > 0 )
          return matches[0][1];
        return null;
      }

      var norm = null;
      switch (event.which) {
        case 39: // Right
          norm = function(a, me){return a.x > me.x};
          break;
        case 37: // Left
          norm = function(a, me){return a.x < me.x};
          break;
        case 40: // Down
          norm = function(a, me){return a.y > me.y};
          break;
        case 38: // Up
          norm = function(a, me){return a.y < me.y}
          break;
      }

      if (norm != null) {
        var nextTab = getClosestTabBy(norm);
        if (nextTab) {
          if (nextTab.inStack() && !nextTab.parent.expanded)
            nextTab = nextTab.parent.getChild(0);
          self.setActiveTab(nextTab);
        }
        event.stopPropagation();
        event.preventDefault();
      } else if (event.which == 32) {
        // alt/control + space to zoom into the active tab.
#ifdef XP_MACOSX
        if (event.altKey && !event.metaKey && !event.shiftKey &&
            !event.ctrlKey) {
#else
        if (event.ctrlKey && !event.metaKey && !event.shiftKey &&
            !event.altKey) {
#endif
          var activeTab = self.getActiveTab();
          if (activeTab)
            activeTab.zoomIn();
          event.stopPropagation();
          event.preventDefault();
        }
      } else if (event.which == 27 || event.which == 13) {
        // esc or return to zoom into the active tab.
        var activeTab = self.getActiveTab();
        if (activeTab)
          activeTab.zoomIn();
        event.stopPropagation();
        event.preventDefault();
      } else if (event.which == 9) {
        // tab/shift + tab to go to the next tab.
        var activeTab = self.getActiveTab();
        if (activeTab) {
          var tabItems = (activeTab.parent ? activeTab.parent.getChildren() :
                          Groups.getOrphanedTabs());
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
      }
    });
  },

  // ----------
  // Function: _createGroupOnDrag
  // Called in response to a mousedown in empty space in the TabCandy UI;
  // creates a new group based on the user's drag.
  _createGroupOnDrag: function(e){
    const minSize = 60;
    const minMinSize = 15;

    var startPos = { x: e.clientX, y: e.clientY };
    var phantom = iQ("<div>")
      .addClass("group phantom")
      .css({
        position: "absolute",
        opacity: .7,
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
        this.container.css("z-index", z);
      },
      setOpacity: function FauxItem_setOpacity( opacity ) {
        this.container.css("opacity", opacity);
      },
      // we don't need to pushAway the phantom item at the end, because
      // when we create a new Group, it'll do the actual pushAway.
      pushAway: function () {},
    };
    item.setBounds(new Rect(startPos.y, startPos.x, 0, 0));

    var dragOutInfo = new Drag(item, e, true); // true = isResizing

    function updateSize(e){
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
      if (box.width > minMinSize && box.height > minMinSize
          && (box.width > minSize || box.height > minSize))
        item.setOpacity(1);
      else
        item.setOpacity(0.7);

      e.preventDefault();
    }

    function collapse() {
      phantom.animate({
        width: 0,
        height: 0,
        top: phantom.position().top + phantom.height()/2,
        left: phantom.position().left + phantom.width()/2
      }, {
        duration: 300,
        complete: function() {
          phantom.remove();
        }
      });
    }

    function finalize(e) {
      iQ(window).unbind("mousemove", updateSize);
      dragOutInfo.stop();
      if (phantom.css("opacity") != 1)
        collapse();
      else {
        var bounds = item.getBounds();

        // Add all of the orphaned tabs that are contained inside the new group
        // to that group.
        var tabs = Groups.getOrphanedTabs();
        var insideTabs = [];
        for each(tab in tabs) {
          if (bounds.contains(tab.bounds))
            insideTabs.push(tab);
        }

        var group = new Group(insideTabs,{bounds:bounds});
        phantom.remove();
        dragOutInfo = null;
      }
    }

    iQ(window).mousemove(updateSize)
    iQ(gWindow).one("mouseup", finalize);
    e.preventDefault();
    return false;
  },

  // ----------
  // Function: _resize
  // Update the TabCandy UI contents in response to a window size change.
  // Won't do anything if it doesn't deem the resize necessary.
  // Parameters:
  //   force - true to update even when "unnecessary"; default false
  _resize: function(force) {
    if (typeof(force) == "undefined")
      force = false;

    // If TabCandy isn't focused and is not showing, don't perform a resize.
    // This resize really slows things down.
    if (!force && !this._isTabCandyVisible())
      return;

    var oldPageBounds = new Rect(this._pageBounds);
    var newPageBounds = Items.getPageBounds();
    if (newPageBounds.equals(oldPageBounds))
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

    Groups.repositionNewTabGroup(); // TODO:

    if (newPageBounds.width < this._pageBounds.width &&
        newPageBounds.width > itemBounds.width)
      newPageBounds.width = this._pageBounds.width;

    if (newPageBounds.height < this._pageBounds.height &&
        newPageBounds.height > itemBounds.height)
      newPageBounds.height = this._pageBounds.height;

    var wScale;
    var hScale;
    if ( Math.abs(newPageBounds.width - this._pageBounds.width)
         > Math.abs(newPageBounds.height - this._pageBounds.height) ) {
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
      bounds.left += newPageBounds.left - self._pageBounds.left;
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
  // Function: _addDevMenu
  // Fills out the "dev menu" in the TabCandy UI.
  _addDevMenu: function() {
    try {
      var self = this;

      var $select = iQ("<select>")
        .css({
          position: "absolute",
          bottom: 5,
          right: 5,
          zIndex: 99999,
          opacity: .2
        })
        .appendTo("#content")
        .change(function () {
          var index = iQ(this).val();
          try {
            commands[index].code.apply(commands[index].element);
          } catch(e) {
            Utils.log("dev menu error", e);
          }
          iQ(this).val(0);
        });

      var commands = [{
        name: "dev menu",
        code: function() { }
      }, {
        name: "show trenches",
        code: function() {
          Trenches.toggleShown();
          iQ(this).html((Trenches.showDebug ? "hide" : "show") + " trenches");
        }
      }, {
        name: "refresh",
        code: function() {
          location.href = "tabcandy.html";
        }
      }, {
        name: "reset",
        code: function() {
          self._reset();
        }
      }, {
        name: "save",
        code: function() {
          self._saveAll();
        }
      }, {
        name: "group sites",
        code: function() {
          self._arrangeBySite();
        }
      }];

      var count = commands.length;
      var a;
      for (a = 0; a < count; a++) {
        commands[a].element = iQ("<option>")
          .val(a)
          .html(commands[a].name)
          .appendTo($select)
          .get(0);
      }
    } catch(e) {
      Utils.log(e);
    }
  },

  // -----------
  // Function: _reset
  // Wipes all TabCandy storage and refreshes, giving you the "first-run" state.
  _reset: function() {
    Storage.wipe();
    location.href = "";
  },

  // ----------
  // Function: storageSanity
  // Given storage data for this object, returns true if it looks valid.
  _storageSanity: function(data) {
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
  _save: function() {
    if (!this._initialized)
      return;

    var data = {
      tabCandyVisible: this._isTabCandyVisible(),
      pageBounds: this._pageBounds
    };

    if (this._storageSanity(data))
      Storage.saveUIData(gWindow, data);
  },

  // ----------
  // Function: _saveAll
  // Saves all data associated with TabCandy.
  // TODO: Save info items
  _saveAll: function() {
    this._save();
    Groups.saveAll();
    TabItems.saveAll();
  },

  // ----------
  // Function: _arrangeBySite
  // Blows away all existing groups and organizes the tabs into new groups based
  // on domain.
  _arrangeBySite: function() {
    function putInGroup(set, key) {
      var group = Groups.getGroupWithTitle(key);
      if (group) {
        set.forEach(function(el) {
          group.add(el);
        });
      } else
        new Group(set, { dontPush: true, dontArrange: true, title: key });
    }

    Groups.removeAll();

    var newTabsGroup = Groups.getNewTabGroup();
    var groups = [];
    var items = TabItems.getItems();
    items.forEach(function(item) {
      var url = item.tab.linkedBrowser.currentURI.spec;
      var domain = url.split('/')[2];

      if (!domain)
        newTabsGroup.add(item);
      else {
        var domainParts = domain.split(".");
        var mainDomain = domainParts[domainParts.length - 2];
        if (groups[mainDomain])
          groups[mainDomain].push(item.container);
        else
          groups[mainDomain] = [item.container];
      }
    });

    var leftovers = [];
    for (key in groups) {
      var set = groups[key];
      if (set.length > 1) {
        putInGroup(set, key);
      } else
        leftovers.push(set[0]);
    }
    putInGroup(leftovers, "mixed");

    Groups.arrange();
  },
};

// ----------
window.UI = UIManager;
window.UI.init();

})();
