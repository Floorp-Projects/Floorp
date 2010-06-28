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
Navbar = {
  // ----------
  get el(){
    var win = Utils.getCurrentWindow();
    if(win) {
      var navbar = win.gBrowser.ownerDocument.getElementById("navigator-toolbox");
      return navbar;      
    }

    return null;
  },
  
  get urlBar(){
    var win = Utils.getCurrentWindow();
    if(win) {
      var navbar = win.gBrowser.ownerDocument.getElementById("urlbar");
      return navbar;      
    }

    return null;    
  },

  // ----------
  show: function() {    
    var el = this.el;
    if(el)
      el.collapsed = false; 
    else { // needs a little longer to get going
      var self = this;
      iQ.timeout(function() {
        self.show();
      }, 300); 
    }
  },

  // ----------
  hide: function() {    
    var el = this.el;
    if(el)
      el.collapsed = true; 
    else { // needs a little longer to get going
      var self = this;
      iQ.timeout(function() {
        self.hide();
      }, 300); 
    }
  },
};

// ##########
// Class: Tabbar
// Singleton for managing the tabbar of the browser. 
var Tabbar = {
  // ----------
  // Variable: _hidden
  // We keep track of whether the tabs are hidden in this (internal) variable
  // so we still have access to that information during the window's unload event,
  // when window.Tabs no longer exists.
  _hidden: false, 
  
  // ----------
  get el() {
    return window.Tabs[0].raw.parentNode; 
  },

  // ----------
  get height() {
    return window.Tabs[0].raw.parentNode.getBoundingClientRect().height;
  },

  // ----------
  hide: function() {
    var self = this;
    self.el.collapsed = true;    
  },

  // ----------
  show: function() {
    this.el.collapsed = false;
  },
  
  // ----------
  // Function: getVisibleTabs
  // Returns an array of the tabs which are currently visibible in the
  // tab bar.
  getVisibleTabs: function(){
    var visibleTabs = [];
    // this.el.children is not a real array and does contain
    // useful functions like filter or forEach. Convert it into a real array.
    for( var i=0; i<this.el.children.length; i++ ){
      var tab = this.el.children[i];
      if( tab.collapsed == false )
        visibleTabs.push();
    }
    
    return visibleTabs;
  },
  
  // ----------
  // Function: showOnlyTheseTabs
  // Hides all of the tabs in the tab bar which are not passed into this function.
  //
  // Paramaters
  //  - An array of <Tab> objects.
  showOnlyTheseTabs: function(tabs, options){
    try { 
      if(!options)
        options = {};
          
      var visibleTabs = [];
      // this.el.children is not a real array and does contain
      // useful functions like filter or forEach. Convert it into a real array.
      var tabBarTabs = [];
      for( var i=0; i<this.el.children.length; i++ ){
        tabBarTabs.push(this.el.children[i]);
      }
      
      for each( var tab in tabs ){
        var rawTab = tab.tab.raw;
        var toShow = tabBarTabs.filter(function(testTab){
          return testTab == rawTab;
        }); 
        visibleTabs = visibleTabs.concat( toShow );
      }
  
      tabBarTabs.forEach(function(tab){
        tab.collapsed = true;
      });
      
      // Show all of the tabs in the group and move them (in order)
      // that they appear in the group to the end of the tab strip.
      // This way the tab order is matched up to the group's thumbnail
      // order.
      var self = this;
      visibleTabs.forEach(function(tab){
        tab.collapsed = false;
        
        if(!options.dontReorg)
          Utils.getCurrentWindow().gBrowser.moveTabTo(tab, self.el.children.length-1);
      });
    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: showAllTabs
  // Shows all of the tabs in the tab bar.
  showAllTabs: function(){
    for( var i=0; i<this.el.children.length; i++ ){
      var tab = this.el.children[i];
      tab.collapsed = false;
    }
  },

  // ----------
  get isHidden(){ return this.el.collapsed; }
}

// ##########
// Class: Page
// Singleton top-level UI manager. TODO: Integrate with <UIClass>.  
window.Page = {
  startX: 30, 
  startY: 70,
  
  show: function(){
    Utils.homeTab.focus();
    this.hideChrome();
  },
  
  isTabCandyFocused: function(){
    return Utils.homeTab.contentDocument == UI.currentTab.contentDocument;    
  },
  
  hideChrome: function(){
    Tabbar.hide();
    Navbar.hide();
    window.statusbar.visible = false;  
        
    // Mac Only
    Utils.getCurrentWindow().document.getElementById("main-window").setAttribute("activetitlebarcolor", "#C4C4C4");
  },
  
  showChrome: function(){
    Tabbar.show();  
    Navbar.show();    
    window.statusbar.visible = true;
    
    this.setCloseButtonOnTabs();
    
    // Mac Only
    Utils.getCurrentWindow().document.getElementById("main-window").removeAttribute("activetitlebarcolor");     
  },
  
  setCloseButtonOnTabs : function() {
    // TODO: we will need to modify the adjustTabstrip() to fix this when merging
    // this extension to Firefox.
    // http://mxr.mozilla.org/mozilla1.9.2/source/browser/base/content/tabbrowser.xml#3050
    iQ.timeout(function() { // iQ.timeout adds a try/catch to setTimeout
      var tabContainer = Utils.getCurrentWindow().gBrowser.tabContainer;
      if (tabContainer.mCloseButtons == 1 &&
          tabContainer.getAttribute("overflow") != "true") {
        tabContainer.setAttribute("closebuttons", "alltabs");
      };
    }, 50);
  },
  
  setupKeyHandlers: function(){
    var self = this;
    iQ(window).keyup(function(e){
      if( e.metaKey == false ) window.Keys.meta = false;
    });
    
    iQ(window).keydown(function(e){
      if( e.metaKey == true ) window.Keys.meta = true;
      
      if( !self.getActiveTab() ) return;
      
      var centers = [[item.bounds.center(), item] for each(item in TabItems.getItems())];
      myCenter = self.getActiveTab().bounds.center();

      function getClosestTabBy(norm){
        var matches = centers
          .filter(function(item){return norm(item[0], myCenter)})
          .sort(function(a,b){
            return myCenter.distance(a[0]) - myCenter.distance(b[0]);
          });
        if( matches.length > 0 ) return matches[0][1];
        else return null;
      }

      var norm = null;
      switch(e.which){
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
        case 49: // Command-1
        case 69: // Command-E
          if( Keys.meta ) if( self.getActiveTab() ) self.getActiveTab().zoomIn();
          break;
      }
      
      if( norm != null && iQ(":focus").length == 0 ){
        var nextTab = getClosestTabBy(norm);
        if( nextTab ){
          if( nextTab.inStack() && !nextTab.parent.expanded){
            nextTab = nextTab.parent.getChild(0);
          }
          self.setActiveTab(nextTab);           
        }
        e.preventDefault();               
      }
      
      if((e.which == 27 || e.which == 13) && iQ(":focus").length == 0 )
        if( self.getActiveTab() ) self.getActiveTab().zoomIn();
      
      
       
    });
    
  },
    
  // ----------  
  init: function() {
    var self = this;
        
    // When you click on the background/empty part of TabCandy
    // we create a new group.
    iQ(Utils.homeTab.contentDocument).mousedown(function(e){
      if( e.originalTarget.id == "bg" )
        Page.createGroupOnDrag(e)
    })

    this.setupKeyHandlers();
        
    Tabs.onClose(function(){
      iQ.timeout(function() { // Marshal event from chrome thread to DOM thread
        Page.setCloseButtonOnTabs();        
          
        // Only go back to the TabCandy tab when there you close the last
        // tab of a group.
        var group = Groups.getActiveGroup();
        if( group && group._children.length == 0 )
          Page.show();
  
        // Take care of the case where you've closed the last tab in
        // an un-named group, which means that the group is gone (null) and
        // there are no visible tabs.
        if( group == null && Tabbar.getVisibleTabs().length == 0){
          Page.show();
        }
      }, 1);

      return false;
    });
    
    Tabs.onMove(function() {
      iQ.timeout(function() { // Marshal event from chrome thread to DOM thread
        var activeGroup = Groups.getActiveGroup();
        if( activeGroup )
          activeGroup.reorderBasedOnTabOrder();                
      }, 1);
    });
    
    Tabs.onFocus(function() {
      var focusTab = this;
      var currentTab = UI.currentTab;
      
      // If we switched to TabCandy window...
      if( focusTab.contentWindow == window ){
        UI.focused = true;
        Page.hideChrome();

        var item = null;
        if(currentTab && currentTab.mirror)
          item = TabItems.getItemByTabElement(currentTab.mirror.el);
        
        if(item) {
          // If there was a previous currentTab we want to animate
          // its mirror for the zoom out.
          // Note that we start the animation on the chrome thread.
          
          // Zoom out!
          item.zoomOut(function() {
            if(!currentTab.mirror) // if the tab's been destroyed 
              item = null;
            
            self.setActiveTab(item);
            
            var activeGroup = Groups.getActiveGroup();
            if( activeGroup )
              activeGroup.setTopChild(item);        
    
            window.Groups.setActiveGroup(null);
            UI.resize(true);
          });
        }
      } else { // switched to another tab
        UI.focused = false;
        Page.showChrome();

        iQ.timeout(function() { // Marshal event from chrome thread to DOM thread
          if(focusTab != UI.currentTab) // things have changed while we were in timeout
            return;
            
          var newItem = null;
          if(focusTab && focusTab.mirror)
            newItem = TabItems.getItemByTabElement(focusTab.mirror.el);

          if(newItem) 
            Groups.setActiveGroup(newItem.parent);
            
          UI.tabBar.show();  
          
          // ___ prepare for when we return to TabCandy
          var oldItem = null;
          if(currentTab && currentTab.mirror)
            oldItem = TabItems.getItemByTabElement(currentTab.mirror.el);
            
          if(newItem != oldItem) {
            if(oldItem)
              oldItem.setZoomPrep(false);

            if(newItem)
              newItem.setZoomPrep(true);
          }
        }, 1);
      }
      
      UI.currentTab = focusTab;
    });
  },
  
  // ----------  
  createGroupOnDrag: function(e){
/*     e.preventDefault(); */
    const minSize = 60;
    const minMinSize = 15;
    
    var startPos = {x:e.clientX, y:e.clientY}
    var phantom = iQ("<div>")
      .addClass('group phantom')
      .css({
        position: "absolute",
        top: startPos.y,
        left: startPos.x,
        width: 0,
        height: 0,
        opacity: .7,
        zIndex: -1,
        cursor: "default"
      })
      .appendTo("body");
    
    function updateSize(e){
      var box = new Rect();
      box.left = Math.min(startPos.x, e.clientX);
      box.right = Math.max(startPos.x, e.clientX);
      box.top = Math.min(startPos.y, e.clientY);
      box.bottom = Math.max(startPos.y, e.clientY);

      var css = box.css();      
      if(css.width > minMinSize && css.height > minMinSize
          && (css.width > minSize || css.height > minSize)) 
        css.opacity = 1;
      else 
        css.opacity = .7
      
      phantom.css(css);
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
      if( phantom.css("opacity") != 1 ) 
        collapse();
      else{
        var bounds = phantom.bounds();

        // Add all of the orphaned tabs that are contained inside the new group
        // to that group.
        var tabs = Groups.getOrphanedTabs();
        var insideTabs = [];
        for each( tab in tabs ){
          if( bounds.contains( tab.bounds ) ){
            insideTabs.push(tab);
          }
        }
        
        var group = new Group(insideTabs,{bounds:bounds});
        phantom.remove();
      }
    }
    
    iQ(window).mousemove(updateSize)
    iQ(window).one('mouseup', finalize);
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
    if(tab == this._activeTab)
      return;
      
    if(this._activeTab) { 
      this._activeTab.makeDeactive();
      this._activeTab.removeOnClose(this);
    }
      
    this._activeTab = tab;
    
    if(this._activeTab) {
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
function UIClass(){ 
  if(window.Tabs) 
    this.init();
  else { 
    var self = this;
    TabsManager.addSubscriber(this, 'load', function() {
      self.init();
    });
  }
};

// ----------
UIClass.prototype = {
  // ----------
  init: function() {
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
      
      // Variable: focused
      // Keeps track of whether Tab Candy is focused. 
      this.focused = (Utils.activeTab == Utils.homeTab);
      
      var self = this;
      
      this.setBrowserKeyHandler();
      
      // ___ Dev Menu
      this.addDevMenu();

      iQ("#reset").click(function(){
        self.reset();
      });
      
      // ___ Navbar
      if(this.focused) {
        Page.hideChrome();
      }
      
      Tabs.onOpen(function(a, b) {
        iQ.timeout(function() { // Marshal event from chrome thread to DOM thread
          self.navBar.show();
          Page.setCloseButtonOnTabs();
        }, 1);
      });
    
      iQ(window).bind('beforeunload', function() {
        // Things may not all be set up by now, so check for everything
        if(self.showChrome)
          self.showChrome();  
          
        if(self.tabBar && self.tabBar.showAllTabs)
          self.tabBar.showAllTabs();
      });
      
      // ___ Page
      var currentWindow = Utils.getCurrentWindow();
      Page.init();
      currentWindow.addEventListener(
        "resize", function() { Page.setCloseButtonOnTabs(); }, false)
      
      // ___ Storage
      var data = Storage.readUIData(currentWindow);
      this.storageSanity(data);
       
      var groupsData = Storage.readGroupsData(currentWindow);
      var firstTime = !groupsData || iQ.isEmptyObject(groupsData);
      var groupData = Storage.readGroupData(currentWindow);
      Groups.reconstitute(groupsData, groupData);
      
      TabItems.init();
      
      if(firstTime) {
        var items = TabItems.getItems();
        iQ.each(items, function(index, item) {
          if(item.parent)
            item.parent.remove(item);
        });
            
        var box = Items.getPageBounds();
        box.inset(10, 10);
        var options = {padding: 10};
        Items.arrange(items, box, options);
      } else
        TabItems.reconstitute();
      
      // ___ resizing
      if(data.pageBounds) {
        this.pageBounds = data.pageBounds;
        this.resize(true);
      } else 
        this.pageBounds = Items.getPageBounds();    
      
      iQ(window).resize(function() {
        self.resize();
      });
            
      // ___ Done
      this.initialized = true;
      this.save(); // for this.pageBounds
    }catch(e) {
      Utils.log("Error in UIClass(): " + e);
      Utils.log(e.fileName);
      Utils.log(e.lineNumber);
      Utils.log(e.stack);
    }
  }, 
  
  // ----------
  setBrowserKeyHandler : function() {
    var self = this;
    var browser = Utils.getCurrentWindow().gBrowser;
    var tabbox = browser.mTabBox;

    browser.addEventListener("keypress", function(event) {
      var handled = false;
      // based on http://mxr.mozilla.org/mozilla1.9.2/source/toolkit/content/widgets/tabbox.xml#145
      switch (event.keyCode) {
        case event.DOM_VK_TAB:
          if (event.ctrlKey && !event.altKey && !event.metaKey)
            if (tabbox.tabs && tabbox.handleCtrlTab) {
              self.advanceSelectedTab(event.shiftKey);
              event.stopPropagation();
              event.preventDefault();
              handled = true;
            }
          break;
        case event.DOM_VK_PAGE_UP:
          if (event.ctrlKey && !event.shiftKey && !event.altKey &&
              !event.metaKey)
            if (tabbox.tabs && tabbox.handleCtrlPageUpDown) {
              self.advanceSelectedTab(true);
              event.stopPropagation();
              event.preventDefault();
              handled = true;
            }
            break;
        case event.DOM_VK_PAGE_DOWN:
          if (event.ctrlKey && !event.shiftKey && !event.altKey &&
              !event.metaKey)
            if (tabbox.tabs && tabbox.handleCtrlPageUpDown) {
              self.advanceSelectedTab(false);
              event.stopPropagation();
              event.preventDefault();
              handled = true;
            }
            break;
        case event.DOM_VK_LEFT:
          if (event.metaKey && event.altKey && !event.shiftKey &&
              !event.ctrlKey)
            if (tabbox.tabs && tabbox._handleMetaAltArrows) {
              var reverse =
                window.getComputedStyle(tabbox, "").direction == "ltr" ? -1 : 1;
              self.advanceSelectedTab(reverse);
              event.stopPropagation();
              event.preventDefault();
              handled = true;
            }
            break;
        case event.DOM_VK_RIGHT:
          if (event.metaKey && event.altKey && !event.shiftKey &&
              !event.ctrlKey)
            if (tabbox.tabs && tabbox._handleMetaAltArrows) {
              var forward =
                window.getComputedStyle(tabbox, "").direction == "ltr" ? 1 : -1;
              self.advanceSelectedTab(!forward);
              event.stopPropagation();
              event.preventDefault();
              handled = true;
            }
            break;
      }
      
      if (!handled) {
        // ToDo: the "tabs" binding implements the nsIDOMXULSelectControlElement,
        // we might need to rewrite the tabs without using the
        // nsIDOMXULSelectControlElement.
        // http://mxr.mozilla.org/mozilla1.9.2/source/toolkit/content/widgets/tabbox.xml#246
        // The below handles the ctrl/meta + number key and prevent the default
        // actions.
        var isMac = (navigator.platform.search(/mac/i) > -1);
        
        if ((isMac && event.metaKey) || (!isMac && event.ctrlKey)) {
          var charCode = event.charCode;
          // 1 to 9
          if (48 < charCode && charCode < 58) {
            self.advanceSelectedTab(false, (charCode - 48));
            event.stopPropagation();
            event.preventDefault();
          }
        }
      }
    }, false);
  },
  
  // ----------
  advanceSelectedTab : function(reverse, index) {
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
  resize: function(force) {
    if( typeof(force) == "undefined" ) force = false;

    // If we are currently doing an animation or if TabCandy isn't focused
    // don't perform a resize. This resize really slows things down.
    var isAnimating = iQ.isAnimating();
    if( force == false){
      if( isAnimating || !Page.isTabCandyFocused() ) {
        // TODO: should try again once the animation is done
        // Actually, looks like iQ.isAnimating is non-functional;
        // perhaps we should clean it out, or maybe we should fix it. 
        return;   
      }
    }   
        
    var items = Items.getTopLevelItems();
    var itemBounds = new Rect(this.pageBounds);
    itemBounds.width = 1;
    itemBounds.height = 1;
    iQ.each(items, function(index, item) {
      if(item.locked.bounds)
        return;
        
      var bounds = item.getBounds();
      itemBounds = (itemBounds ? itemBounds.union(bounds) : new Rect(bounds));
    });

    var oldPageBounds = new Rect(this.pageBounds);
    
    var newPageBounds = Items.getPageBounds();
    if(newPageBounds.equals(oldPageBounds))
      return;
      
    Groups.repositionNewTabGroup(); // TODO: 

    if(newPageBounds.width < this.pageBounds.width && newPageBounds.width > itemBounds.width)
      newPageBounds.width = this.pageBounds.width;

    if(newPageBounds.height < this.pageBounds.height && newPageBounds.height > itemBounds.height)
      newPageBounds.height = this.pageBounds.height;

    var wScale;
    var hScale;
    if(Math.abs(newPageBounds.width - this.pageBounds.width)
        > Math.abs(newPageBounds.height - this.pageBounds.height)) {
      wScale = newPageBounds.width / this.pageBounds.width;
      hScale = newPageBounds.height / itemBounds.height;
    } else {
      wScale = newPageBounds.width / itemBounds.width;
      hScale = newPageBounds.height / this.pageBounds.height;
    }
    
    var scale = Math.min(hScale, wScale);
    var self = this;
    var pairs = [];
    iQ.each(items, function(index, item) {
      if(item.locked.bounds)
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
    
    iQ.each(pairs, function(index, pair) {
      pair.item.setBounds(pair.bounds, true);
    });

    this.pageBounds = Items.getPageBounds();
    this.save();
  },
  
  // ----------
  addDevMenu: function() {
    var self = this;
    
    var $select = iQ('<select>')
      .css({
        position: 'absolute',
        bottom: 5,
        right: 5,
        opacity: .2
      })
      .appendTo('body')
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
        location.href = 'index.html';
      }
    }, {
      name: 'code docs', 
      code: function() {
        location.href = 'doc/index.html';
      }
    }, {
      name: 'tests', 
      code: function() {
        location.href = 'test.html';
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
    for(a = 0; a < count; a++) {
      commands[a].element = iQ('<option>')
        .val(a)
        .html(commands[a].name)
        .appendTo($select)
        .get(0);
    }
  },

  // -----------
  reset: function() {
    Storage.wipe();
    location.href = '';      
  },
    
  // ----------
  saveAll: function() {  
    this.save();
    Groups.saveAll();
    TabItems.saveAll();
  },

  // ----------
  save: function() {  
    if(!this.initialized) 
      return;
      
    var data = {
      pageBounds: this.pageBounds
    };
    
    if(this.storageSanity(data))
      Storage.saveUIData(Utils.getCurrentWindow(), data);
  },

  // ----------
  storageSanity: function(data) {
    if(iQ.isEmptyObject(data))
      return true;
      
    if(!isRect(data.pageBounds)) {
      Utils.log('UI.storageSanity: bad pageBounds', data.pageBounds);
      data.pageBounds = null;
      return false;
    }
      
    return true;
  },
  
  // ----------
  arrangeBySite: function() {
    function putInGroup(set, key) {
      var group = Groups.getGroupWithTitle(key);
      if(group) {
        iQ.each(set, function(index, el) {
          group.add(el);
        });
      } else 
        new Group(set, {dontPush: true, dontArrange: true, title: key});      
    }

    Groups.removeAll();
    
    var newTabsGroup = Groups.getNewTabGroup();
    var groups = [];
    var items = TabItems.getItems();
    iQ.each(items, function(index, item) {
      var url = item.getURL(); 
      var domain = url.split('/')[2]; 
      if(!domain)
        newTabsGroup.add(item);
      else {
        var domainParts = domain.split('.');
        var mainDomain = domainParts[domainParts.length - 2];
        if(groups[mainDomain]) 
          groups[mainDomain].push(item.container);
        else 
          groups[mainDomain] = [item.container];
      }
    });
    
    var leftovers = [];
    for(key in groups) {
      var set = groups[key];
      if(set.length > 1) {
        putInGroup(set, key);
      } else
        leftovers.push(set[0]);
    }
    
    putInGroup(leftovers, 'mixed');
    
    Groups.arrange();
  },
  
  // ----------
  newTab: function(url, inBackground) {
    Tabs.open(url, inBackground);
  }
};

// ----------
window.UI = new UIClass();

})();

