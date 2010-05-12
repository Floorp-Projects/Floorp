// Title: tabitems.js (revision-a)

// ##########
window.TabItem = function(container, tab) {
  this.defaultSize = new Point(TabItems.tabWidth, TabItems.tabHeight);
  this.locked = {};

  this._init(container);

  this._hasBeenDrawn = false;
  this.tab = tab;
  this.setResizable(true);
};

window.TabItem.prototype = $.extend(new Item(), {
  // ----------  
  getStorageData: function() {
    return {
      bounds: this.getBounds(), 
      userSize: (isPoint(this.userSize) ? new Point(this.userSize) : null),
      url: this.tab.url,
      groupID: (this.parent ? this.parent.id : 0)
    };
  },
  
  // ----------  
  getURL: function() {
    return this.tab.url;
  },
  
  // ----------  
  _getSizeExtra: function() {
    var $container = $(this.container);

    var widthExtra = parseInt($container.css('padding-left')) 
        + parseInt($container.css('padding-right'));

    var heightExtra = parseInt($container.css('padding-top')) 
        + parseInt($container.css('padding-bottom'));

    return new Point(widthExtra, heightExtra);
  },
  
  // ----------  
  reloadBounds: function() {
    var newBounds = Utils.getBounds(this.container);
    var extra = this._getSizeExtra();
    newBounds.width += extra.x;
    newBounds.height += extra.y;

/*
    if(!this.bounds || newBounds.width != this.bounds.width || newBounds.height != this.bounds.height) {
      // if resizing, or first time, do the whole deal
      if(!this.bounds)
        this.bounds = new Rect(0, 0, 0, 0);
  
      this.setBounds(newBounds, true);      
    } else { 
*/
      // if we're just moving, this is more efficient
      this.bounds = newBounds;
      this._updateDebugBounds();
/*     } */
  },
  
  // ----------  
  setBounds: function(rect, immediately) {
    if(!isRect(rect)) {
      Utils.trace('TabItem.setBounds: rect is not a real rectangle!', rect);
      return;
    }

    var $container = $(this.container);
    var $title = $('.tab-title', $container);
    var $thumb = $('.thumb', $container);
    var $close = $('.close', $container);
    var extra = this._getSizeExtra();
    var css = {};
    
    const minFontSize = 8;
    const maxFontSize = 15;

    if(rect.left != this.bounds.left)
      css.left = rect.left;
      
    if(rect.top != this.bounds.top)
      css.top = rect.top;
      
    if(rect.width != this.bounds.width) {
      css.width = rect.width - extra.x;
      var scale = css.width / TabItems.tabWidth;
      
      // The ease function ".5+.5*Math.tanh(2*x-2)" is a pretty
      // little graph. It goes from near 0 at x=0 to near 1 at x=2
      // smoothly and beautifully.
      css.fontSize = minFontSize + (maxFontSize-minFontSize)*(.5+.5*Math.tanh(2*scale-2))
    }

    if(rect.height != this.bounds.height) {
      css.height = rect.height - extra.y; 
    }
      
    if($.isEmptyObject(css))
      return;
      
    this.bounds.copy(rect);
    
    // If this is a brand new tab don't animate it in from
    // a random location (i.e., from [0,0]). Instead, just
    // have it appear where it should be.
    if(immediately || (!this._hasBeenDrawn) ) {
      $container.stop(true, true);
      $container.css(css);
    } else {
      TabMirror.pausePainting();
      $container.animate(css,{
        complete: function() {TabMirror.resumePainting();},
        duration: 350,
        easing: "tabcandyBounce"
      }).dequeue();
    }

    if(css.fontSize && !$container.hasClass("stacked")) {
      if(css.fontSize < minFontSize )
        $title.fadeOut().dequeue();
      else
        $title.fadeIn().dequeue();
    }

    if(css.width) {
      if(css.width < 30) {
        $thumb.fadeOut();
        $close.fadeOut();
      } else {
        $thumb.fadeIn();
        $close.fadeIn();
      }
    }    

    this._updateDebugBounds();
    this._hasBeenDrawn = true;
    
    if(!isRect(this.bounds))
      Utils.trace('TabItem.setBounds: this.bounds is not a real rectangle!', this.bounds);
  },

  // ----------
  setZ: function(value) {
    $(this.container).css({zIndex: value});
  },
    
  // ----------
  close: function() {
    this.tab.close();
  },
  
  // ----------
  addClass: function(className) {
    $(this.container).addClass(className);
  },
  
  // ----------
  removeClass: function(className) {
    $(this.container).removeClass(className);
  },
  
  // ----------
  addOnClose: function(referenceObject, callback) {
    this.tab.mirror.addOnClose(referenceObject, callback);      
  },

  // ----------
  removeOnClose: function(referenceObject) {
    this.tab.mirror.removeOnClose(referenceObject);      
  },
  
  // ----------  
  setResizable: function(value){
    var self = this;
    
    var $resizer = $('.expander', this.container);
    if(value) {
      $resizer.fadeIn();
      $(this.container).resizable({
        handles: "se",
        aspectRatio: true,
        minWidth: TabItems.minTabWidth,
        minHeight: TabItems.minTabWidth * (TabItems.tabHeight / TabItems.tabWidth),
        resize: function(){
          self.reloadBounds();
        },
        stop: function(){
          self.reloadBounds();
          self.setUserSize();        
          self.pushAway();
        } 
      });
    } else {
      $resizer.fadeOut();
      $(this.container).resizable('destroy');
    }
  }
});

// ##########
window.TabItems = {
  minTabWidth: 40, 
  tabWidth: 160,
  tabHeight: 120, 
  fontSize: 9,

  // ----------
  init: function() {
    var self = this;
        
    function mod($div){
      if(window.Groups) {        
        $div.data('isDragging', false);
        $div.draggable(window.Groups.dragOptions);
        $div.droppable(window.Groups.dropOptions);
      }
      
      $div.mousedown(function(e) {
        if(!Utils.isRightClick(e))
          self.lastMouseDownTarget = e.target;
      });
        
      $div.mouseup(function(e) { 
        var same = (e.target == self.lastMouseDownTarget);
        self.lastMouseDownTarget = null;
        if(!same)
          return;
        
        if(e.target.className == "close") {
          $(this).find("canvas").data("link").tab.close(); }
        else {
          if(!$(this).data('isDragging')) {        
            var item = $(this).data('tabItem');
            var childHitResult = { shouldZoom: true };
            if(item.parent)
              childHitResult = item.parent.childHit(item);
              
            if(childHitResult.shouldZoom) {
              // Zoom in! 
              var orig = {
                width: $(this).width(),
                height:  $(this).height(),
                pos: $(this).position()
              }
  
              var scale = window.innerWidth/orig.width;
              
              var tab = Tabs.tab(this);
              var mirror = tab.mirror;
              
              var overflow = $("body").css("overflow");
              $("body").css("overflow", "hidden");
              
              function onZoomDone(){
                UI.tabBar.show(false);              
                TabMirror.resumePainting();
                $(this).find("canvas").data("link").tab.focus();
                $(this).css({
                  top:   orig.pos.top,
                  left:  orig.pos.left,
                  width: orig.width,
                  height:orig.height,
                  })
                  .removeClass("front");  
                Navbar.show();
                       
                // If the tab is in a group set then set the active
                // group to the tab's parent. 
                if( self.getItemByTab(this).parent ){
                  var gID = self.getItemByTab(this).parent.id;
                  var group = Groups.group(gID);
                  Groups.setActiveGroup( group );                  
                }
                else
                  Groups.setActiveGroup( null );
              
                $("body").css("overflow", overflow); 
                
                if(childHitResult.callback)
                  childHitResult.callback();             
              }
    
              TabMirror.pausePainting();
              $(this)
                .addClass("front")
                .animate({
                  top:    -10,
                  left:   0,
                  width:  orig.width*scale,
                  height: orig.height*scale
                  }, 200, "easeInQuad", onZoomDone);
            }
          } else {
            $(this).find("canvas").data("link").tab.raw.pos = $(this).position();
          }
        }
      });
      
      $("<div class='close'></div>").appendTo($div);
      $("<div class='expander'></div>").appendTo($div);
  
      var reconnected = false;
      $div.each(function() {
        var tab = Tabs.tab(this);
        if(tab == Utils.homeTab) 
          $(this).hide();
        else {
          var item = new TabItem(this, tab);
          $(this).data('tabItem', item);    
          
          item.addOnClose(self, function() {
            Items.unsquish(null, item);
          });

          if(TabItems.reconnect(item))
            reconnected = true;
          else if(!tab.url || tab.url == 'about:blank') {
            tab.mirror.addSubscriber(item, 'urlChanged', function(who, info) {
              Utils.assert('changing away from blank', info.oldURL == 'about:blank' || !info.oldURL);
              TabItems.reconnect(item);
              tab.mirror.removeSubscriber(item);
            });
          }
        }
      });
      
      if(!reconnected && $div.length == 1 && Groups){
          Groups.newTab($div.data('tabItem'));          
      }
        
            
      // TODO: Figure out this really weird bug?
      // Why is that:
      //    $div.find("canvas").data("link").tab.url
      // returns chrome://tabcandy/content/candies/original/index.html for
      // every $div (which isn't right), but that
      //   $div.bind("test", function(){
      //      var url = $(this).find("canvas").data("link").tab.url;
      //   });
      //   $div.trigger("test")
      // returns the right result (i.e., the per-tab URL)?
      // I'm so confused...
      // Although I can use the trigger trick, I was thinking about
      // adding code in here which sorted the tabs into groups.
      // -- Aza
    }
    
    window.TabMirror.customize(mod);
  },

  // ----------
  getItems: function() {
    var items = [];
    $('.tab:visible').each(function() {
      items.push($(this).data('tabItem'));
    });
    
    return items;
  },
    
  // ----------
  getItemByTab: function(tab) {
    return $(tab).data("tabItem");
  },
  
  // ----------
  getStorageData: function() {
    var data = {tabs: []};
    var items = this.getItems();
    $.each(items, function(index, item) {
      data.tabs.push(item.getStorageData());
    });
    
    return data;
  },
  
  // ----------
  reconstitute: function(data) {
    this.storageData = data;
    var items = this.getItems();
    if(data && data.tabs) {
      var self = this;
      $.each(items, function(index, item) {
        if(!self.reconnect(item))
          Groups.newTab(item);
      });
    } else {
        var box = Items.getPageBounds();
        box.inset(20, 20);
        
        Items.arrange(items, box, {padding: 10, animate:false});
    }
  },
  
  // ----------
  storageSanity: function(data) {
    // TODO: check everything 
    if(!data.tabs)
      return false;
      
    var sane = true;
    $.each(data.tabs, function(index, tab) {
      if(!isRect(tab.bounds)) {
        Utils.log('TabItems.storageSanity: bad bounds', tab.bounds);
        sane = false;
      }
    });
    
    return sane;
  },

  // ----------
  reconnect: function(item) {
    if(item.reconnected)
      return true;
      
    var found = false;
    if(this.storageData && this.storageData.tabs) {
      var self = this;
      $.each(this.storageData.tabs, function(index, tab) {
        if(tab.url == 'about:blank')
          return;
          
        if(item.getURL() == tab.url) {
          if(item.parent)
            item.parent.remove(item);
            
          item.setBounds(tab.bounds, true);
          
          if(isPoint(tab.userSize))
            item.userSize = new Point(tab.userSize);
            
          if(tab.groupID) {
            var group = Groups.group(tab.groupID);
            group.add(item);
          }
          
          self.storageData.tabs.splice(index, 1);
          item.reconnected = true;
          found = true;
          return false;
        }      
      });
    }   
    
    return found; 
  }
};

TabItems.init();