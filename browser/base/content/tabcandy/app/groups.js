// Title: groups.js (revision-a)
(function(){

// ----------
// Function: numCmp
// Numeric compare for sorting.
// Private to this file.
var numCmp = function(a,b){ return a-b; }

// ----------
// Function: min
// Given a list of numbers, returns the smallest. 
// Private to this file.
function min(list){ return list.slice().sort(numCmp)[0]; }

// ----------
// Function: max
// Given a list of numbers, returns the largest. 
// Private to this file.
function max(list){ return list.slice().sort(numCmp).reverse()[0]; }

// ----------
// Function: dropAcceptFunction
// Given a DOM element, returns true if it should accept tabs being dropped on it.
// Private to this file.
function dropAcceptFunction(el) {
  var $el = iQ(el);
  if($el.hasClass('tab')) {
    var item = Items.item($el);
    if(item && (!item.parent || !item.parent.expanded)) {
      return true;
    }
  }           
          
  return false;
}

// ##########
// Class: Group
// A single group in the tab candy window. Descended from <Item>.
// Note that it implements the <Subscribable> interface.
// 
// ----------
// Constructor: Group
// 
// Parameters: 
//   listOfEls - an array of DOM elements for tabs to be added to this group
//   options - various options for this group (see below). In addition, gets passed 
//     to <add> along with the elements provided. 
// 
// Possible options: 
//   id - specifies the group's id; otherwise automatically generated
//   locked - see <Item.locked>; default is {}
//   userSize - see <Item.userSize>; default is null
//   bounds - a <Rect>; otherwise based on the locations of the provided elements
//   container - a DOM element to use as the container for this group; otherwise will create 
//   title - the title for the group; otherwise blank
//   dontPush - true if this group shouldn't push away on creation; default is false
window.Group = function(listOfEls, options) {
  try {
  if(typeof(options) == 'undefined')
    options = {};

  this._inited = false;
  this._children = []; // an array of Items
  this.defaultSize = new Point(TabItems.tabWidth * 1.5, TabItems.tabHeight * 1.5);
  this.isAGroup = true;
  this.id = options.id || Groups.getNextID();
  this._isStacked = false;
  this._stackAngles = [0];
  this.expanded = null;
  this.locked = (options.locked ? Utils.copy(options.locked) : {});
  this.topChild = null;
  this._activeTab = null;
  
  if(isPoint(options.userSize))  
    this.userSize = new Point(options.userSize);

  var self = this;

  var rectToBe;
  if(options.bounds)
    rectToBe = new Rect(options.bounds);
    
  if(!rectToBe) {
    var boundingBox = this._getBoundingBox(listOfEls);
    var padding = 30;
    rectToBe = new Rect(
      boundingBox.left-padding,
      boundingBox.top-padding,
      boundingBox.width+padding*2,
      boundingBox.height+padding*2
    );
  }

  var $container = options.container; 
  if(!$container) {
    $container = iQ('<div>')
      .addClass('group')
      .css({position: 'absolute'})
      .css(rectToBe);
    
    if( this.isNewTabsGroup() ) $container.addClass("newTabGroup");
  }
  
  $container
    .css({zIndex: -100})
    .data('isDragging', false)
    .appendTo("body");
/*     .dequeue(); */
        
  // ___ New Tab Button   
  this.$ntb = iQ("<div>")
    .appendTo($container);
    
  this.$ntb    
    .addClass(this.isNewTabsGroup() ? 'newTabButtonAlt' : 'newTabButton')
    .click(function(){
      self.newTab();
    });
  
  if( this.isNewTabsGroup() ) this.$ntb.html("<span>+</span>");
    
  // ___ Resizer
  this.$resizer = iQ("<div>")
    .addClass('resizer')
    .css({
      position: "absolute",
      width: 16, height: 16,
      bottom: 0, right: 0,
    })
    .appendTo($container)
    .hide();

  // ___ Titlebar
  var html =
    "<div class='title-container'>" +
      "<input class='name' value='" + (options.title || "") + "'/>" + 
      "<div class='title-shield' />" + 
    "</div>" + 
    "<div class='close' />";
       
  this.$titlebar = iQ('<div>')
    .addClass('titlebar')
    .html(html)        
    .appendTo($container);
    
  this.$titlebar.css({
      position: "absolute",
    });
    
  var $close = iQ('.close', this.$titlebar).click(function() {
    self.closeAll();
  });
  
  // ___ Title 
  this.$titleContainer = iQ('.title-container', this.$titlebar);
  this.$title = iQ('.name', this.$titlebar);
  this.$titleShield = iQ('.title-shield', this.$titlebar);
  
  var titleUnfocus = function() {
    self.$titleShield.show();
    if(!self.getTitle()) {
      self.$title
        .addClass("defaultName")
        .val(self.defaultName);
    } else {
      self.$title
        .css({"background":"none"})
        .animate({
          "paddingLeft": 1
        }, {
          duration: 350,
          easing: 'tabcandyBounce'
        });
    }
  };
  
  var handleKeyPress = function(e){
    if( e.which == 13 ) { // return
      self.$title.get(0).blur();
      self.$title
        .addClass("transparentBorder")
        .one("mouseout", function(){
          self.$title.removeClass("transparentBorder");
        });
    } else 
      self.adjustTitleSize();
      
    self.save();
  }
  
  this.$title
    .css({backgroundRepeat: 'no-repeat'})
    .blur(titleUnfocus)
    .focus(function() {
      if(self.locked.title) {
        self.$title.get(0).blur();
        return;
      }  
      self.$title.get(0).select();
      if(!self.getTitle()) {
        self.$title
          .removeClass("defaultName")
          .val('');
      }
    })
    .keyup(handleKeyPress);
  
  titleUnfocus();
  
  if(this.locked.title)
    this.$title.addClass('name-locked');
  else {
    this.$titleShield
      .mousedown(function(e) {
        self.lastMouseDownTarget = (Utils.isRightClick(e) ? null : e.target);
      })
      .mouseup(function(e) { 
        var same = (e.target == self.lastMouseDownTarget);
        self.lastMouseDownTarget = null;
        if(!same)
          return;
        
        if(!$container.data('isDragging')) {        
          self.$titleShield.hide();
          self.$title.get(0).focus();
        }
      });
  }
    
  // ___ Content
  // TODO: I don't think we need this any more
  this.$content = iQ('<div>')
    .addClass('group-content')
    .css({
      left: 0,
      top: this.$titlebar.height(),
      position: 'absolute'
    })
    .appendTo($container);
  
  // ___ locking
  if(this.locked.bounds)
    $container.css({cursor: 'default'});    
    
  if(this.locked.close)
    $close.hide();
    
  // ___ Superclass initialization
  this._init($container.get(0));

  if(this.$debug) 
    this.$debug.css({zIndex: -1000});
  
  // ___ Children
  iQ.each(listOfEls, function(index, el) {  
    self.add(el, null, options);
  });

  // ___ Finish Up
  this._addHandlers($container);
  
  if(!this.locked.bounds)
    this.setResizable(true);
  
  Groups.register(this);
  
  this.setBounds(rectToBe);
  
  // ___ Push other objects away
  if(!options.dontPush)
    this.pushAway();   

  this._inited = true;
  this.save();
  } catch(e){
    Utils.log("Error in Group(): " + e);
  }
};

// ----------
window.Group.prototype = iQ.extend(new Item(), new Subscribable(), {
  // ----------
  // Variable: defaultName
  // The prompt text for the title field.
  defaultName: "name this group...",

  // -----------
  // Function: setActiveTab
  // Sets the active tab (for keyboard selection, etc)
  // TODO: This currently accepts only the DOM element of a tab.
  // It should also take a TabItem... 
  setActiveTab: function(tab){
    this._activeTab = tab;
  },

  // -----------
  // Function: getActiveTab
  // Gets the active tab (for keyboard selection, etc)
  // TODO: This currently returns a DOM element of the selected tab.
  // It should probably actually be a TabItem...
  getActiveTab: function(tab){
    return this._activeTab;
  },
  
  // ----------  
  // Function: getStorageData
  // Returns all of the info worth storing about this group.
  getStorageData: function() {
    var data = {
      bounds: this.getBounds(), 
      userSize: null,
      locked: Utils.copy(this.locked), 
      title: this.getTitle(),
      id: this.id
    };
    
    if(isPoint(this.userSize))  
      data.userSize = new Point(this.userSize);
  
    return data;
  },

  // ----------
  // Function: save
  // Saves this group to persistant storage. 
  save: function() {
    if (!this._inited) // too soon to save now
      return;

    var data = this.getStorageData();
    if(Groups.groupStorageSanity(data))
      Storage.saveGroup(Utils.getCurrentWindow(), data);
  },
  
  // ----------
  // Function: isNewTabsGroup
  // Returns true if the callee is the "New Tabs" group.
  // TODO: more robust
  isNewTabsGroup: function() {
    return (this.locked.bounds && this.locked.title && this.locked.close);
  },
  
  // ----------
  getTitle: function() {
    var value = (this.$title ? this.$title.val() : '');
    return (value == this.defaultName ? '' : value);
  },

  // ----------  
  setTitle: function(value) {
    this.$title.val(value); 
    this.save();
  },

  // ----------  
  adjustTitleSize: function() {
    Utils.assert('bounds needs to have been set', this.bounds);
    var w = Math.min(this.bounds.width - 35, Math.max(150, this.getTitle().length * 6));
    var css = {width: w};
    this.$title.css(css);
    this.$titleShield.css(css);
  },
  
  // ----------  
  _getBoundingBox: function(els) {
    var el;
    var boundingBox = {
      top:    min( [iQ(el).position().top  for([,el] in Iterator(els))] ),
      left:   min( [iQ(el).position().left for([,el] in Iterator(els))] ),
      bottom: max( [iQ(el).position().top  for([,el] in Iterator(els))] )  + iQ(els[0]).height(),
      right:  max( [iQ(el).position().left for([,el] in Iterator(els))] ) + iQ(els[0]).width(),
    };
    boundingBox.height = boundingBox.bottom - boundingBox.top;
    boundingBox.width  = boundingBox.right - boundingBox.left;
    return boundingBox;
  },
  
  // ----------  
  getContentBounds: function() {
    var box = this.getBounds();
    var titleHeight = this.$titlebar.height();
    box.top += titleHeight;
    box.height -= titleHeight;
    box.inset(6, 6);
    
    if(this.isNewTabsGroup())
      box.height -= 12; // Hack for tab titles
    else
      box.height -= 33; // For new tab button
      
    return box;
  },
  
  // ----------  
  reloadBounds: function() {
    var bb = Utils.getBounds(this.container);
    
    if(!this.bounds)
      this.bounds = new Rect(0, 0, 0, 0);
    
    this.setBounds(bb, true);
  },
  
  // ----------  
  setBounds: function(rect, immediately) {
    if(!isRect(rect)) {
      Utils.trace('Group.setBounds: rect is not a real rectangle!', rect);
      return;
    }
    
    var titleHeight = this.$titlebar.height();
    
    // ___ Determine what has changed
    var css = {};
    var titlebarCSS = {};
    var contentCSS = {};
    var force = false;

    if(force || rect.left != this.bounds.left)
      css.left = rect.left;
      
    if(force || rect.top != this.bounds.top) 
      css.top = rect.top;
      
    if(force || rect.width != this.bounds.width) {
      css.width = rect.width;
      titlebarCSS.width = rect.width;
      contentCSS.width = rect.width;
    }

    if(force || rect.height != this.bounds.height) {
      css.height = rect.height; 
      contentCSS.height = rect.height - titleHeight; 
    }
      
    if(iQ.isEmptyObject(css))
      return;
      
    var offset = new Point(rect.left - this.bounds.left, rect.top - this.bounds.top);
    this.bounds = new Rect(rect);

    // ___ Deal with children
    if(css.width || css.height) {
      this.arrange({animate: !immediately}); //(immediately ? 'sometimes' : true)});
    } else if(css.left || css.top) {
      iQ.each(this._children, function(index, child) {
        var box = child.getBounds();
        child.setPosition(box.left + offset.x, box.top + offset.y, immediately);
      });
    }
          
    // ___ Update our representation
    if(immediately) {
/*
      $(this.container).stop(true, true);
      this.$titlebar.stop(true, true);
      this.$content.stop(true, true);
*/

      iQ(this.container).css(css);
      this.$titlebar.css(titlebarCSS);
      this.$content.css(contentCSS);
    } else {
      TabMirror.pausePainting();
      iQ(this.container).animate(css, {
        duration: 350, 
        easing: 'tabcandyBounce', 
        complete: function() {
          TabMirror.resumePainting();
        }
      });
  /*       }).dequeue(); */
      
      this.$titlebar.animate(titlebarCSS, {
        duration: 350
      });//.dequeue();        
      
      this.$content.animate(contentCSS, {
        duration: 350
      }); //.dequeue();        
    }
    
    this.adjustTitleSize();

    this._updateDebugBounds();

    if(!isRect(this.bounds))
      Utils.trace('Group.setBounds: this.bounds is not a real rectangle!', this.bounds);

    this.save();
  },
  
  // ----------
  setZ: function(value) {
    iQ(this.container).css({zIndex: value});

    if(this.$debug) 
      this.$debug.css({zIndex: value + 1});

    var count = this._children.length;
    if(count) {
      var topZIndex = value + count + 1;
      var zIndex = topZIndex;
      var self = this;
      iQ.each(this._children, function(index, child) {
        if(child == self.topChild)
          child.setZ(topZIndex + 1);
        else {
          child.setZ(zIndex);
          zIndex--;
        }
      });
    }
  },
    
  // ----------
  close: function() {
    this.removeAll();
    this._sendOnClose();
    Groups.unregister(this);
    iQ(this.container).fadeOut(function() {
      iQ(this).remove();
      Items.unsquish();
    });

    Storage.deleteGroup(Utils.getCurrentWindow(), this.id);
  },
  
  // ----------  
  closeAll: function() {
    if(this._children.length) {
      var toClose = iQ.merge([], this._children);
      iQ.each(toClose, function(index, child) {
        child.close();
      });
    } else if(!this.locked.close)
      this.close();
  },
    
  // ----------  
  // Function: add
  // Adds an item to the group.
  // Parameters: 
  // 
  //   a - The item to add. Can be an <Item>, a DOM element or a jQuery object.
  //       The latter two must refer to the container of an <Item>.
  //   dropPos - An object with left and top properties referring to the location dropped at.  Optional.
  //   options - An object with optional settings for this call. Currently the only one is dontArrange. 
  add: function(a, dropPos, options) {
    try {
      var item;
      var $el;
      if(a.isAnItem) {
        item = a;
        $el = iQ(a.container);
      } else {
        $el = iQ(a);
        item = Items.item($el);
      }    
      
      Utils.assert('shouldn\'t already be in another group', !item.parent || item.parent == this);
  
      if(!dropPos) 
        dropPos = {top:window.innerWidth, left:window.innerHeight};
        
      if(typeof(options) == 'undefined')
        options = {};
        
      var self = this;
      
      var wasAlreadyInThisGroup = false;
      var oldIndex = iQ.inArray(item, this._children);
      if(oldIndex != -1) {
        this._children.splice(oldIndex, 1); 
        wasAlreadyInThisGroup = true;
      }
  
      // TODO: You should be allowed to drop in the white space at the bottom and have it go to the end
      // (right now it can match the thumbnail above it and go there)
      function findInsertionPoint(dropPos){
        if(self.shouldStack(self._children.length + 1))
          return 0;
          
        var best = {dist: Infinity, item: null};
        var index = 0;
        var box;
        iQ.each(self._children, function(index, child) {        
          box = child.getBounds();
          if(box.bottom < dropPos.top || box.top > dropPos.top)
            return;
          
          var dist = Math.sqrt( Math.pow((box.top+box.height/2)-dropPos.top,2) 
              + Math.pow((box.left+box.width/2)-dropPos.left,2) );
              
          if( dist <= best.dist ){
            best.item = child;
            best.dist = dist;
            best.index = index;
          }
        });
  
        if( self._children.length > 0 ){
          if(best.item) {
            box = best.item.getBounds();
            var insertLeft = dropPos.left <= box.left + box.width/2;
            if( !insertLeft ) 
              return best.index+1;
            else 
              return best.index;
          } else 
            return self._children.length;
        }
        
        return 0;      
      }
      
      // Insert the tab into the right position.
      var index = findInsertionPoint(dropPos);
      this._children.splice( index, 0, item );
  
      item.setZ(this.getZ() + 1);
      $el.addClass("tabInGroup");
      if( this.isNewTabsGroup() ) $el.addClass("inNewTabGroup")
      
      if(!wasAlreadyInThisGroup) {
        iQ($el.get(0)).droppable("disable");
        item.groupData = {};
    
        item.addOnClose(this, function() {
          self.remove($el);
        });
        
        item.setParent(this);
        
        if(typeof(item.setResizable) == 'function')
          item.setResizable(false);
          
        if(item.tab == Utils.activeTab)
          Groups.setActiveGroup(this);
      }
      
      if(!options.dontArrange)
        this.arrange();
      
      if( this._nextNewTabCallback ){
        this._nextNewTabCallback.apply(this, [item])
        this._nextNewTabCallback = null;
      }
    } catch(e) {
      Utils.log('Group.add error', e);
    }
  },
  
  // ----------  
  // Function: remove
  // Removes an item from the group.
  // Parameters: 
  // 
  //   a - The item to remove. Can be an <Item>, a DOM element or a jQuery object.
  //       The latter two must refer to the container of an <Item>.
  //   options - An object with optional settings for this call. Currently the only one is dontArrange. 
  remove: function(a, options) {
    var $el;  
    var item;
     
    if(a.isAnItem) {
      item = a;
      $el = iQ(item.container);
    } else {
      $el = iQ(a);  
      item = Items.item($el);
    }
    
    if(typeof(options) == 'undefined')
      options = {};
    
    var index = iQ.inArray(item, this._children);
    if(index != -1)
      this._children.splice(index, 1); 
    
    item.setParent(null);
    item.removeClass("tabInGroup");
    item.removeClass("inNewTabGroup")    
    item.removeClass("stacked");
    item.removeClass("stack-trayed");
    item.setRotation(0);
    item.setSize(item.defaultSize.x, item.defaultSize.y);

    iQ($el.get(0)).droppable("enable");    
    item.removeOnClose(this);
    
    if(typeof(item.setResizable) == 'function')
      item.setResizable(true);

    if(this._children.length == 0 && !this.locked.close && !this.getTitle() && !options.dontClose){
      this.close();
    } else if(!options.dontArrange) {
      this.arrange();
    }
  },
  
  // ----------
  removeAll: function() {
    var self = this;
    var toRemove = iQ.merge([], this._children);
    iQ.each(toRemove, function(index, child) {
      self.remove(child, {dontArrange: true});
    });
  },
    
  // ----------  
  setNewTabButtonBounds: function(box, immediately) {
    var css = {
      left: box.left,
      top: box.top,
      width: box.width,
      height: box.height
    };
    
/*     this.$ntb.stop(true, true);     */
    if(!immediately)
      this.$ntb.animate(css, {
        duration: 320,
        easing: 'tabcandyBounce'
      });
    else
      this.$ntb.css(css);
  },
  
  // ----------  
  shouldStack: function(count) {
    if(count <= 1)
      return false;
      
    var bb = this.getContentBounds();
    var options = {
      pretend: true,
      count: (this.isNewTabsGroup() ? count + 1 : count)
    };
    
    var rects = Items.arrange(null, bb, options);
    return (rects[0].width < TabItems.minTabWidth * 1.35 );
  },

  // ----------  
  arrange: function(options) {
    if(this.expanded) {
      this.topChild = null;
      var box = new Rect(this.expanded.bounds);
      box.inset(8, 8);
      Items.arrange(this._children, box, iQ.extend({}, options, {padding: 8, z: 99999}));
    } else {
      var bb = this.getContentBounds();
      var count = this._children.length;
      if(!this.shouldStack(count)) {
        var animate;
        if(!options || typeof(options.animate) == 'undefined') 
          animate = true;
        else 
          animate = options.animate;
    
        if(typeof(options) == 'undefined')
          options = {};
          
        this._children.forEach(function(child){
            child.removeClass("stacked")
        });
  
        this.topChild = null;
        
        var arrangeOptions = Utils.copy(options);
        iQ.extend(arrangeOptions, {
          pretend: true,
          count: count
        });

        if(this.isNewTabsGroup()) {
          arrangeOptions.count++;
        } else if(!count)
          return;
    
        var rects = Items.arrange(this._children, bb, arrangeOptions);
        
        iQ.each(this._children, function(index, child) {
          if(!child.locked.bounds) {
            child.setBounds(rects[index], !animate);
            child.setRotation(0);
            if(options.z)
              child.setZ(options.z);
          }
        });
        
        if(this.isNewTabsGroup()) {
          var box = rects[rects.length - 1];
          box.left -= this.bounds.left;
          box.top -= this.bounds.top;
          this.setNewTabButtonBounds(box, !animate);
        }
        
        this._isStacked = false;
      } else
        this._stackArrange(bb, options);
    }
  },
  
  // ----------
  _stackArrange: function(bb, options) { 
    var animate;
    if(!options || typeof(options.animate) == 'undefined') 
      animate = true;
    else 
      animate = options.animate;

    if(typeof(options) == 'undefined')
      options = {};

    var count = this._children.length;
    if(!count)
      return;
    
    var zIndex = this.getZ() + count + 1;
    
    var scale = 0.8;
    var newTabsPad = 10;
    var w;
    var h; 
    var itemAspect = TabItems.tabHeight / TabItems.tabWidth;
    var bbAspect = bb.height / bb.width;
    if(bbAspect > itemAspect) { // Tall, thin group
      w = bb.width * scale;
      h = w * itemAspect;
    } else { // Short, wide group
      h = bb.height * scale;
      w = h * (1 / itemAspect);
    }
    
    var x = (bb.width - w) / 2;
    if(this.isNewTabsGroup())
      x -= (w + newTabsPad) / 2;
      
    var y = Math.min(x, (bb.height - h) / 2);
    var box = new Rect(bb.left + x, bb.top + y, w, h);
    
    var self = this;
    var children = [];
    iQ.each(this._children, function(index, child) {
      if(child == self.topChild)
        children.unshift(child);
      else
        children.push(child);
    });
    
    iQ.each(children, function(index, child) {
      if(!child.locked.bounds) {
        child.setZ(zIndex);
        zIndex--;
        
        child.addClass("stacked");
        child.setBounds(box, !animate);
        child.setRotation(self._randRotate(35, index));
      }
    });
    
    if(this.isNewTabsGroup()) {
      box.left += box.width + newTabsPad;
      box.left -= this.bounds.left;
      box.top -= this.bounds.top;
      this.setNewTabButtonBounds(box, !animate);
    }

    self._isStacked = true;
  },

  // ----------
  _randRotate: function(spread, index){
    if( index >= this._stackAngles.length ){
      var randAngle = 5*index + parseInt( (Math.random()-.5)*1 );
      this._stackAngles.push(randAngle);
      return randAngle;          
    }
    
    if( index > 5 ) index = 5;

    return this._stackAngles[index];
  },

  // ----------
  // Function: childHit
  // Called by one of the group's children when the child is clicked on. Returns an object:
  //   shouldZoom - true if the browser should launch into the tab represented by the child
  //   callback - called after the zoom animation is complete
  childHit: function(child) {
    var self = this;
    
    // ___ normal click
    if(!this._isStacked || this.expanded) {
      return {
        shouldZoom: true,
        callback: function() {
          self.collapse();
        }
      };
    }

    // ___ we're stacked, but command isn't held down
    if( Keys.meta == false ){
      Groups.setActiveGroup(self);
      return { shouldZoom: true };      
    }
      
    // ___ we're stacked, and command is held down so expand
    Groups.setActiveGroup(self);
    var startBounds = child.getBounds();
    var $tray = iQ("<div>").css({
      top: startBounds.top,
      left: startBounds.left,
      width: startBounds.width,
      height: startBounds.height,
      position: "absolute",
      zIndex: 99998
    }).appendTo("body");

    var w = 180;
    var h = w * (TabItems.tabHeight / TabItems.tabWidth) * 1.1;
    var padding = 20;
    var col = Math.ceil(Math.sqrt(this._children.length));
    var row = Math.ceil(this._children.length/col);

    var overlayWidth = Math.min(window.innerWidth - (padding * 2), w*col + padding*(col+1));
    var overlayHeight = Math.min(window.innerHeight - (padding * 2), h*row + padding*(row+1));
    
    var pos = {left: startBounds.left, top: startBounds.top};
    pos.left -= overlayWidth/3;
    pos.top  -= overlayHeight/3;      
          
    if( pos.top < 0 )  pos.top = 20;
    if( pos.left < 0 ) pos.left = 20;      
    if( pos.top+overlayHeight > window.innerHeight ) pos.top = window.innerHeight-overlayHeight-20;
    if( pos.left+overlayWidth > window.innerWidth )  pos.left = window.innerWidth-overlayWidth-20;
    
    $tray
      .animate({
        width:  overlayWidth,
        height: overlayHeight,
        top: pos.top,
        left: pos.left
      }, {
        duration: 350, 
        easing: 'tabcandyBounce'
      })
      .addClass("overlay");

    this._children.forEach(function(child){
      child.addClass("stack-trayed");
    });

    var $shield = iQ('<div>')
      .css({
        left: 0,
        top: 0,
        width: window.innerWidth,
        height: window.innerHeight,
        position: 'absolute',
        zIndex: 99997
      })
      .appendTo('body')
      .mouseover(function() {
        self.collapse();
      })
      .click(function() { // just in case
        self.collapse();
      });
      
    this.expanded = {
      $tray: $tray,
      $shield: $shield,
      bounds: new Rect(pos.left, pos.top, overlayWidth, overlayHeight)
    };
    
    this.arrange();

    return {};
  },

  // ----------
  collapse: function() {
    if(this.expanded) {
      var z = this.getZ();
      var box = this.getBounds();
      this.expanded.$tray
        .css({
          zIndex: z + 1
        })
        .animate({
          width:  box.width,
          height: box.height,
          top: box.top,
          left: box.left,
          opacity: 0
        }, {
          duration: 350,
          easing: 'tabcandyBounce',
          complete: function() {
            iQ(this).remove();  
          }
        });
  
      this.expanded.$shield.remove();
      this.expanded = null;

      this._children.forEach(function(child){
        child.removeClass("stack-trayed");
      });
                  
      this.arrange({z: z + 2});
    }
  },
  
  // ----------  
  _addHandlers: function(container) {
    var self = this;
    
    if(!this.locked.bounds) {
      iQ(container).draggable({
        cancelClass: 'close name',
        start: function(e, ui){
          drag.info = new DragInfo(this, e);
        },
        drag: function(e, ui){
          drag.info.drag(e, ui);
        }, 
        stop: function() {
          drag.info.stop();
          drag.info = null;
        }
      });
    }
    
    iQ(container)
      .mousedown(function(e){
        self._mouseDownLocation = new Point(e.clientX, e.clientY);
      })    
      .mouseup(function(e){
        // Don't zoom in on clicks inside of the title area.
        if( e.target.className == "title-shield" || e.target.className == "name" ) return;
        
        var location = new Point(e.clientX, e.clientY);
      
        if( !self._mouseDownLocation || location.distance(self._mouseDownLocation) > 1.0 ) 
          return;
        // Don't zoom in to the last tab for the new tab group.
        if( self.isNewTabsGroup() ) return;
        var activeTab = self.getActiveTab();
        if( activeTab ) TabItems.zoomTo(activeTab)
        // TODO: This should also accept TabItems
        else if(self.getChild(0))
          TabItems.zoomTo(self.getChild(0).tab.mirror.el);
          
        self._mouseDownLocation = null;
    });
    
    iQ(container).droppable({
      over: function(){
        if( !self.isNewTabsGroup() )
          iQ(this).addClass("acceptsDrop");
      },
      out: function(){
        var group = drag.info.item.parent;
        if(group) {
          group.remove(drag.info.$el, {dontClose: true});
        }
          
        iQ(this).removeClass("acceptsDrop");
      },
      drop: function(event){
        iQ(this).removeClass("acceptsDrop");
        self.add( drag.info.$el, {left:event.pageX, top:event.pageY} );
      },
      accept: dropAcceptFunction
    });
  },

  // ----------  
  setResizable: function(value){
    var self = this;
    
    if(value) {
      this.$resizer.fadeIn();
      iQ(this.container).resizable({
        aspectRatio: false,
        minWidth: 90,
        minHeight: 90,
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
      this.$resizer.fadeOut();
      iQ(this.container).resizable('destroy');
    }
  },
  
  // ----------
  newTab: function() {
    Groups.setActiveGroup(this);          
    var newTab = Tabs.open("about:blank", true);
    
    // Because opening a new tab happens in a different thread(?)
    // calling Page.hideChrome() inline won't do anything. Instead
    // we have to marshal it. A value of 0 wait time doesn't seem
    // to work. Instead, we use a value of 1 which seems to be the
    // minimum amount of time required.
    iQ.timeout(function(){
      Page.hideChrome()
    }, 1);
    
    var self = this;
    var doNextTab = function(tab){
      var group = Groups.getActiveGroup();

      iQ(tab.container).css({opacity: 0});
      var $anim = iQ("<div>")
        .addClass('newTabAnimatee')
        .css({
          top: tab.bounds.top+5,
          left: tab.bounds.left+5,
          width: tab.bounds.width-10,
          height: tab.bounds.height-10,
          zIndex: 999,
          opacity: 0
        })      
        .appendTo("body")
        .animate({
          opacity: 1.0
        }, {
          duration: 500, 
          complete: function() {
            $anim.animate({
              top: 0,
              left: 0,
              width: window.innerWidth,
              height: window.innerHeight
            }, {
              duration: 270, 
              complete: function(){
                iQ(tab.container).css({opacity: 1});
                newTab.focus();
                Page.showChrome()
                UI.navBar.urlBar.focus();
                $anim.remove();
                // We need a timeout here so that there is a chance for the
                // new tab to get made! Otherwise it won't appear in the list
                // of the group's tab.
                // TODO: This is probably a terrible hack that sets up a race
                // condition. We need a better solution.
                iQ.timeout(function(){
                  UI.tabBar.showOnlyTheseTabs(Groups.getActiveGroup()._children);
                }, 400);
              }
            });      
          }
        });
    }    
    
    // TODO: Because this happens as a callback, there is
    // sometimes a long delay before the animation occurs.
    // We need to fix this--immediate response to a users
    // actions is necessary for a good user experience.
    
    self.onNextNewTab(doNextTab); 
  },

  // ----------
  // Function: reorderBasedOnTabOrder
  // Reorderes the tabs in a group based on the arrangment of the tabs
  // shown in the tab bar. It doesn't it by sorting the children
  // of the group by the positions of their respective tabs in the
  // tab bar.
  // 
  // Parameters: 
  //   topChild - the <Item> that should be displayed on top of a stack
  reorderBasedOnTabOrder: function(topChild){    
    this.topChild = topChild;
    
    var groupTabs = [];
    for( var i=0; i<UI.tabBar.el.children.length; i++ ){
      var tab = UI.tabBar.el.children[i];
      if( tab.collapsed == false )
        groupTabs.push(tab);
    }
     
    this._children.sort(function(a,b){
      return groupTabs.indexOf(a.tab.raw) - groupTabs.indexOf(b.tab.raw)
    });
    
    this.arrange({animate: false});
    // this.arrange calls this.save for us
  },
  
  // ----------
  // Function: getChild
  // Returns the nth child tab or null if index is out of range.
  //
  // Parameters:
  //  index - the index of the child tab to return, use negative
  //          numbers to index from the end (-1 is the last child)
  getChild: function(index){
    if( index < 0 ) index = this._children.length+index;
    if( index >= this._children.length || index < 0 ) return null;
    return this._children[index];
  },
  
  // ---------
  // Function: onNextNewTab
  // Sets up a one-time handler that gets called the next time a
  // tab is added to the group.
  //
  // Parameters:
  //  callback - the one-time callback that is fired when the next
  //             time a tab is added to a group; it gets passed the
  //             new tab
  onNextNewTab: function(callback){
    this._nextNewTabCallback = callback;
  }
});

// ##########
// Class: DragInfo
// Helper class for dragging <Item>s
// 
// ----------
// Constructor: DragInfo
// Called to create a DragInfo in response to a jQuery-UI draggable "start" event.
var DragInfo = function(element, event) {
  this.el = element;
  this.$el = iQ(this.el);
  this.item = Items.item(this.el);
  this.parent = this.item.parent;
  this.startPosition = new Point(event.clientX, event.clientY);
  this.startTime = Utils.getMilliseconds();
  
  this.$el.data('isDragging', true);
  this.item.setZ(999999);
  
  // When a tab drag starts, make it the focused tab.
  if(this.item.isAGroup) {
    var tab = Page.getActiveTab();
    if(!tab || tab.parent != this.item) {
      if(this.item._children.length)
        Page.setActiveTab(this.item._children[0]);
    }
  } else
    Page.setActiveTab(this.item);
};

DragInfo.prototype = {
  // ----------  
  snap: function(event, ui){
    var me = this.item;
    function closeTo(a,b){ return Math.abs(a-b) <= 12 }
        
    // Snap inline to the tops of other groups.
    var closestTop = null;
    var minDist = Infinity;
    for each(var group in Groups.groups){
      // Exlcude the current group
      if( group == me ) continue;      
      var dist = Math.abs(group.bounds.top - me.bounds.top);
      if( dist < minDist ){
        minDist = dist;
        closestTop = group.bounds.top;
      }
    }
    
    if( closeTo(ui.position.top, closestTop) ){
      ui.position.top = closestTop;
    }
      
    // Snap to the right of other groups by topish left corner
    var topLeft = new Point( me.bounds.left, ui.position.top + 25 );
    var other = Groups.findGroupClosestToPoint(topLeft, {exclude:me});     
    var closestRight = other.bounds.right + 20;
    if( closeTo(ui.position.left, closestRight) ){
      ui.position.left = closestRight;
    }

    // Snap to the bottom of other groups by top leftish corner
    var topLeft = new Point( me.bounds.left+25, ui.position.top);
    var other = Groups.findGroupClosestToPoint(topLeft, {exclude:me});     
    var closestBottom = other.bounds.bottom + 20;
    if( closeTo(ui.position.top, closestBottom) ){
      ui.position.top = closestBottom;
    }
    
    // Snap inline to the right of other groups by top right corner
    var topRight = new Point( me.bounds.right, ui.position.top);
    var other = Groups.findGroupClosestToPoint(topRight, {exclude:me});     
    var closestRight = other.bounds.right;
    if( closeTo(ui.position.left + me.bounds.width, closestRight) ){
      ui.position.left = closestRight - me.bounds.width;
    }      
        
    // Snap inline to the left of other groups by top left corner
    var topLeft = new Point( me.bounds.left, ui.position.top);
    var other = Groups.findGroupClosestToPoint(topLeft, {exclude:me});     
    var closestLeft = other.bounds.left;
    if( closeTo(ui.position.left, closestLeft) ){
      ui.position.left = closestLeft;
    }  
    
    // Step 4: Profit?
    return ui;
    
  },
  
  // ----------  
  // Function: drag
  // Called in response to a jQuery-UI draggable "drag" event.
  drag: function(event, ui) {
    if(this.item.isAGroup) {
      //ui = this.snap(event,ui);      
      var bb = this.item.getBounds();
      bb.left = ui.position.left;
      bb.top = ui.position.top;
      this.item.setBounds(bb, true);
    } else
      this.item.reloadBounds();
      
    if(this.parent && this.parent.expanded) {
      var now = Utils.getMilliseconds();
      var distance = this.startPosition.distance(new Point(event.clientX, event.clientY));
      if(/* now - this.startTime > 500 ||  */distance > 100) {
        this.parent.remove(this.item);
        this.parent.collapse();
      }
    }
  },

  // ----------  
  // Function: stop
  // Called in response to a jQuery-UI draggable "stop" event.
  stop: function() {
    this.$el.data('isDragging', false);    

    // I'm commenting this out for a while as I believe it feels uncomfortable
    // that groups go away when there is still a tab in them. I do this at
    // the cost of symmetry. -- Aza
    /*
    if(this.parent && !this.parent.locked.close && this.parent != this.item.parent 
        && this.parent._children.length == 1 && !this.parent.getTitle()) {
      this.parent.remove(this.parent._children[0]);
    }*/

    if(this.parent && !this.parent.locked.close && this.parent != this.item.parent 
        && this.parent._children.length == 0 && !this.parent.getTitle()) {
      this.parent.close();
    }
     
    if(this.parent && this.parent.expanded)
      this.parent.arrange();
      
    if(this.item && !this.item.parent) {
      this.item.setZ(drag.zIndex);
      drag.zIndex++;
      
      this.item.reloadBounds();
      this.item.pushAway();
    }
    
  }
};

// ----------
// Variable: drag
// The DragInfo that's currently in process. 
var drag = {
  info: null,
  zIndex: 100
};

// ##########
// Class: Groups
// Singelton for managing all <Group>s. 
window.Groups = {
  // ----------  
  dragOptions: {
    cancelClass: 'close',
    start: function(e, ui) {
      drag.info = new DragInfo(this, e);
    },
    drag: function(e, ui) {
      drag.info.drag(e, ui);
    },
    stop: function() {
      drag.info.stop();
      drag.info = null;
    }
  },
  
  // ----------  
  dropOptions: {
    accept: dropAcceptFunction,
    drop: function(e){
      $target = iQ(this);  
      iQ(this).removeClass("acceptsDrop");
      var phantom = $target.data("phantomGroup")
      
      var group = drag.info.item.parent;
      if( group == null ){
        phantom.removeClass("phantom");
        phantom.removeClass("group-content");
        var group = new Group([$target, drag.info.$el], {container:phantom});
      } else 
        group.add( drag.info.$el );      
    },
    over: function(e){
      var $target = iQ(this);

      function elToRect($el){
       return new Rect( $el.position().left, $el.position().top, $el.width(), $el.height() );
      }

      var height = elToRect($target).height * 1.5 + 20;
      var width = elToRect($target).width * 1.5 + 20;
      var unionRect = elToRect($target).union( elToRect(drag.info.$el) );

      var newLeft = unionRect.left + unionRect.width/2 - width/2;
      var newTop = unionRect.top + unionRect.height/2 - height/2;

      iQ(".phantom").remove();
      var phantom = iQ("<div>")
        .addClass('group phantom group-content')
        .css({
          width: width,
          height: height,
          position:"absolute",
          top: newTop,
          left: newLeft,
          zIndex: -99
        })
        .appendTo("body")
        .hide()
        .fadeIn();
        
      $target.data("phantomGroup", phantom);      
    },
    out: function(e){      
      var phantom = iQ(this).data("phantomGroup");
      if(phantom) { 
        phantom.fadeOut(function(){
          iQ(this).remove();
        });
      }
    }
  }, 
  
  // ----------
  init: function() {
    this.groups = [];
    this.nextID = 1;
    this._inited = false;
  },
  
  // ----------
  getNextID: function() {
    var result = this.nextID;
    this.nextID++;
    this.save();
    return result;
  },

  // ----------
  getStorageData: function() {
    var data = {nextID: this.nextID, groups: []};
    iQ.each(this.groups, function(index, group) {
      data.groups.push(group.getStorageData());
    });
    
    return data;
  },
  
  // ----------
  saveAll: function() {
    this.save();
    iQ.each(this.groups, function(index, group) {
      group.save();
    });
  },
  
  // ----------
  save: function() {
    if (!this._inited) // too soon to save now
      return;

    Storage.saveGroupsData(Utils.getCurrentWindow(), {nextID:this.nextID});
  },

  // ----------
  reconstitute: function(groupsData, groupData) {
    try {
      if(groupsData && groupsData.nextID)
        this.nextID = groupsData.nextID;
        
      if(groupData) {
        for (var id in groupData) {
          var group = groupData[id];
          if(this.groupStorageSanity(group)) {
            var isNewTabsGroup = (group.title == 'New Tabs');
            var options = {
              locked: {
                close: isNewTabsGroup, 
                title: isNewTabsGroup,
                bounds: isNewTabsGroup
              },
              dontPush: true
            };
            
            new Group([], iQ.extend({}, group, options)); 
          }
        }
      }
      
      var group = this.getNewTabGroup();
      if(!group) {
        var box = this.getBoundsForNewTabGroup();
        var options = {
          locked: {
            close: true, 
            title: true,
            bounds: true
          },
          dontPush: true, 
          bounds: box,
          title: 'New Tabs'
        };
  
        new Group([], options); 
      } 
      
      this.repositionNewTabGroup();
      
      this._inited = true;
      this.save(); // for nextID
    }catch(e){
      Utils.log("error in recons: "+e);
    }
  },
  
  // ----------
  groupStorageSanity: function(groupData) {
    // TODO: check everything 
    var sane = true;
    if(!isRect(groupData.bounds)) {
      Utils.log('Groups.groupStorageSanity: bad bounds', groupData.bounds);
      sane = false;
    }
    
    return sane;
  },
  
  // ----------
  getGroupWithTitle: function(title) {
    var result = null;
    iQ.each(this.groups, function(index, group) {
      if(group.getTitle() == title) {
        result = group;
        return false;
      }
    });
    
    return result;
  }, 
 
  // ----------
  getNewTabGroup: function() {
    var groupTitle = 'New Tabs';
    var array = iQ.grep(this.groups, function(group) {
      return group.getTitle() == groupTitle;
    });
    
    if(array.length) 
      return array[0];
      
    return null;
  },

  // ----------
  getBoundsForNewTabGroup: function() {
    var pad = 0;
    var sw = window.innerWidth;
    var sh = window.innerHeight;
    var w = sw - (pad * 2);
    var h = TabItems.tabHeight * 0.9 + pad*2;
    return new Rect(pad, sh - (h + pad), w, h);
  },

  // ----------
  repositionNewTabGroup: function() {
    var box = this.getBoundsForNewTabGroup();
    var group = this.getNewTabGroup();
    group.setBounds(box, true);
  },
  
  // ----------  
  register: function(group) {
    Utils.assert('group', group);
    Utils.assert('only register once per group', iQ.inArray(group, this.groups) == -1);
    this.groups.push(group);
  },
  
  // ----------  
  unregister: function(group) {
    var index = iQ.inArray(group, this.groups);
    if(index != -1)
      this.groups.splice(index, 1);  
    
    if(group == this._activeGroup)
      this._activeGroup = null;   
  },
  
  // ----------
  // Function: group
  // Given some sort of identifier, returns the appropriate group.
  // Currently only supports group ids. 
  group: function(a) {
    var result = null;
    iQ.each(this.groups, function(index, candidate) {
      if(candidate.id == a) {
        result = candidate;
        return false;
      }
    });
    
    return result;
  },
  
  // ----------  
  arrange: function() {
    var bounds = Items.getPageBounds();
    var count = this.groups.length - 1;
    var columns = Math.ceil(Math.sqrt(count));
    var rows = ((columns * columns) - count >= columns ? columns - 1 : columns); 
    var padding = 12;
    var startX = bounds.left + padding;
    var startY = bounds.top + padding;
    var totalWidth = bounds.width - padding;
    var totalHeight = bounds.height - padding;
    var box = new Rect(startX, startY, 
        (totalWidth / columns) - padding,
        (totalHeight / rows) - padding);
    
    var i = 0;
    iQ.each(this.groups, function(index, group) {
      if(group.locked.bounds)
        return; 
        
      group.setBounds(box, true);
      
      box.left += box.width + padding;
      i++;
      if(i % columns == 0) {
        box.left = startX;
        box.top += box.height + padding;
      }
    });
  },
  
  // ----------
  removeAll: function() {
    var toRemove = iQ.merge([], this.groups);
    iQ.each(toRemove, function(index, group) {
      group.removeAll();
    });
  },
  
  // ----------
  newTab: function(tabItem) {
    var group = this.getActiveGroup();
    if( group == null )
      group = this.getNewTabGroup();
    
    var $el = iQ(tabItem.container);
    if(group) group.add($el);
  },
  
  // ----------
  // Function: getActiveGroup
  // Returns the active group. Active means the group where a new
  // tab will live when it is created as well as what tabs are
  // shown in the tab bar when not in the TabCandy interface.
  getActiveGroup: function() {
    return this._activeGroup;
  },
  
  // ----------
  // Function: setActiveGroup
  // Sets the active group, thereby showing only the relavent tabs
  // to that group. The change is visible only if the tab bar is
  // visible.
  //
  // Paramaters
  //  group - the active <Group> or <null> if no group is active
  //          (which means we have an orphaned tab selected)
  setActiveGroup: function(group) {
    this._activeGroup = group;
    this.updateTabBarForActiveGroup();
  },
  
  // ----------
  // Function: updateTabBarForActiveGroup
  // Hides and shows tabs in the tab bar based on the active group.
  updateTabBarForActiveGroup: function() {
    if(!window.UI)
      return; // called too soon
      
    if(this._activeGroup)
      UI.tabBar.showOnlyTheseTabs( this._activeGroup._children );
    else if( this._activeGroup == null)
      UI.tabBar.showOnlyTheseTabs( this.getOrphanedTabs());
  },
  
  // ----------
  // Function: getOrphanedTabs
  // Returns an array of all tabs that aren't in a group
  getOrphanedTabs: function(){
    var tabs = TabItems.getItems();
    tabs = tabs.filter(function(tab){
      return tab.parent == null;
    });
    return tabs;
  },
  
  // ---------
  // Function: findGroupClosestToPoint
  // Given a <Point> returns an object which contains
  // the group and it's closest side: {group:<Group>, side:"top|left|right|bottom"}
  //
  // Paramters
  //  - <Point>
  //  - options
  //   + exclude: <Group> will exclude a group for being matched against
  findGroupClosestToPoint: function(point, options){
    minDist = Infinity;
    closestGroup = null;
    var onSide = null;
    for each(var group in this.groups){
      // Step 0: Exlcude any exluded groups
      if( options && options.exclude && options.exclude == group ) continue; 
      
      // Step 1: Find the closest side
      var sideDists = [];
      sideDists.push( [Math.abs(group.bounds.top    - point.y), "top"] );
      sideDists.push( [Math.abs(group.bounds.bottom - point.y), "bottom"] );      
      sideDists.push( [Math.abs(group.bounds.left   - point.x), "left"] );
      sideDists.push( [Math.abs(group.bounds.right  - point.x), "right"] );
      sideDists.sort(function(a,b){return a[0]-b[0]});
      var closestSide = sideDists[0][1];
      
      // Step 2: Find where one that side is closest to the point.
      if( closestSide == "top" || closestSide == "bottom" ){
        var closestPoint = new Point(0, group.bounds[closestSide]);
        closestPoint.x = Math.max(Math.min(point.x, group.bounds.right), group.bounds.left);
      } else {
        var closestPoint = new Point(group.bounds[closestSide], 0);
        closestPoint.y = Math.max(Math.min(point.y, group.bounds.bottom), group.bounds.top);        
      }
      
      // Step 3: Calculate the distance from the closest point on the rect
      // to the given point      
      var dist = closestPoint.distance(point);
      if( dist < minDist ){
        closestGroup = group;
        onSide = closestSide;
        minDist = dist;
      }
      
    }
    
    return closestGroup;
  }
  
};

// ----------
Groups.init();

})();
