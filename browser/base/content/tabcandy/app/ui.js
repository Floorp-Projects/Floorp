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

(function(){

window.Keys = {meta: false};

// ##########
// Class: Navbar
// Singleton for helping with the browser's nav bar.
Navbar = {
  // ----------
  // Variable: urlBar
  // The URL bar for the window.
  get urlBar() {
    var win = Utils.getCurrentWindow();
    if (win)
      return win.gURLBar;
    return null;
  }
};

// ##########
// Class: Tabbar
// Singleton for managing the tabbar of the browser.
var Tabbar = {
  // ----------
  // Variable: el
  // The tab bar's element.
  get el() {
    return window.Tabs[0].raw.parentNode;
  },

  // ----------
  // Function: getVisibleTabCount
  // Returns the number of tabs that are currently visible
  getVisibleTabCount: function(){
    var visibleTabCount = 0;
    this.getAllTabs().forEach(function(tab){
      if ( !tab.collapsed )
        visibleTabCount++
    });
    return visibleTabCount;
  },

  // ----------
  // Function: getAllTabs
  // Returns an array of all tabs which exist in the current window
  // tabbrowser. Returns an Array.
  getAllTabs: function() {
    var tabBarTabs = [];
    if (this.el) {
      // this.el.children is not a real array and does contain
      // useful functions like filter or forEach. Convert it into a real array.
      for (var i = 0; i < this.el.children.length; i++) {
        tabBarTabs.push(this.el.children[i]);
      }
    }
    return tabBarTabs;
  },

  // ----------
  // Function: showOnlyTheseTabs
  // Hides all of the tabs in the tab bar which are not passed into this function.
  //
  // Paramaters
  //  - An array of <BrowserTab> objects.
  //  - Some options
  showOnlyTheseTabs: function(tabs, options){
    try {
      if (!options)
        options = {};

      var tabbrowser = Utils.getCurrentWindow().gBrowser;
      var tabBarTabs = this.getAllTabs();

      // Show all of the tabs in the group.
      tabBarTabs.forEach(function(tab){
        var collapsed = true;
        tabs.some(function(visibleTabItem, i) {
          if (visibleTabItem.tab.raw == tab) {
            collapsed = false;
            // remove the element to speed up the next loop.
            if (options.dontReorg)
              tabs.splice(i, 1);
            return true;
          }
        });
        tab.collapsed = collapsed;
      });

      // Move them (in order) that they appear in the group to the end of the
      // tab strip. This way the tab order is matched up to the group's
      // thumbnail order.
      if (!options.dontReorg) {
        tabs.forEach(function(visibleTabItem) {
          tabbrowser.moveTabTo(visibleTabItem.tab.raw, tabBarTabs.length - 1);
        });
      }

    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: showAllTabs
  // Shows all of the tabs in the tab bar.
  showAllTabs: function(){
    this.getAllTabs().forEach(function(tab) {
      tab.collapsed = false;
    });
  }
}

// ##########
// Class: Page
// Singleton top-level UI manager. TODO: Integrate with <UIClass>.
window.Page = {
  closedLastVisibleTab: false,
  closedSelectedTabInTabCandy: false,
  stopZoomPreparation: false,

  // ----------
  // Function: isTabCandyVisible
  // Returns true if the TabCandy UI is currently shown.
  isTabCandyVisible: function(){
    return (Utils.getCurrentWindow().document.getElementById("tab-candy-deck").
             selectedIndex == 1);
  },

  // ----------
  // Function: hideChrome
  // Hides the nav bar, tab bar, etc.
  hideChrome: function(){
    var currentWin = Utils.getCurrentWindow();
    currentWin.document.getElementById("tab-candy-deck").selectedIndex = 1;
    currentWin.document.getElementById("tab-candy").contentWindow.focus();

    currentWin.gBrowser.updateTitlebar();
#ifdef XP_MACOSX
    this._setActiveTitleColor(true);
#endif
  },

  // ----------
  // Function: showChrome
  // Shows the nav bar, tab bar, etc.
  showChrome: function(){
    var currentWin = Utils.getCurrentWindow();
    var tabContainer = currentWin.gBrowser.tabContainer;
    currentWin.document.getElementById("tab-candy-deck").selectedIndex = 0;
    currentWin.gBrowser.contentWindow.focus();

    // set the close button on tab
/*     iQ.timeout(function() { // Marshal event from chrome thread to DOM thread    */
      tabContainer.adjustTabstrip();
/*     }, 1); */

    currentWin.gBrowser.updateTitlebar();
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
    var mainWindow =
      Utils.getCurrentWindow().document.getElementById("main-window");
    if (set)
      mainWindow.setAttribute("activetitlebarcolor", "#C4C4C4");
    else
      mainWindow.removeAttribute("activetitlebarcolor");
  },
#endif

  // ----------
  // Function: showTabCandy
  // Zoom out of the current tab (if applicable) and show the TabCandy UI.
  showTabCandy: function() {
    var self = this;
    var currentTab = UI.currentTab;
    var item = null;

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
        if ( activeGroup )
          activeGroup.setTopChild(item);

        window.Groups.setActiveGroup(null);
        UI.resize(true);
      });
    }
  },

  // ----------
  // Function: init
  // Starts this object.
  init: function() {
    var self = this;

    // When you click on the background/empty part of TabCandy,
    // we create a new group.
    var tabCandyContentDoc =
      Utils.getCurrentWindow().document.getElementById("tab-candy").
        contentDocument;
    iQ(tabCandyContentDoc).mousedown(function(e){
      if ( e.originalTarget.id == "content" )
        self.createGroupOnDrag(e)
    });

    this._setupKeyHandlers();

    Tabs.onClose(function(){
      if (self.isTabCandyVisible()) {
        // just closed the selected tab in the tab candy interface.
        if (UI.currentTab == this) {
          self.closedSelectedTabInTabCandy = true;
        }
      } else {
        var group = Groups.getActiveGroup();
        // 1) Only go back to the TabCandy tab when there you close the last
        // tab of a group.
        // 2) ake care of the case where you've closed the last tab in
        // an un-named group, which means that the group is gone (null) and
        // there are no visible tabs.
        if ((group && group._children.length == 1) ||
            (group == null && Tabbar.getVisibleTabCount() == 1)) {
          self.closedLastVisibleTab = true;
          // remove the zoom prep.
          if (this && this.mirror) {
            var item = TabItems.getItemByTabElement(this.mirror.el);
            if (item) {
              item.setZoomPrep(false);
            }
          }
          self.hideChrome();
        }
      }
      return false;
    });

    Tabs.onMove(function() {
      iQ.timeout(function() { // Marshal event from chrome thread to DOM thread
        var activeGroup = Groups.getActiveGroup();
        if ( activeGroup ) {
          activeGroup.reorderBasedOnTabOrder();
        }
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
    var focusTab = tab;
    var currentTab = UI.currentTab;
    var currentWindow = Utils.getCurrentWindow();
    var self = this;

    UI.currentTab = focusTab;
    // if the last visible tab has just been closed, don't show the chrome UI.
    if (this.isTabCandyVisible() &&
        (this.closedLastVisibleTab || this.closedSelectedTabInTabCandy)) {
      this.closedLastVisibleTab = false;
      this.closedSelectedTabInTabCandy = false;
      return;
    }

    // if TabCandy is visible but we didn't just close the last tab or
    // selected tab, show chrome.
    if (this.isTabCandyVisible())
      this.showChrome();

    // reset these vars, just in case.
    this.closedLastVisibleTab = false;
    this.closedSelectedTabInTabCandy = false;

    iQ.timeout(function() { // Marshal event from chrome thread to DOM thread
      // this value is true when tabcandy is open at browser startup.
      if (self.stopZoomPreparation) {
        self.stopZoomPreparation = false;
        if (focusTab && focusTab.mirror) {
          var item = TabItems.getItemByTabElement(focusTab.mirror.el);
          if (item)
            self.setActiveTab(item);
        }
        return;
      }

      if (focusTab != UI.currentTab) {
        // things have changed while we were in timeout
        return;
      }

      var visibleTabCount = Tabbar.getVisibleTabCount();

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
        if (visibleTabCount > 0) {
          if (newItem)
            newItem.setZoomPrep(true);
        }
      } else {
        // the tab is already focused so the new and old items are the
        // same.
        if (oldItem)
          oldItem.setZoomPrep(true);
      }
    }, 1);
  },

  // ----------
  _setupKeyHandlers: function(){
    var self = this;
    iQ(window).keyup(function(event){
      if (!event.metaKey) window.Keys.meta = false;
    });

    iQ(window).keydown(function(event){
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
      } else if (event.which == 32) { // alt or control + space
#ifdef XP_MACOSX
        if (event.altKey && !event.metaKey && !event.shiftKey &&
            !event.ctrlKey) {
#else
        if (event.ctrlKey && !event.metaKey && !event.shiftKey &&
            !event.altKey) {
#endif
          var activeTab = Page.getActiveTab();
          if (activeTab)
            activeTab.zoomIn();
          event.stopPropagation();
          event.preventDefault();
        }
      } else if (event.which == 27 || event.which == 13) { // esc or return
        var activeTab = self.getActiveTab();
        if (activeTab)
          activeTab.zoomIn();
        event.stopPropagation();
        event.preventDefault();
      } else if (event.which == 9) { // tab or shift+tab
        var activeTab = self.getActiveTab();
        if (activeTab) {
          var tabItems = (activeTab.parent ? activeTab.parent.getChildren() :
                          Groups.getOrphanedTabs());
          var length = tabItems.length;
          var currentIndex = tabItems.indexOf(activeTab);

          if (length > 1) {
            if (event.shiftKey) {
              if (currentIndex == 0) {
                newIndex = (length - 1);
              } else {
                newIndex = (currentIndex - 1);
              }
            } else {
              if (currentIndex == (length - 1)) {
                newIndex = 0;
              } else {
                newIndex = (currentIndex + 1);
              }
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
  // Function: createGroupOnDrag
  // Called in response to a mousedown in empty space in the TabCandy UI;
  // creates a new group based on the user's drag.
  createGroupOnDrag: function(e){
    const minSize = 60;
    const minMinSize = 15;

    var startPos = {x:e.clientX, y:e.clientY}
    var phantom = iQ("<div>")
      .addClass('group phantom')
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
      setBounds: function FauxItem_setBounds( bounds ) {
        this.container.css( bounds );
      },
      setZ: function FauxItem_setZ( z ) {
        this.container.css( 'z-index', z );
      },
      setOpacity: function FauxItem_setOpacity( opacity ) {
        this.container.css( 'opacity', opacity );
      },
      // we don't need to pushAway the phantom item at the end, because
      // when we create a new Group, it'll do the actual pushAway.
      pushAway: function () {},
    };
    item.setBounds( new Rect( startPos.y, startPos.x, 0, 0 ) );

    var dragOutInfo = new Drag(item, e, true); // true = isResizing

    function updateSize(e){
      var box = new Rect();
      box.left = Math.min(startPos.x, e.clientX);
      box.right = Math.max(startPos.x, e.clientX);
      box.top = Math.min(startPos.y, e.clientY);
      box.bottom = Math.max(startPos.y, e.clientY);
      item.setBounds(box);

      // compute the stationaryCorner
      var stationaryCorner = '';

      if (startPos.y == box.top)
        stationaryCorner += 'top';
      else
        stationaryCorner += 'bottom';

      if (startPos.x == box.left)
        stationaryCorner += 'left';
      else
        stationaryCorner += 'right';

      dragOutInfo.snap(stationaryCorner, false, false); // null for ui, which we don't use anyway.

      box = item.getBounds();
      if (box.width > minMinSize && box.height > minMinSize
          && (box.width > minSize || box.height > minSize))
        item.setOpacity(1);
      else
        item.setOpacity(0.7);

      e.preventDefault();
    }

    function collapse(){
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

    function finalize(e){
      iQ(window).unbind("mousemove", updateSize);
      dragOutInfo.stop();
      if ( phantom.css("opacity") != 1 )
        collapse();
      else {
        var bounds = item.getBounds();

        // Add all of the orphaned tabs that are contained inside the new group
        // to that group.
        var tabs = Groups.getOrphanedTabs();
        var insideTabs = [];
        for each( tab in tabs ){
          if ( bounds.contains( tab.bounds ) ){
            insideTabs.push(tab);
          }
        }

        var group = new Group(insideTabs,{bounds:bounds});
        phantom.remove();
        dragOutInfo = null;
      }
    }

    iQ(window).mousemove(updateSize)
    iQ(Utils.getCurrentWindow()).one('mouseup', finalize);
    e.preventDefault();
    return false;
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
  //
  setActiveTab: function(tab){
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
  // Function: getActiveTab
  // Returns the currently active tab as a <TabItem>
  //
  getActiveTab: function(){
    return this._activeTab;
  }
}

// ##########
// Class: UIClass
// Singleton top-level UI manager. TODO: Integrate with <Page>.
function UIClass() {
  try {
    // Variable: navBar
    // A reference to the <Navbar>, for manipulating the browser's nav bar.
    this.navBar = Navbar;

    // Variable: tabBar
    // A reference to the <Tabbar>, for manipulating the browser's tab bar.
    this.tabBar = Tabbar;

    // Variable: devMode
    // If true (set by an url parameter), adds extra features to the screen.
    // TODO: Integrate with the dev menu
    this.devMode = false;

    // Variable: currentTab
    // Keeps track of which <Tabs> tab we are currently on.
    // Used to facilitate zooming down from a previous tab.
    this.currentTab = Utils.activeTab;
  } catch(e) {
    Utils.log(e);
  }
};

// ----------
UIClass.prototype = {
  // ----------
  // Function: init
  // Must be called after the object is created.
  init: function() {
    try {
      if (window.Tabs)
        this._secondaryInit();
      else {
        var self = this;
        TabsManager.addSubscriber(this, 'load', function() {
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

      this._setBrowserKeyHandler();

      // ___ Dev Menu
      this.addDevMenu();

      iQ("#reset").click(function(){
        self.reset();
      });

      iQ("#feedback").click(function(){
        self.newTab('http://feedback.mozillalabs.com/forums/56804-tabcandy');
      });

      iQ(window).bind('beforeunload', function() {
        // Things may not all be set up by now, so check for everything
        if (self.showChrome)
          self.showChrome();

        if (self.tabBar && self.tabBar.showAllTabs)
          self.tabBar.showAllTabs();
      });

      // ___ Page
      var currentWindow = Utils.getCurrentWindow();
      Page.init();

      currentWindow.addEventListener(
        "tabcandyshow", function() {
          Page.hideChrome();
          Page.showTabCandy();
        }, false);

      currentWindow.addEventListener(
        "tabcandyhide", function() {
          var activeTab = Page.getActiveTab();
          if (activeTab) {
            activeTab.zoomIn();
          }
        }, false);

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
      // ___ Storage
      var currentWindow = Utils.getCurrentWindow();

      var data = Storage.readUIData(currentWindow);
      this.storageSanity(data);

      var groupsData = Storage.readGroupsData(currentWindow);
      var firstTime = !groupsData || iQ.isEmptyObject(groupsData);
      var groupData = Storage.readGroupData(currentWindow);
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
        box.width = Math.min(box.width * 0.667, pageBounds.width - (infoWidth + padding));
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
            + "<video src='http://html5demos.com/assets/dizzy.ogv' width='100%' preload controls>"
          + "</div>";

        box.left = box.right + padding;
        box.width = infoWidth;
        box.height = infoHeight;
        var infoItem = new InfoItem(box);
        infoItem.html(html);
      }

      // ___ resizing
      if (data.pageBounds) {
        this.pageBounds = data.pageBounds;
        this.resize(true);
      } else
        this.pageBounds = Items.getPageBounds();

      var self = this;
      iQ(window).resize(function() {
        self.resize();
      });

      // ___ show tab candy at startup
      var visibilityData = Storage.readVisibilityData(currentWindow);
      if (visibilityData && visibilityData.visible) {
        var currentTab = UI.currentTab;
        var item;

        if (currentTab && currentTab.mirror)
          item = TabItems.getItemByTabElement(currentTab.mirror.el);

        if (item)
          item.setZoomPrep(false);
        else
          Page.stopZoomPreparation = true;

        Page.hideChrome();
      } else
        Page.showChrome();

      // ___ setup observer to save canvas images
      Components.utils.import("resource://gre/modules/Services.jsm");
      var observer = {
        observe : function(subject, topic, data) {
          if (topic == "quit-application-requested") {
            if (Page.isTabCandyVisible()) {
              TabItems.saveAll(true);
              self.saveVisibility(true);
            } else {
              self.saveVisibility(false);
            }
          }
        }
      };
      Services.obs.addObserver(
        observer, "quit-application-requested", false);

      // ___ Done
      this.initialized = true;
      this.save(); // for this.pageBounds
    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: setBrowserKeyHandler
  // Overrides the browser's keys for navigating between tabs (outside of the TabCandy UI),
  // so they do the right thing in respect to groups.
  _setBrowserKeyHandler : function() {
    var self = this;
    var currentWin = Utils.getCurrentWindow();
    var tabbox = currentWin.gBrowser.mTabBox;

    currentWin.addEventListener("keypress", function(event) {
      if (Page.isTabCandyVisible()) {
        return;
      }
      var handled = false;
      // based on http://mxr.mozilla.org/mozilla1.9.2/source/toolkit/content/widgets/tabbox.xml#145
      switch (event.keyCode) {
        case event.DOM_VK_TAB:
          if (event.ctrlKey && !event.altKey && !event.metaKey)
            if (tabbox.tabs && tabbox.handleCtrlTab) {
              event.stopPropagation();
              event.preventDefault();
              self.advanceSelectedTab(event.shiftKey);
              handled = true;
            }
          break;
        case event.DOM_VK_PAGE_UP:
          if (event.ctrlKey && !event.shiftKey && !event.altKey &&
              !event.metaKey)
            if (tabbox.tabs && tabbox.handleCtrlPageUpDown) {
              event.stopPropagation();
              event.preventDefault();
              self.advanceSelectedTab(true);
              handled = true;
            }
            break;
        case event.DOM_VK_PAGE_DOWN:
          if (event.ctrlKey && !event.shiftKey && !event.altKey &&
              !event.metaKey)
            if (tabbox.tabs && tabbox.handleCtrlPageUpDown) {
              event.stopPropagation();
              event.preventDefault();
              self.advanceSelectedTab(false);
              handled = true;
            }
            break;
#ifdef XP_MACOSX
        case event.DOM_VK_LEFT:
        case event.DOM_VK_RIGHT:
          if (event.metaKey && event.altKey && !event.shiftKey &&
              !event.ctrlKey)
            if (tabbox.tabs && tabbox._handleMetaAltArrows) {
              var reverse =
                (window.getComputedStyle(tabbox, "").direction ==
                  "ltr" ? true : false);
              event.stopPropagation();
              event.preventDefault();
              self.advanceSelectedTab(
                event.keyCode == event.DOM_VK_LEFT ? reverse : !reverse);
              handled = true;
            }
            break;
#endif
      }

      if (!handled) {
        var charCode = event.charCode;
#ifdef XP_MACOSX
        // if a text box in a webpage has the focus, the event.altKey would
        // returns false but we are depending on the charCode here.
        if (charCode == 160) { // alt + space
#else
        if (event.ctrlKey && !event.metaKey && !event.shiftKey &&
            !event.altKey && charCode == 32) {
#endif
          event.stopPropagation();
          event.preventDefault();
          Page.hideChrome();
          Page.showTabCandy();
          return;
        }

#ifdef XP_UNIX
#ifndef XP_MACOSX
        if (event.altKey && !event.metaKey && !event.shiftKey &&
            !event.ctrlKey) {
#else
        if (event.metaKey && !event.altKey && !event.shiftKey &&
            !event.ctrlKey) {
#endif
#else
        if (event.ctrlKey && !event.altKey && !event.shiftKey &&
            !event.metaKey) {
#endif
          // ToDo: the "tabs" binding implements the nsIDOMXULSelectControlElement,
          // we might need to rewrite the tabs without using the
          // nsIDOMXULSelectControlElement.
          // http://mxr.mozilla.org/mozilla1.9.2/source/toolkit/content/widgets/tabbox.xml#246
          // The below handles the ctrl/meta + number key and prevent the default
          // actions.
          // 1 to 9
          if (48 < charCode && charCode < 58) {
            event.stopPropagation();
            event.preventDefault();
            self.advanceSelectedTab(false, (charCode - 48));
          }
#ifdef XP_MACOSX
          // Cmd + { / Cmd + }
          else if (charCode == 91 || charCode == 93) {
              event.stopPropagation();
              event.preventDefault();
              var reverse =
                (window.getComputedStyle(tabbox, "").direction ==
                  "ltr" ? true : false);
              self.advanceSelectedTab(charCode == 91 ? reverse : !reverse);
          }
#endif
        }
      }
    }, true);
  },

  // ----------
  // Function: advanceSelectedTab
  // Moves you to the next tab in the current group's tab strip
  // (outside the TabCandy UI).
  //
  // Parameters:
  //   reverse - true to go to previous instead of next
  //   index - go to a particular tab; numerical value from 1 to 9
  advanceSelectedTab : function(reverse, index) {
    Utils.assert('reverse should be false when index exists', !index || !reverse);
    var tabbox = Utils.getCurrentWindow().gBrowser.mTabBox;
    var tabs = tabbox.tabs;
    var visibleTabs = [];
    var selectedIndex;

    for (var i = 0; i < tabs.childNodes.length ; i++) {
      var tab = tabs.childNodes[i];
      if (!tab.collapsed) {
        visibleTabs.push(tab);
        if (tabs.selectedItem == tab) {
          selectedIndex = (visibleTabs.length - 1);
        }
      }
    }

    // reverse should be false when index exists.
    if (index && index > 0) {
      if (visibleTabs.length > 1) {
        if (visibleTabs.length >= index && index < 9) {
          tabs.selectedItem = visibleTabs[index - 1];
        } else {
          tabs.selectedItem = visibleTabs[visibleTabs.length - 1];
        }
      }
    } else {
      if (visibleTabs.length > 1) {
        if (reverse) {
          tabs.selectedItem =
            (selectedIndex == 0) ? visibleTabs[visibleTabs.length - 1] :
              visibleTabs[selectedIndex - 1]
        } else {
          tabs.selectedItem =
            (selectedIndex == (visibleTabs.length - 1)) ? visibleTabs[0] :
              visibleTabs[selectedIndex + 1];
        }
      }
    }
  },

  // ----------
  // Function: resize
  // Update the TabCandy UI contents in response to a window size change.
  // Won't do anything if it doesn't deem the resize necessary.
  //
  // Parameters:
  //   force - true to update even when "unnecessary"; default false
  resize: function(force) {
    if ( typeof(force) == "undefined" ) force = false;

    // If we are currently doing an animation or if TabCandy isn't focused
    // don't perform a resize. This resize really slows things down.
    var isAnimating = iQ.isAnimating();
    if ( !force && ( isAnimating || !Page.isTabCandyVisible() ) ) {
      // TODO: should try again once the animation is done
      // Actually, looks like iQ.isAnimating is non-functional;
      // perhaps we should clean it out, or maybe we should fix it.
      return;
    }

    var oldPageBounds = new Rect(this.pageBounds);
    var newPageBounds = Items.getPageBounds();
    if (newPageBounds.equals(oldPageBounds))
      return;

    var items = Items.getTopLevelItems();

    // compute itemBounds: the union of all the top-level items' bounds.
    var itemBounds = new Rect(this.pageBounds); // We start with pageBounds so that we respect
                                                // the empty space the user has left on the page.
    itemBounds.width = 1;
    itemBounds.height = 1;
    items.forEach(function(item) {
      if (item.locked.bounds)
        return;

      var bounds = item.getBounds();
      itemBounds = (itemBounds ? itemBounds.union(bounds) : new Rect(bounds));
    });

    Groups.repositionNewTabGroup(); // TODO:

    if (newPageBounds.width < this.pageBounds.width && newPageBounds.width > itemBounds.width)
      newPageBounds.width = this.pageBounds.width;

    if (newPageBounds.height < this.pageBounds.height && newPageBounds.height > itemBounds.height)
      newPageBounds.height = this.pageBounds.height;

    var wScale;
    var hScale;
    if ( Math.abs(newPageBounds.width - this.pageBounds.width)
         > Math.abs(newPageBounds.height - this.pageBounds.height) ) {
      wScale = newPageBounds.width / this.pageBounds.width;
      hScale = newPageBounds.height / itemBounds.height;
    } else {
      wScale = newPageBounds.width / itemBounds.width;
      hScale = newPageBounds.height / this.pageBounds.height;
    }

    var scale = Math.min(hScale, wScale);
    var self = this;
    var pairs = [];
    items.forEach(function(item) {
      if (item.locked.bounds)
        return;

      var bounds = item.getBounds();

      bounds.left += newPageBounds.left - self.pageBounds.left;
      bounds.left *= scale;
      bounds.width *= scale;

      bounds.top += newPageBounds.top - self.pageBounds.top;
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

    this.pageBounds = Items.getPageBounds();
    this.save();
  },

  // ----------
  // Function: addDevMenu
  // Fills out the "dev menu" in the TabCandy UI.
  addDevMenu: function() {
    try {
      var self = this;

      var $select = iQ('<select>')
        .css({
          position: 'absolute',
          bottom: 5,
          right: 5,
          zIndex: 99999,
          opacity: .2
        })
        .appendTo('#content')
        .change(function () {
          var index = iQ(this).val();
          try {
            commands[index].code.apply(commands[index].element);
          } catch(e) {
            Utils.log('dev menu error', e);
          }
          iQ(this).val(0);
        });

      var commands = [{
        name: 'dev menu',
        code: function() {
        }
      }, {
        name: 'show trenches',
        code: function() {
          Trenches.toggleShown();
          iQ(this).html((Trenches.showDebug ? 'hide' : 'show') + ' trenches');
        }
      }, {
        name: 'refresh',
        code: function() {
          location.href = 'tabcandy.html';
        }
      }, {
        name: 'reset',
        code: function() {
          self.reset();
        }
      }, {
        name: 'code docs',
        code: function() {
          self.newTab('http://hg.mozilla.org/labs/tabcandy/raw-file/tip/content/doc/index.html');
        }
      }, {
        name: 'save',
        code: function() {
          self.saveAll();
        }
      }, {
        name: 'group sites',
        code: function() {
          self.arrangeBySite();
        }
      }];

      var count = commands.length;
      var a;
      for (a = 0; a < count; a++) {
        commands[a].element = iQ('<option>')
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
  // Function: reset
  // Wipes all TabCandy storage and refreshes, giving you the "first-run" state.
  reset: function() {
    Storage.wipe();
    location.href = '';
  },

  // ----------
  // Function: saveAll
  // Saves all data associated with TabCandy.
  // TODO: Save info items
  saveAll: function() {
    this.save();
    Groups.saveAll();
    TabItems.saveAll();
  },

  // ----------
  // Function: save
  // Saves the data for this object to persistent storage
  save: function() {
    if (!this.initialized)
      return;

    var data = {
      pageBounds: this.pageBounds
    };

    if (this.storageSanity(data))
      Storage.saveUIData(Utils.getCurrentWindow(), data);
  },

  // ----------
  // Function: storageSanity
  // Given storage data for this object, returns true if it looks valid.
  storageSanity: function(data) {
    if (iQ.isEmptyObject(data))
      return true;

    if (!isRect(data.pageBounds)) {
      Utils.log('UI.storageSanity: bad pageBounds', data.pageBounds);
      data.pageBounds = null;
      return false;
    }

    return true;
  },

  // ----------
  // Function: saveVisibility
  // Saves to storage whether the TabCandy UI is visible (as passed in).
  saveVisibility: function(isVisible) {
    Storage.saveVisibilityData(Utils.getCurrentWindow(), { visible: isVisible });
  },

  // ----------
  // Function: arrangeBySite
  // Blows away all existing groups and organizes the tabs into new groups based on domain.
  arrangeBySite: function() {
    function putInGroup(set, key) {
      var group = Groups.getGroupWithTitle(key);
      if (group) {
        set.forEach(function(el) {
          group.add(el);
        });
      } else
        new Group(set, {dontPush: true, dontArrange: true, title: key});
    }

    Groups.removeAll();

    var newTabsGroup = Groups.getNewTabGroup();
    var groups = [];
    var items = TabItems.getItems();
    items.forEach(function(item) {
      var url = item.getURL();
      var domain = url.split('/')[2];
      if (!domain)
        newTabsGroup.add(item);
      else {
        var domainParts = domain.split('.');
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

    putInGroup(leftovers, 'mixed');

    Groups.arrange();
  },

  // ----------
  // Function: newTab
  // Opens a new tab with the given URL.
  newTab: function(url) {
    try {
      var group = Groups.getNewTabGroup();
      if (group)
        group.newTab(url);
      else
        Tabs.open(url);
    } catch(e) {
      Utils.log(e);
    }
  }
};

// ----------
window.UI = new UIClass();
window.UI.init();

})();
