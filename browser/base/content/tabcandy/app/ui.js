// Title: ui.js (revision-a)

(function(){

// ##########
Navbar = {
  // ----------
  get el(){
    var win = Utils.activeWindow;
    if(win) {
      var navbar = win.gBrowser.ownerDocument.getElementById("navigator-toolbox");
      return navbar;      
    }

    return null;
  },
  
  get urlBar(){
    var win = Utils.activeWindow;
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
  hide: function(animate) {
    var self = this;
    this._hidden = true;
    
    if( animate == false ) speed = 0;
    else speed = 150;
    
    $(self.el).animate({"marginTop":-self.height}, speed, function(){
      self.el.collapsed = true;
    });
  },

  // ----------
  show: function(animate) {
    this._hidden = false;
    this.el.collapsed = false;
    
    if(animate == false) {
      $(this.el).css({"marginTop":0});
    } else {
      var speed = 150;
      $(this.el).animate({"marginTop":0}, speed);
    }
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
  get isHidden(){ return this._hidden; }
}

// ##########
// Class: Page
// Singleton top-level UI manager. TODO: Integrate with <UIClass>.  
window.Page = {
  startX: 30, 
  startY: 70,
  
  show: function(){
    Utils.homeTab.focus();
    UI.tabBar.hide(false);
  },
  
  setupKeyHandlers: function(){
    var self = this;
    $(window).keydown(function(e){
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
      if( e.originalTarget.nodeName == "HTML" )
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
      if( focusTab.contentWindow == window){
        var currentTab = UI.currentTab;
        if(currentTab != null && currentTab.mirror != null) {
          // If there was a previous currentTab we want to animate
          // its mirror for the zoom out.
          // Zoom out!
          
          var mirror = currentTab.mirror;
          var $tab = $(mirror.el);
          var item = $tab.data().tabItem;
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
    
            if(window.UI)
              UI.tabBar.hide(false);
            
            window.Groups.setActiveGroup(null);
            TabMirror.resumePainting();
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
    var phantom = $("<div class='group'>").css({
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
      $("html").unbind("mousemove");
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
    
    $("html").mousemove(updateSize)
    $("html").one('mouseup',finalize);
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
function ArrangeClass(name, func){ this.init(name, func); };
ArrangeClass.prototype = {
  init: function(name, func){
    this.$el = this._create(name);
    this.arrange = func;
    if(func) this.$el.click(func);
  },
  
  _create: function(name){
    return $("<a class='action' href='#'/>").text(name).css({margin:5}).appendTo("#actions");
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
    
    // ___ URL Params
    var params = document.location.search.replace('?', '').split('&');
    $.each(params, function(index, param) {
      var parts = param.split('=');
      if(parts[0] == 'dev' && parts[1] == '1') 
        self.devMode = true;
    });
    
    // ___ Dev Mode
    if(this.devMode) {
      Switch.insert('body', '');
      $('<br><br>').appendTo("#actions");
      this._addArrangements();
    }
    
    // ___ Navbar
    if(this.focused) {
      this.tabBar.hide();
      this.navBar.hide();
    }
    
    Tabs.onFocus(function() {
      var me = this;
      setTimeout(function() { // Marshal event from chrome thread to DOM thread
        try{
          if(me.contentWindow.location.host == "tabcandy") {
            self.focused = true;
            self.navBar.hide();
            self.tabBar.hide();
          } else {
            self.focused = false;
            self.navBar.show();      
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
  
    // ___ Page
    Page.init();
    
    // ___ Storage
    var data = Storage.read();
    var sane = this.storageSanity(data);
    if(!sane || data.dataVersion < 2) {
      data.groups = null;
      data.tabs = null;
      data.pageBounds = null;
      
      if(!sane)
        alert('storage data is bad; starting fresh');
    }
     
    Groups.reconstitute(data.groups);
    TabItems.reconstitute(data.tabs);
    
    $(window).bind('beforeunload', function() {
      if(self.initialized) 
        self.save();
        
      self.navBar.show();
      self.tabBar.show(false);    
      self.tabBar.showAllTabs();
    });
    
    // ___ resizing
    if(data.pageBounds) {
      this.pageBounds = data.pageBounds;
      this.resize();
    } else 
      this.pageBounds = Items.getPageBounds();    
    
    $(window).resize(function() {
      self.resize();
    });
    
    // ___ Dev Menu
    this.addDevMenu();
    
    // ___ Done
    this.initialized = true;
  }, 
  
  // ----------
  resize: function() {
/*     Groups.repositionNewTabGroup(); */
    
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
  },
  
  // ----------
  addDevMenu: function() {
    var self = this;
    
    var html = '<select style="position:absolute; bottom:5px; right:5px; opacity:.2;">'; 
    var $select = $(html)
      .appendTo('body')
      .change(function () {
        var index = $(this).val();
        commands[index].code();
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
      name: 'save', 
      code: function() {
        self.save();
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
  save: function() {  
    var data = {
      dataVersion: 2,
      groups: Groups.getStorageData(),
      tabs: TabItems.getStorageData(), 
      pageBounds: Items.getPageBounds()
    };
    
    if(this.storageSanity(data))
      Storage.write(data);
    else
      alert('storage data is bad; reverting to previous version');
  },

  // ----------
  storageSanity: function(data) {
    if($.isEmptyObject(data))
      return true;
      
    var sane = true;
    sane = sane && typeof(data.dataVersion) == 'number';
    sane = sane && isRect(data.pageBounds);
    
    if(data.tabs)
      sane = sane && TabItems.storageSanity(data.tabs);
      
    if(data.groups)
      sane = sane && Groups.storageSanity(data.groups);
    
    return sane;
  },
  
  // ----------
  _addArrangements: function() {
    this.grid = new ArrangeClass("Grid", function(value) {
      if(typeof(Groups) != 'undefined')
        Groups.removeAll();
    
      var immediately = false;
      if(typeof(value) == 'boolean')
        immediately = value;
    
      var box = new Rect(Page.startX, Page.startY, TabItems.tabWidth, TabItems.tabHeight); 
      $(".tab:visible").each(function(i){
        var item = Items.item(this);
        item.setBounds(box, immediately);
        
        box.left += box.width + 30;
        if( box.left > window.innerWidth - (box.width + Page.startX)){ // includes allowance for the box shadow
          box.left = Page.startX;
          box.top += box.height + 30;
        }
      });
    });
        
    this.site = new ArrangeClass("Site", function() {
      Groups.removeAll();
      
      var groups = [];
      $(".tab:visible").each(function(i) {
        $el = $(this);
        var tab = Tabs.tab(this);
        
        var url = tab.url; 
        var domain = url.split('/')[2]; 
        var domainParts = domain.split('.');
        var mainDomain = domainParts[domainParts.length - 2];
        if(groups[mainDomain]) 
          groups[mainDomain].push($(this));
        else 
          groups[mainDomain] = [$(this)];
      });
      
      var createOptions = {dontPush: true, dontArrange: true};
      
      var leftovers = [];
      for(key in groups) {
        var set = groups[key];
        if(set.length > 1) {
          group = new Group(set, createOptions);
        } else
          leftovers.push(set[0]);
      }
      
    /*   if(leftovers.length > 1) { */
        group = new Group(leftovers, createOptions);
    /*   } */
      
      Groups.arrange();
    });
  },
  
  // ----------
  newTab: function(url, inBackground) {
    Tabs.open(url, inBackground);
  }
};

// ----------
window.UI = new UIClass();

})();

