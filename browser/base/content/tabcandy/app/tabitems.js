// Title: tabitems.js (revision-a)

// ##########
// Class: TabItem
// An <Item> that represents a tab. 
window.TabItem = function(container, tab) {
  Utils.assert('container', container);
  Utils.assert('tab', tab);
  Utils.assert('tab.mirror', tab.mirror);
  
  this.defaultSize = new Point(TabItems.tabWidth, TabItems.tabHeight);
  this.locked = {};
  this.isATabItem = true;
  this._zoomPrep = false;
  this.sizeExtra = new Point();

  // ___ setup div
  var $div = iQ(container);
  var self = this;
  
  $div.data('tabItem', this);    
  $div.data('isDragging', false);
  $div.draggable(window.Groups.dragOptions);
  $div.droppable(window.Groups.dropOptions);
  
  $div.mousedown(function(e) {
    if(!Utils.isRightClick(e))
      self.lastMouseDownTarget = e.target;
  });
    
  $div.mouseup(function(e) {
    var same = (e.target == self.lastMouseDownTarget);
    self.lastMouseDownTarget = null;
    if(!same)
      return;
    
    if(iQ(e.target).hasClass("close")) 
      tab.close();
    else {
      if(!iQ(this).data('isDragging')) 
        self.zoomIn();
/*
      else 
        tab.raw.pos = iQ(this).position(); // TODO: is this necessary?
*/
    }
  });
  
  iQ("<div>")
    .addClass('close')
    .appendTo($div); 
    
  iQ("<div>")
    .addClass('expander')
    .appendTo($div);

  // ___ additional setup
  this._init(container);

  this.reconnected = false;
  this._hasBeenDrawn = false;
  this.tab = tab;
  this.setResizable(true);

  TabItems.register(this);
  var self = this;
  this.tab.mirror.addOnClose(this, function(who, info) {
    TabItems.unregister(self);
  });   
     
  this.tab.mirror.addSubscriber(this, 'urlChanged', function(who, info) {
    if(!self.reconnected && (info.oldURL == 'about:blank' || !info.oldURL)) 
      TabItems.reconnect(self);

    self.save();
  });
};

window.TabItem.prototype = iQ.extend(new Item(), {
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
  save: function() {
    try{
      if(!this.tab || !this.tab.raw || !this.reconnected) // too soon/late to save
        return;

      var data = this.getStorageData();
      if(TabItems.storageSanity(data))
        Storage.saveTab(this.tab.raw, data);
    }catch(e){
      Utils.log("Error in saving tab value: "+e);
    }
  },
  
  // ----------  
  getURL: function() {
    return this.tab.url;
  },
  
  // ----------  
  _getSizeExtra: function() {
    var $container = iQ(this.container);

    this.sizeExtra.x = parseInt($container.css('padding-left')) 
        + parseInt($container.css('padding-right'));

    this.sizeExtra.y = parseInt($container.css('padding-top')) 
        + parseInt($container.css('padding-bottom'));

    return new Point(this.sizeExtra);
  },
  
  // ----------  
  reloadBounds: function() {
    var newBounds = Utils.getBounds(this.container);

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

    this.save();
  },
  
  // ----------  
  setBounds: function(rect, immediately, options) {
    if(!isRect(rect)) {
      Utils.trace('TabItem.setBounds: rect is not a real rectangle!', rect);
      return;
    }
    
    if(!options)
      options = {};
    
    if(this._zoomPrep)
      this.bounds.copy(rect);
    else {
      var $container = iQ(this.container);
      var $title = iQ('.tab-title', $container);
      var $thumb = iQ('.thumb', $container);
      var $close = iQ('.close', $container);
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
        
      if(iQ.isEmptyObject(css) && !options.force)
        return;
        
      this.bounds.copy(rect);
      
      // If this is a brand new tab don't animate it in from
      // a random location (i.e., from [0,0]). Instead, just
      // have it appear where it should be.
      if(immediately || (!this._hasBeenDrawn) ) {
  /*       $container.stop(true, true); */
        $container.css(css);
      } else {
        TabMirror.pausePainting();
        $container.animate(css, {
          duration: 200,
          easing: 'tabcandyBounce',
          complete: function() {
            TabMirror.resumePainting();
          }
        });
    /*       }).dequeue(); */
      }
  
      if(css.fontSize && !this.inStack()) {
        if(css.fontSize < minFontSize )
          $title.fadeOut();//.dequeue();
        else
          $title.fadeIn();//.dequeue();
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

      this._hasBeenDrawn = true;
    }

    this._updateDebugBounds();
    
    if(!isRect(this.bounds))
      Utils.trace('TabItem.setBounds: this.bounds is not a real rectangle!', this.bounds);

    this.save();
  },

  // ----------
  inStack: function(){
    return iQ(this.container).hasClass("stacked");
  },

  // ----------
  setZ: function(value) {
    iQ(this.container).css({zIndex: value});
  },
    
  // ----------
  close: function() {
    this.tab.close();

    // No need to explicitly delete the tab data, becasue sessionstore data
    // associated with the tab will automatically go away
  },
  
  // ----------
  addClass: function(className) {
    iQ(this.container).addClass(className);
  },
  
  // ----------
  removeClass: function(className) {
    iQ(this.container).removeClass(className);
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
    
    var $resizer = iQ('.expander', this.container);
    if(value) {
      $resizer.fadeIn();
      iQ(this.container).resizable({
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
      iQ(this.container).resizable('destroy');
    }
  },
  
  // ----------  
  makeActive: function(){
   iQ(this.container).find("canvas").addClass("focus")
  },

  // ----------    
  makeDeactive: function(){
   iQ(this.container).find("canvas").removeClass("focus")
  },
  
  // ----------
  // Function: zoomIn
  // Allows you to select the tab and zoom in on it, thereby bringing you
  // to the tab in Firefox to interact with.
  zoomIn: function() {
    var self = this;
    var $tabEl = iQ(this.container);
    var childHitResult = { shouldZoom: true };
    if(this.parent)
      childHitResult = this.parent.childHit(this);
      
    if(childHitResult.shouldZoom) {
      // Zoom in! 
      var orig = {
        width: $tabEl.width(),
        height:  $tabEl.height(),
        pos: $tabEl.position()
      }

      var scale = window.innerWidth/orig.width;
      
      var tab = this.tab;
      var mirror = tab.mirror;
      
      function onZoomDone(){
        UI.tabBar.show(false);              
        TabMirror.resumePainting();
        tab.focus();
        $tabEl
          .css({
            top:   orig.pos.top,
            left:  orig.pos.left,
            width: orig.width,
            height:orig.height,
          })
          .removeClass("front");  
        Navbar.show();
               
        // If the tab is in a group set then set the active
        // group to the tab's parent. 
        if( self.parent ){
          var gID = self.parent.id;
          var group = Groups.group(gID);
          Groups.setActiveGroup( group );
          group.setActiveTab( self );                 
        }
        else
          Groups.setActiveGroup( null );
      
        if(childHitResult.callback)
          childHitResult.callback();             
      }
      
      // The scaleCheat is a clever way to speed up the zoom-in code.
      // Because image scaling is slowest on big images, we cheat and stop the image
      // at scaled-down size and placed accordingly. Because the animation is fast, you can't
      // see the difference but it feels a lot zippier. The only trick is choosing the
      // right animation function so that you don't see a change in percieved 
      // animation speed.
      var scaleCheat = 1.7;
      TabMirror.pausePainting();
      $tabEl
        .addClass("front")
        .animate({
          top:    orig.pos.top * (1-1/scaleCheat),
          left:   orig.pos.left * (1-1/scaleCheat),
          width:  orig.width*scale/scaleCheat,
          height: orig.height*scale/scaleCheat
        }, {
          duration: 230,
          easing: 'fast',
          complete: onZoomDone
        });
    }    
  },
  
  // ----------
  // Function: zoomOut
  // Handles the zoom down animation after returning to TabCandy.
  // It is expected that this routine will be called from the chrome thread
  // (in response to Tabs.onFocus()).
  // 
  // Parameters: 
  //   complete - a function to call after the zoom down animation
  zoomOut: function(complete) {
    var $tab = iQ(this.container);
          
    var box = this.getBounds();
    box.width -= this.sizeExtra.x;
    box.height -= this.sizeExtra.y;
      
    TabMirror.pausePainting();

    var self = this;
    $tab.animate({
      left: box.left,
      top: box.top, 
      width: box.width,
      height: box.height
    }, {
      duration: 300,
      easing: 'fast',
      complete: function() { // note that this will happen on the DOM thread
        $tab.removeClass('front');
        
        TabMirror.resumePainting();   
        
        self._zoomPrep = false;
        self.setBounds(self.getBounds(), true, {force: true});    
        
        if(iQ.isFunction(complete)) 
           complete();
      }
    });
  },
  
  // ----------
  // Function: setZoomPrep
  // Either go into or return from (depending on <value>) "zoom prep" mode, 
  // where the tab fills a large portion of the screen in anticipation of 
  // the zoom out animation.
  setZoomPrep: function(value) {
    var $div = iQ(this.container);
    var data;
    
    if(value) { 
      this._zoomPrep = true;
      var box = this.getBounds();

      // The divide by two part here is a clever way to speed up the zoom-out code.
      // Because image scaling is slowest on big images, we cheat and start the image
      // at half-size and placed accordingly. Because the animation is fast, you can't
      // see the difference but it feels a lot zippier. The only trick is choosing the
      // right animation function so that you don't see a change in percieved 
      // animation speed from frame #1 (the tab) to frame #2 (the half-size image) to 
      // frame #3 (the first frame of real animation). Choosing an animation that starts
      // fast is key.
      var scaleCheat = 2;
      $div
        .addClass('front')
        .css({
          left: box.left * (1-1/scaleCheat),
          top: box.top * (1-1/scaleCheat), 
          width: window.innerWidth/scaleCheat,
          height: box.height * (window.innerWidth / box.width)/scaleCheat
        });
    } else {
      this._zoomPrep = false;
      $div.removeClass('front')
        
      var box = this.getBounds();
      this.reloadBounds();
      this.setBounds(box, true);
    }                
  }
});

// ##########
// Class: TabItems
// Singleton for managing <TabItem>s
window.TabItems = {
  minTabWidth: 40, 
  tabWidth: 160,
  tabHeight: 120, 
  fontSize: 9,

  // ----------
  init: function() {
    this.items = [];
    
    var self = this;
    window.TabMirror.customize(function(mirror) {
      var $div = iQ(mirror.el);
      var tab = mirror.tab;

      if(tab == Utils.homeTab) 
        $div.hide();
      else {
        var item = new TabItem(mirror.el, tab);
        
        item.addOnClose(self, function() {
          Items.unsquish(null, item);
        });

        if(!self.reconnect(item))
          Groups.newTab(item);          
      }
    });
  },

  // ----------  
  register: function(item) {
    Utils.assert('only register once per item', iQ.inArray(item, this.items) == -1);
    this.items.push(item);
  },
  
  // ----------  
  unregister: function(item) {
    var index = iQ.inArray(item, this.items);
    if(index != -1)
      this.items.splice(index, 1);  
  },
    
  // ----------
  getItems: function() {
    return Utils.copy(this.items);
  },
    
  // ----------
  // Function: getItemByTabElement
  // Given the DOM element that contains the tab's representation on screen, 
  // returns the <TabItem> it belongs to.
  getItemByTabElement: function(tabElement) {
    return iQ(tabElement).data("tabItem");
  },
  
  // ----------
  saveAll: function() {
    var items = this.getItems();
    iQ.each(items, function(index, item) {
      item.save();
    });
  },
  
  // ----------
  reconstitute: function() {
    var items = this.getItems();
    var self = this;
    iQ.each(items, function(index, item) {
      if(!self.reconnect(item))
        Groups.newTab(item);
    });
  },
  
  // ----------
  storageSanity: function(data) {
    // TODO: check everything 
    var sane = true;
    if(!isRect(data.bounds)) {
      Utils.log('TabItems.storageSanity: bad bounds', data.bounds);
      sane = false;
    }
    
    return sane;
  },

  // ----------
  reconnect: function(item) {
    var found = false;

    try{
      Utils.assert('item', item);
      Utils.assert('item.tab', item.tab);
      
      if(item.reconnected) 
        return true;
        
      if(!item.tab.raw)
        return false;
        
      var tab = Storage.getTabData(item.tab.raw);
      if (tab && this.storageSanity(tab)) {
        if(item.parent)
          item.parent.remove(item);
          
        item.setBounds(tab.bounds, true);
        
        if(isPoint(tab.userSize))
          item.userSize = new Point(tab.userSize);
          
        if(tab.groupID) {
          var group = Groups.group(tab.groupID);
          if(group) {
            group.add(item);          
          
            if(item.tab == Utils.activeTab) 
              Groups.setActiveGroup(item.parent);
          }
        }  
        
        Groups.updateTabBarForActiveGroup();
        
        item.reconnected = true;
        found = true;
      } else
        item.reconnected = (item.tab.url != 'about:blank');
    
      item.save();
    }catch(e){
      Utils.log(e);
    }
        
    return found; 
  }
};
