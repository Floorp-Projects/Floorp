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
var Tabbar = {
  // ----------
  // Variable: _hidden
  // We keep track of whether the tabs are hidden in this (internal) variable
  // so we still have access to that information during the window's unload event,
  // when window.Tabs no longer exists.
  _hidden: false, 
  get el(){ return window.Tabs[0].raw.parentNode; },
  height: window.Tabs[0].raw.parentNode.getBoundingClientRect().height,
  hide: function() {
    this._hidden = true;
    var self = this;
    $(self.el).animate({"marginTop":-self.height}, 150, function(){
      self.el.collapsed = true;
    });
  },
  show: function() {
    this._hidden = false;
    this.el.collapsed = false;
    $(this.el).animate({"marginTop":0}, 150);
  },
  get isHidden(){ return this._hidden; }
}

// ##########
window.Page = {
  startX: 30, 
  startY: 70,
    
  // ----------  
  init: function() {    
    Utils.homeTab.raw.maxWidth = 60;
    Utils.homeTab.raw.minWidth = 60;
    
    Tabs.onClose(function(){
      Utils.homeTab.focus();
      Toolbar.unread = 0;    
      return false;
    });
    
    Tabs.onOpen(function(){
      setTimeout(function(){
        Toolbar.unread += 1;
      },100);
    });
    
    var lastTab = null;
    Tabs.onFocus(function(){
      // If we switched to TabCandy window...
      if( this.contentWindow == window && lastTab != null && lastTab.mirror != null){
        Toolbar.unread = 0;
        // If there was a lastTab we want to animate
        // its mirror for the zoom out.
        var $tab = $(lastTab.mirror.el);
        
        var [w,h, pos, z] = [$tab.width(), $tab.height(), $tab.position(), $tab.css("zIndex")];
        var scale = window.innerWidth / w;

        var overflow = $("body").css("overflow");
        $("body").css("overflow", "hidden");
        
        var mirror = lastTab.mirror;
        TabMirror.pausePainting();
        $tab.css({
            top: 0, left: 0,
            width: window.innerWidth,
            height: h * (window.innerWidth/w),
            zIndex: 999999,
        }).animate({
            top: pos.top, left: pos.left,
            width: w, height: h
        },250, '', function() {
          $tab.css("zIndex",z);
          $("body").css("overflow", overflow);
          TabMirror.resumePainting();
        });
      }
      lastTab = this;
    });
  },
  
  // ----------  
  findOpenSpaceFor: function($div) {
    var w = window.innerWidth;
    var h = 0;
    var bufferX = 30;
    var bufferY = 30;
    var rects = [];
    var r;
    var $el;
    $(".tab:visible").each(function(i) {
      if(this == $div.get(0))
        return;
        
      $el = $(this);
      r = {x: parseInt($el.css('left')),
        y: parseInt($el.css('top')),
        w: parseInt($el.css('width')) + bufferX,
        h: parseInt($el.css('height')) + bufferY};
        
      if(r.x + r.w > w)
        w = r.x + r.w;

      if(r.y + r.h > h)
        h = r.y + r.h;
  
      rects.push(r);
    });

    if(!h) 
      return { 'x': this.startX, 'y': this.startY };
      
    var canvas = document.createElement('canvas');
    $(canvas).attr({width:w,height:h});

    var ctx = canvas.getContext('2d');
    ctx.fillStyle = 'rgb(255, 255, 255)';
    ctx.fillRect(0, 0, w, h);
    ctx.fillStyle = 'rgb(0, 0, 0)';
    var count = rects.length;
    var a;
    for(a = 0; a < count; a++) {
      r = rects[a];
      ctx.fillRect(r.x, r.y, r.w, r.h);
    }
    
    var divWidth = parseInt($div.css('width')) + bufferX;
    var divHeight = parseInt($div.css('height')) + bufferY;
    var strideX = divWidth / 4;
    var strideY = divHeight / 4;
    var data = ctx.getImageData(0, 0, w, h).data;

    function isEmpty(x1, y1) {
      return (x1 >= 0 && y1 >= 0
          && x1 < w && y1 < h
          && data[(y1 * w + x1) * 4]);
    }

    function isEmptyBox(x1, y1) {
      return (isEmpty(x1, y1) 
          && isEmpty(x1 + (divWidth - 1), y1)
          && isEmpty(x1, y1 + (divHeight - 1))
          && isEmpty(x1 + (divWidth - 1), y1 + (divHeight - 1)));
    }
    
    for(var y = this.startY; y < h; y += strideY) {
      for(var x = this.startX; x < w; x += strideX) {
        if(isEmptyBox(x, y)) {
          for(; y > this.startY + 1; y--) {
            if(!isEmptyBox(x, y - 1))
              break;
          }

          for(; x > this.startX + 1; x--) {
            if(!isEmptyBox(x - 1, y))
              break;
          }

          return { 'x': x, 'y': y };
        }
  	  }
    }

    return { 'x': this.startX, 'y': h };        
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
function UIClass(){ 
  this.navBar = Navbar;
  this.tabBar = Tabbar;
  this.devMode = false;
  
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
  this.navBar.hide();
  
  Tabs.onFocus(function() {
    if(this.contentWindow.location.host == "tabcandy")
      self.navBar.hide();
    else
      self.navBar.show();
  });

  Tabs.onOpen(function(a, b) {
    self.navBar.show();
  });

  // ___ tab bar
  this.$tabBarToggle = $("#tabbar-toggle")
    .click(function() {
      if(self.tabBar.isHidden)
        self.showTabBar();
      else
        self.hideTabBar();
    });

  // ___ Page
  Page.init();
  
  // ___ Storage
  var data = Storage.read();
  if(data.hideTabBar)
    this.hideTabBar();
    
  if(data.dataVersion < 2) {
    data.groups = null;
    data.tabs = null;
  }
      
  Groups.reconstitute(data.groups);
  TabItems.reconstitute(data.tabs);
  
  $(window).bind('beforeunload', function() {
    if(self.initialized) {
      var data = {
        dataVersion: 2,
        hideTabBar: self.tabBar._hidden,
        groups: Groups.getStorageData(),
        tabs: TabItems.getStorageData(), 
        pageBounds: Items.getPageBounds()
      };
      
      Storage.write(data);
    }
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
  
  // ___ Done
  this.initialized = true;
};

// ----------
UIClass.prototype = {
  // ----------
  showTabBar: function() {
    this.tabBar.show();
    this.$tabBarToggle.removeClass("tabbar-off");        
  },

  // ----------
  hideTabBar: function() {
    this.tabBar.hide();
    this.$tabBarToggle.addClass("tabbar-off");        
  },

  // ----------
  resize: function() {
    Groups.repositionNewTabGroup();
    
    var items = Items.getTopLevelItems();
    var itemBounds = new Rect(this.pageBounds);
    itemBounds.width = 1;
    itemBounds.height = 1;
    $.each(items, function(index, item) {
      if(item.locked)
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
    $.each(items, function(index, item) {
      if(item.locked)
        return;
        
      var bounds = item.getBounds();

      bounds.left += newPageBounds.left - self.pageBounds.left;
      bounds.left *= scale;
      bounds.width *= scale;

      bounds.top += newPageBounds.top - self.pageBounds.top;            
      bounds.top *= scale;
      bounds.height *= scale;
      
      item.setBounds(bounds, true);
    });
    
    this.pageBounds = Items.getPageBounds();
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

