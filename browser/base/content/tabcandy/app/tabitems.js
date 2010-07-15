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
 * The Original Code is tabitems.js.
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
// Title: tabitems.js

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
  this.keepProportional = true;

  // ___ set up div
  var $div = iQ(container);
  var self = this;
  
  $div.data('tabItem', this);
  this.isDragging = false;
  
  this.sizeExtra.x = parseInt($div.css('padding-left')) 
      + parseInt($div.css('padding-right'));

  this.sizeExtra.y = parseInt($div.css('padding-top')) 
      + parseInt($div.css('padding-bottom'));

  this.bounds = $div.bounds();
  this.bounds.width += this.sizeExtra.x;
  this.bounds.height += this.sizeExtra.y;

  // ___ superclass setup
  this._init(container);
  
  // ___ drag/drop
  // override dropOptions with custom tabitem methods
  // This is mostly to support the phantom groups.
  this.dropOptions.drop = function(e){
    var $target = iQ(this.container);  
    $target.removeClass("acceptsDrop");
    var phantom = $target.data("phantomGroup");
    
    var group = drag.info.item.parent;
    if ( group == null ){
      phantom.removeClass("phantom");
      phantom.removeClass("group-content");
      group = new Group([$target, drag.info.$el], {container:phantom});
    } else 
      group.add( drag.info.$el );      
  };
  
  this.dropOptions.over = function(e){
    var $target = iQ(this.container);

    var groupBounds = Groups.getBoundingBox( [drag.info.$el, $target] );
    groupBounds.inset( -20, -20 );

    iQ(".phantom").remove();
    var phantom = iQ("<div>")
      .addClass('group phantom group-content')
      .css({
        position: "absolute",
        zIndex: -99
      })
      .css(groupBounds)
      .appendTo("body")
      .hide()
      .fadeIn();
      
    $target.data("phantomGroup", phantom);      
  };
  
  this.dropOptions.out = function(e){      
    var phantom = iQ(this.container).data("phantomGroup");
    if (phantom) { 
      phantom.fadeOut(function(){
        iQ(this).remove();
      });
    }
  };
  
  this.draggable();
  this.droppable(true);
  
  // ___ more div setup
  $div.mousedown(function(e) {
    if (!Utils.isRightClick(e))
      self.lastMouseDownTarget = e.target;
  });
    
  $div.mouseup(function(e) {
    var same = (e.target == self.lastMouseDownTarget);
    self.lastMouseDownTarget = null;
    if (!same)
      return;
    
    if (iQ(e.target).hasClass("close")) 
      tab.close();
    else {
      if (!Items.item(this).isDragging) 
        self.zoomIn();
    }
  });
  
  iQ("<div>")
    .addClass('close')
    .appendTo($div); 
    
  iQ("<div>")
    .addClass('expander')
    .appendTo($div);

  // ___ additional setup
  this.reconnected = false;
  this._hasBeenDrawn = false;
  this.tab = tab;
  this.setResizable(true);

  this._updateDebugBounds();
  
  TabItems.register(this);
  this.tab.mirror.addSubscriber(this, "close", function(who, info) {
    TabItems.unregister(self);
    self.removeTrenches();
  });   
     
  this.tab.mirror.addSubscriber(this, 'urlChanged', function(who, info) {
    if (!self.reconnected && (info.oldURL == 'about:blank' || !info.oldURL)) 
      TabItems.reconnect(self);

    self.save();
  });
};

window.TabItem.prototype = iQ.extend(new Item(), {
  // ----------  
  getStorageData: function(getImageData) {
    return {
      bounds: this.getBounds(), 
      userSize: (isPoint(this.userSize) ? new Point(this.userSize) : null),
      url: this.tab.url,
      groupID: (this.parent ? this.parent.id : 0),
      imageData: (getImageData && this.tab.mirror.tabCanvas ?
                  this.tab.mirror.tabCanvas.toImageData() : null),
      title: (getImageData && this.tab.raw.label ? this.tab.raw.label : null)
    };
  },

  // ----------
  save: function(saveImageData) {
    try{
      if (!this.tab || !this.tab.raw || !this.reconnected) // too soon/late to save
        return;

      var data = this.getStorageData(saveImageData);
      if (TabItems.storageSanity(data))
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
  setBounds: function(rect, immediately, options) {
    if (!isRect(rect)) {
      Utils.trace('TabItem.setBounds: rect is not a real rectangle!', rect);
      return;
    }
    
    if (!options)
      options = {};

    if (this._zoomPrep)
      this.bounds.copy(rect);
    else {
      var $container = iQ(this.container);
      var $title = iQ('.tab-title', $container);
      var $thumb = iQ('.thumb', $container);
      var $close = iQ('.close', $container);
      var $fav   = iQ('.favicon', $container);
      var css = {};
      
      const minFontSize = 8;
      const maxFontSize = 15;
  
      if (rect.left != this.bounds.left || options.force)
        css.left = rect.left;
        
      if (rect.top != this.bounds.top || options.force)
        css.top = rect.top;
        
      if (rect.width != this.bounds.width || options.force) {
        css.width = rect.width - this.sizeExtra.x;
        var scale = css.width / TabItems.tabWidth;
        
        // The ease function ".5+.5*Math.tanh(2*x-2)" is a pretty
        // little graph. It goes from near 0 at x=0 to near 1 at x=2
        // smoothly and beautifully.
        css.fontSize = minFontSize + (maxFontSize-minFontSize)*(.5+.5*Math.tanh(2*scale-2))
      }
  
      if (rect.height != this.bounds.height || options.force) 
        css.height = rect.height - this.sizeExtra.y; 
        
      if (iQ.isEmptyObject(css))
        return;
        
      this.bounds.copy(rect);
      
      // If this is a brand new tab don't animate it in from
      // a random location (i.e., from [0,0]). Instead, just
      // have it appear where it should be.
      if (immediately || (!this._hasBeenDrawn) ) {
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
  
      if (css.fontSize && !this.inStack()) {
        if (css.fontSize < minFontSize )
          $title.fadeOut();//.dequeue();
        else
          $title.fadeIn();//.dequeue();
      }
  
              
      // Usage      
      // slide({60:0, 70:1}, 66);
      // @60- return 0; at @70+ return 1; @65 return 0.5
      function slider(bounds, val){
        var keys = [];
        for (var key in bounds){ keys.push(key); bounds[key] = parseFloat(bounds[key]); };
        keys.sort(function(a,b){return a-b});
        var min = keys[0], max = keys[1];
        
        function slide(value){
          if ( value >= max ) return bounds[max];
          if ( value <= min ) return bounds[min];
          var rise = bounds[max] - bounds[min];
          var run = max-min;
          var value = rise * (value-min)/run;
          if ( value >= bounds[max] ) return bounds[max];
          if ( value <= bounds[min] ) return bounds[min];
          return value;
        }
        
        if ( val == undefined )
          return slide;
        return slide(val);
      };

      if (css.width && !this.inStack()) {
        $fav.css({top:4,left:4});
        
        var opacity = slider({70:1, 60:0}, css.width);
        $close.show().css({opacity:opacity});
        if ( opacity <= .1 ) $close.hide()

        var pad = slider({70:6, 60:1}, css.width);
        $fav.css({
         "padding-left": pad + "px",
         "padding-right": pad + 2 + "px",
         "padding-top": pad + "px",
         "padding-bottom": pad + "px",
         "border-color": "rgba(0,0,0,"+ slider({70:.2, 60:.1}, css.width) +")",
        });
      } 
      
      if (css.width && this.inStack()){
        $fav.css({top:0, left:0});
        var opacity = slider({90:1, 70:0}, css.width);
        
        var pad = slider({90:6, 70:1}, css.width);
        $fav.css({
         "padding-left": pad + "px",
         "padding-right": pad + 2 + "px",
         "padding-top": pad + "px",
         "padding-bottom": pad + "px",
         "border-color": "rgba(0,0,0,"+ slider({90:.2, 70:.1}, css.width) +")",
        });
      }   

      this._hasBeenDrawn = true;
    }

    this._updateDebugBounds();
    rect = this.getBounds(); // ensure that it's a <Rect>
    
    if (!isRect(this.bounds))
      Utils.trace('TabItem.setBounds: this.bounds is not a real rectangle!', this.bounds);
    
    if (!this.parent && !this.tab.closed)
      this.setTrenches(rect);

    this.save();
  },

  // ----------
  inStack: function(){
    return iQ(this.container).hasClass("stacked");
  },

  // ----------
  setZ: function(value) {
    this.zIndex = value;
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
  // Function: addOnClose
  // Accepts a callback that will be called when this item closes. 
  // The referenceObject is used to facilitate removal if necessary. 
  addOnClose: function(referenceObject, callback) {
    this.tab.mirror.addSubscriber(referenceObject, "close", callback);      
  },

  // ----------
  // Function: removeOnClose
  // Removes the close event callback associated with referenceObject.
  removeOnClose: function(referenceObject) {
    this.tab.mirror.removeSubscriber(referenceObject, "close");      
  },
  
  // ----------  
  setResizable: function(value){
    var $resizer = iQ('.expander', this.container);

    this.resizeOptions.minWidth = TabItems.minTabWidth;
    this.resizeOptions.minHeight = TabItems.minTabWidth * (TabItems.tabHeight / TabItems.tabWidth);

    if (value) {
      $resizer.fadeIn();
      this.resizable(true);
    } else {
      $resizer.fadeOut();
      this.resizable(false);
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
    if (this.parent)
      childHitResult = this.parent.childHit(this);
      
    if (childHitResult.shouldZoom) {
      // Zoom in! 
      var orig = {
        width: $tabEl.width(),
        height:  $tabEl.height(),
        pos: $tabEl.position()
      };

      var scale = window.innerWidth/orig.width;
      
      var tab = this.tab;

      function onZoomDone(){
        TabMirror.resumePainting();
        // If it's not focused, the onFocus lsitener would handle it.
        if (tab.isFocused()) {
          Page.tabOnFocus(tab);
        } else {
          tab.focus();
        }

        $tabEl
          .css({
            top:   orig.pos.top,
            left:  orig.pos.left,
            width: orig.width,
            height:orig.height,
          })
          .removeClass("front");

        // If the tab is in a group set then set the active
        // group to the tab's parent. 
        if ( self.parent ){
          var gID = self.parent.id;
          var group = Groups.group(gID);
          Groups.setActiveGroup( group );
          group.setActiveTab( self );                 
        }
        else
          Groups.setActiveGroup( null );
      
        if (childHitResult.callback)
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
      easing: 'cubic-bezier', // note that this is legal easing, even without parameters
      complete: function() { // note that this will happen on the DOM thread
        $tab.removeClass('front');
        
        TabMirror.resumePainting();   
        
        self._zoomPrep = false;
        self.setBounds(self.getBounds(), true, {force: true});    
        
        if (iQ.isFunction(complete)) 
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
    
    var box = this.getBounds();
    if (value) { 
      this._zoomPrep = true;

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
      $div.removeClass('front');
        
      this.setBounds(box, true, {force: true});
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
      var item = new TabItem(mirror.el, tab);
        
      item.addOnClose(self, function() {
        Items.unsquish(null, item);
      });

      if (!self.reconnect(item))
        Groups.newTab(item);          
    });
  },

  // ----------  
  register: function(item) {
    Utils.assert('only register once per item', this.items.indexOf(item) == -1);
    this.items.push(item);
  },
  
  // ----------  
  unregister: function(item) {
    var index = this.items.indexOf(item);
    if (index != -1)
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
  saveAll: function(saveImageData) {
    var items = this.getItems();
    items.forEach(function(item) {
      item.save(saveImageData);
    });
  },
  
  // ----------
  storageSanity: function(data) {
    // TODO: check everything 
    var sane = true;
    if (!isRect(data.bounds)) {
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
      
      if (item.reconnected) 
        return true;
        
      if (!item.tab.raw)
        return false;
        
      var tab = Storage.getTabData(item.tab.raw);       
      if (tab && this.storageSanity(tab)) {
        if (item.parent)
          item.parent.remove(item);
          
        item.setBounds(tab.bounds, true);
        
        if (isPoint(tab.userSize))
          item.userSize = new Point(tab.userSize);
          
        if (tab.groupID) {
          var group = Groups.group(tab.groupID);
          if (group) {
            group.add(item);          
          
            if (item.tab == Utils.activeTab) 
              Groups.setActiveGroup(item.parent);
          }
        }
        
        if (tab.imageData) {
          var mirror = item.tab.mirror;
          var rawTab = item.tab.raw;
          var $nameElement = iQ(mirror.nameEl);
          var $canvasElement = iQ(mirror.canvasEl);
          var $canvasPlaceholderElement = iQ(mirror.canvasPlaceholderEl);
          var hidePlaceholder = function() {
            $canvasPlaceholderElement.hide();
            $canvasElement.show();
          };
          var hidPlaceholder = false;
          var webProgress = {           
            onStateChange: function(aWebProgress, aRequest, aFlag, aStatus) {
              if (aFlag &
                  Components.interfaces.nsIWebProgressListener.STATE_STOP) {
                if (hidPlaceholder) {
                  rawtab.linkedBrowser.removeProgressListener(webProgress);
                } else if (aFlag &
                  Components.interfaces.nsIWebProgressListener.STATE_IS_WINDOW) {
                  iQ.timeout(function() {
                    if (!hidPlaceholder) {
                      hidPlaceholder = true;
                      hidePlaceholder();
                    }
                  }, 100);

                  rawTab.linkedBrowser.removeProgressListener(webProgress);
                }
              }
            },
          
            onLocationChange: function(aProgress, aRequest, aURI) { },
            onProgressChange: function(
              aWebProgress, aRequest, curSelf, maxSelf, curTot, maxTot) { },
            onStatusChange: function(
              aWebProgress, aRequest, aStatus, aMessage) { },
            onSecurityChange: function(aWebProgress, aRequest, aState) { },

            QueryInterface: function(aIID) {
             if (aIID.equals(Components.interfaces.nsIWebProgressListener) ||
                 aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
                 aIID.equals(Components.interfaces.nsISupports))
               return this;
             throw Components.results.NS_NOINTERFACE;
            }
          };
          
          // show the placeholders
          $canvasPlaceholderElement.attr("src", tab.imageData).show();
          $canvasElement.hide();
          $nameElement.text(tab.title ? tab.title : "");
          
          // add progress listener
          rawTab.linkedBrowser.addProgressListener(
            webProgress, Components.interfaces.nsIWebProgress.NOTIFY_ALL);
          // the code in the progress listener doesn't fire sometimes because
          // tab is being restored so need to catch that.
          iQ.timeout(function() {
            if (!hidPlaceholder) {
              hidPlaceholder = true;
              hidePlaceholder();
            }
          }, 30000);
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
