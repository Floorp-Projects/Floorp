// Title: ui.js (revision-a)

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
      setTimeout(function() {
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
      setTimeout(function() {
        self.hide();
      }, 300); 
    }
  },
}

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
  showOnlyTheseTabs: function(tabs){
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
      Utils.activeWindow.gBrowser.moveTabTo(tab, self.el.children.length-1);
    });
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
    Utils.activeWindow.document.getElementById("main-window").setAttribute("activetitlebarcolor", "#C4C4C4");
  },
  
  showChrome: function(){
    Tabbar.show();  
    Navbar.show();    
    window.statusbar.visible = true;
    
    // Mac Only
    Utils.activeWindow.document.getElementById("main-window").removeAttribute("activetitlebarcolor");     
  },
  
  setupKeyHandlers: function(){
    var self = this;
    $(window).keyup(function(e){
      if( e.metaKey == false ) window.Keys.meta = false;
    });
    
    $(window).keydown(function(e){
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
          if( Keys.meta ) if( self.getActiveTab() ) self.getActiveTab().zoom();
          break;
      }
      
      if( norm != null && $(":focus").length == 0 ){
        var nextTab = getClosestTabBy(norm);
        if( nextTab ){
          if( nextTab.inStack() && !nextTab.parent.expanded){
            nextTab = nextTab.parent.getChild(0);
          }
          self.setActiveTab(nextTab);           
        }
        e.preventDefault();               
      }
      
      if((e.which == 27 || e.which == 13) && $(":focus").length == 0 )
        if( self.getActiveTab() ) self.getActiveTab().zoom();
      
      
       
    });
    
  },
    
  // ----------  
  init: function() {
    var self = this;
    Utils.homeTab.raw.maxWidth = 60;
    Utils.homeTab.raw.minWidth = 60;
        
    // When you click on the background/empty part of TabCandy
    // we create a new group.
    $(Utils.homeTab.contentDocument).mousedown(function(e){
      if( e.originalTarget.id == "bg" )
        Page.createGroupOnDrag(e)
    })

    this.setupKeyHandlers();
        
    Tabs.onClose(function(){
      setTimeout(function() { // Marshal event from chrome thread to DOM thread
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
    
    Tabs.onFocus(function() {
      var focusTab = this;

      // If we switched to TabCandy window...
      if( focusTab.contentWindow == window ){
        var currentTab = UI.currentTab;
        if(currentTab != null && currentTab.mirror != null) {
          // If there was a previous currentTab we want to animate
          // its mirror for the zoom out.
          
          // Zoom out!
          var mirror = currentTab.mirror;
          var $tab = $(mirror.el);
          var item = TabItems.getItemByTab(mirror.el);
          self.setActiveTab(item);
          
          var rotation = $tab.css("-moz-transform");
          var [w,h, pos, z] = [$tab.width(), $tab.height(), $tab.position(), $tab.css("zIndex")];
          var scale = window.innerWidth / w;
  
          var overflow = $("body").css("overflow");
          $("body").css("overflow", "hidden");
          
          TabMirror.pausePainting();
          $tab.css({
              top: 0, left: 0,
              width: window.innerWidth,
              height: h * (window.innerWidth/w),
              zIndex: 999999,
              '-moz-transform': 'rotate(0deg)'
          }).animate({
              top: pos.top, left: pos.left,
              width: w, height: h
          },350, '', function() { // note that this will happen on the DOM thread
            $tab.css({
              zIndex: z,
              '-moz-transform': rotation
            });
            $("body").css("overflow", overflow);
            var activeGroup = Groups.getActiveGroup();
            if( activeGroup ) activeGroup.reorderBasedOnTabOrder(item);        
    
            window.Groups.setActiveGroup(null);
            TabMirror.resumePainting();        
            UI.resize(true);
          });
        }
      } else { // switched to another tab
        setTimeout(function() { // Marshal event from chrome thread to DOM thread
          var item = TabItems.getItemByTab(Utils.activeTab);
          if(item) 
            Groups.setActiveGroup(item.parent);
            
          UI.tabBar.show();        
        }, 1);
      }
      
      UI.currentTab = focusTab;
    });
  },
  
  // ----------  
  createGroupOnDrag: function(e){
/*     e.preventDefault(); */
    const minSize = 60;
    
    var startPos = {x:e.clientX, y:e.clientY}
    var phantom = $("<div class='group phantom'>").css({
      position: "absolute",
      top: startPos.y,
      left: startPos.x,
      width: 0,
      height: 0,
      opacity: .7,
      zIndex: -1,
      cursor: "default"
    }).appendTo("body");
    
    function updateSize(e){
      var css = {width: e.clientX-startPos.x, height:e.clientY-startPos.y}
      if( css.width > minSize || css.height > minSize ) css.opacity = 1;
      else css.opacity = .7
      
      phantom.css(css);
      e.preventDefault();     
    }
    
    function collapse(){
      phantom.animate({
        width: 0,
        height: 0,
        top: phantom.position().top + phantom.height()/2,
        left: phantom.position().left + phantom.width()/2
      }, 300, function(){
        phantom.remove();
      })
    }
    
    function finalize(e){
      $("#bg, .phantom").unbind("mousemove");
      if( phantom.css("opacity") != 1 ) collapse();
      else{
        var bounds = new Rect(startPos.x, startPos.y, phantom.width(), phantom.height())

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
    
    $("#bg, .phantom").mousemove(updateSize)
    $(window).one('mouseup', finalize);
    e.preventDefault();  
    return false;
  },
  
  // ----------
  // Function: setActiveTab
  // Sets the currently active tab. The idea of a focused tab is useful
  // for keyboard navigation and returning to the last zoomed-in tab.
  // Hiting return/esc brings you to the focused tab, and using the
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
      
      // ___ Dev Menu
      this.addDevMenu();

      // ___ Navbar
      if(this.focused) {
        Page.hideChrome();
      }
      
      Tabs.onFocus(function() {
        var me = this;
        setTimeout(function() { // Marshal event from chrome thread to DOM thread
          try{
            if(me.contentWindow == window) {
              self.focused = true;
              Page.hideChrome();
            } else {
              self.focused = false;
              Page.showChrome();
            }
          }catch(e){
            Utils.log(e)
          }
        }, 1);
      });
    
      Tabs.onOpen(function(a, b) {
        setTimeout(function() { // Marshal event from chrome thread to DOM thread
          self.navBar.show();
        }, 1);
      });
    
      $(window).bind('beforeunload', function() {
        self.showChrome();  
        self.tabBar.showAllTabs();
      });
      
      // ___ Page
      Page.init();
      
      // ___ Storage
      var currentWindow = Utils.getCurrentWindow();
      
      var data = Storage.readUIData(currentWindow);
      this.storageSanity(data);
       
      var groupsData = Storage.readGroupsData(currentWindow);
      var firstTime = !groupsData || $.isEmptyObject(groupsData);
      var groupData = Storage.readGroupData(currentWindow);
      Groups.reconstitute(groupsData, groupData);
      
      TabItems.init();
      
      if(firstTime) {
        var items = TabItems.getItems();
        $.each(items, function(index, item) {
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
      
      $(window).resize(function() {
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
  resize: function(force) {
    if( typeof(force) == "undefined" ) force = false;

    // If we are currently doing an animation or if TabCandy isn't focused
    // don't perform a resize. This resize really slows things down.
    var isAnimating = $(":animated").length > 0;
    if( force == false){
      if( isAnimating || !Page.isTabCandyFocused() ) return;   }   
        
    var items = Items.getTopLevelItems();
    var itemBounds = new Rect(this.pageBounds);
    itemBounds.width = 1;
    itemBounds.height = 1;
    $.each(items, function(index, item) {
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
    $.each(items, function(index, item) {
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
    
    $.each(pairs, function(index, pair) {
      pair.item.setBounds(pair.bounds, true);
    });

    this.pageBounds = Items.getPageBounds();
    this.save();
  },
  
  // ----------
  addDevMenu: function() {
    var self = this;
    
    var html = '<select style="position:absolute; bottom:5px; right:5px; opacity:.2;">'; 
    var $select = $(html)
      .appendTo('body')
      .change(function () {
        var index = $(this).val();
        try {
          commands[index].code();
        } catch(e) {
          Utils.log('dev menu error', e);
        }
        $(this).val(0);
      });
      
    var commands = [{
      name: 'dev menu', 
      code: function() {
      }
    }, {
      name: 'home', 
      code: function() {
        location.href = '../../index.html';
      }
    }, {
      name: 'code docs', 
      code: function() {
        location.href = '../../doc/index.html';
      }
    }, {
      name: 'save', 
      code: function() {
        self.saveAll();
      }
    }, {
      name: 'reset', 
      code: function() {
        Storage.wipe();
        location.href = '';
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
      html = '<option value="'
        + a
        + '">'
        + commands[a].name
        + '</option>';
        
      $select.append(html);
    }
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
    if($.isEmptyObject(data))
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

