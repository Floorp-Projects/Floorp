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
 * The Original Code is groups.js.
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
// Title: groups.js

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
  if (typeof(options) == 'undefined')
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
  
  this.keepProportional = false;
  
  // Variable: _activeTab
  // The <TabItem> for the group's active tab. 
  this._activeTab = null;
  
   // Variables: xDensity, yDensity
   // "density" ranges from 0 to 1, with 0 being "not dense" = "squishable" and 1 being "dense"
   // = "not squishable". For example, if there is extra space in the vertical direction, 
   // yDensity will be < 1. These are set by <Group.arrange>, as it is dependent on the tab items
   // inside the group.
  this.xDensity = 0;
  this.yDensity = 0;

  
  if (isPoint(options.userSize))  
    this.userSize = new Point(options.userSize);

  var self = this;

  var rectToBe;
  if (options.bounds)
    rectToBe = new Rect(options.bounds);
    
  if (!rectToBe) {
    rectToBe = Groups.getBoundingBox(listOfEls);
    rectToBe.inset( -30, -30 );
  }

  var $container = options.container; 
  if (!$container) {
    $container = iQ('<div>')
      .addClass('group')
      .css({position: 'absolute'})
      .css(rectToBe);
    
    if ( this.isNewTabsGroup() ) $container.addClass("newTabGroup");
  }
  
  this.bounds = $container.bounds();
  
  this.isDragging = false;
  $container
    .css({zIndex: -100})
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
    
  this.$ntb.get(0).title = 'New tab';
  
  if ( this.isNewTabsGroup() ) this.$ntb.html("<span>+</span>");
    
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
    "</div>";
       
  this.$titlebar = iQ('<div>')
    .addClass('titlebar')
    .html(html)        
    .appendTo($container);
    
  this.$titlebar.css({
      position: "absolute",
    });
  
  var $close = iQ('<div>')
    .addClass('close')
    .click(function() {
      self.closeAll();
    })
    .appendTo($container);
  
  // ___ Title 
  this.$titleContainer = iQ('.title-container', this.$titlebar);
  this.$title = iQ('.name', this.$titlebar);
  this.$titleShield = iQ('.title-shield', this.$titlebar);
  
  var titleUnfocus = function() {
    self.$titleShield.show();
    if (!self.getTitle()) {
      self.$title
        .addClass("defaultName")
        .val(self.defaultName);
    } else {
      self.$title
        .css({"background":"none"})
        .animate({
          "padding-left": "1px"
        }, {
          duration: 200,
          easing: 'tabcandyBounce'
        });
    }
  };
  
  var handleKeyPress = function(e){
    if ( e.which == 13 ) { // return
      self.$title.get(0).blur();
      self.$title
        .addClass("transparentBorder")
        .one("mouseout", function(){
          self.$title.removeClass("transparentBorder");
        });
    } else 
      self.adjustTitleSize();
      
    self.save();
  };
  
  this.$title
    .css({backgroundRepeat: 'no-repeat'})
    .blur(titleUnfocus)
    .focus(function() {
      if (self.locked.title) {
        self.$title.get(0).blur();
        return;
      }  
      self.$title.get(0).select();
      if (!self.getTitle()) {
        self.$title
          .removeClass("defaultName")
          .val('');
      }
    })
    .keyup(handleKeyPress);
  
  titleUnfocus();
  
  if (this.locked.title)
    this.$title.addClass('name-locked');
  else {
    this.$titleShield
      .mousedown(function(e) {
        self.lastMouseDownTarget = (Utils.isRightClick(e) ? null : e.target);
      })
      .mouseup(function(e) { 
        var same = (e.target == self.lastMouseDownTarget);
        self.lastMouseDownTarget = null;
        if (!same)
          return;
        
        if (!self.isDragging) {        
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
    
  // ___ Stack Expander
  this.$expander = iQ("<img/>")
    .attr('src', 'chrome://browser/skin/tabcandy/stack-expander.png')
    .addClass("stackExpander")
    .appendTo($container)
    .hide(); 
  
  // ___ locking
  if (this.locked.bounds)
    $container.css({cursor: 'default'});    
    
  if (this.locked.close)
    $close.hide();
    
  // ___ Superclass initialization
  this._init($container.get(0));

  if (this.$debug) 
    this.$debug.css({zIndex: -1000});
  
  // ___ Children
  Array.prototype.forEach.call(listOfEls, function(el) {  
    self.add(el, null, options);
  });

  // ___ Finish Up
  this._addHandlers($container);
  
  if (!this.locked.bounds)
    this.setResizable(true);
  
  Groups.register(this);

  // ___ Position
  this.setBounds(rectToBe);
  this.snap();
  
  // ___ Push other objects away
  if (!options.dontPush)
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

  // ----------
  // Accepts a callback that will be called when this item closes. 
  // The referenceObject is used to facilitate removal if necessary. 
  addOnClose: function(referenceObject, callback) {
    this.addSubscriber(referenceObject, "close", callback);      
  },

  // ----------
  // Removes the close event callback associated with referenceObject.
  removeOnClose: function(referenceObject) {
    this.removeSubscriber(referenceObject, "close");      
  },
  
  // -----------
  // Function: setActiveTab
  // Sets the active <TabItem> for this group
  setActiveTab: function(tab){
    Utils.assert('tab must be a TabItem', tab && tab.isATabItem);
    this._activeTab = tab;
  },

  // -----------
  // Function: getActiveTab
  // Gets the active <TabItem> for this group
  getActiveTab: function(){
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
    
    if (isPoint(this.userSize))  
      data.userSize = new Point(this.userSize);
  
    return data;
  },

  // ----------
  // Function: isEmpty
  // Returns true if the tab group is empty and unnamed.
  isEmpty: function() {
    return !this._children.length && !this.getTitle();
  },

  // ----------
  // Function: save
  // Saves this group to persistent storage. 
  save: function() {
    if (!this._inited) // too soon to save now
      return;

    var data = this.getStorageData();
    if (Groups.groupStorageSanity(data))
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
  // Function: getTitle
  // Returns the title of this group as a string.
  getTitle: function() {
    var value = (this.$title ? this.$title.val() : '');
    return (value == this.defaultName ? '' : value);
  },

  // ----------  
  // Function: setTitle
  // Sets the title of this group with the given string
  setTitle: function(value) {
    this.$title.val(value); 
    this.save();
  },

  // ----------  
  // Function: adjustTitleSize
  // Used to adjust the width of the title box depending on group width and title size.
  adjustTitleSize: function() {
    Utils.assert('bounds needs to have been set', this.bounds);
    var w = Math.min(this.bounds.width - 35, Math.max(150, this.getTitle().length * 6));
    var css = {width: w};
    this.$title.css(css);
    this.$titleShield.css(css);
  },
  
  // ----------  
  // Function: getContentBounds
  // Returns a <Rect> for the group's content area (which doesn't include the title, etc). 
  getContentBounds: function() {
    var box = this.getBounds();
    var titleHeight = this.$titlebar.height();
    box.top += titleHeight;
    box.height -= titleHeight;
    box.inset(6, 6);
    
    if (this.isNewTabsGroup())
      box.height -= 12; // Hack for tab titles
    else
      box.height -= 33; // For new tab button
      
    return box;
  },
  
  // ----------  
  // Function: setBounds
  // Sets the bounds with the given <Rect>, animating unless "immediately" is false.
  // 
  // Parameters: 
  //   rect - a <Rect> giving the new bounds
  //   immediately - true if it should not animate; default false
  //   options - an object with additional parameters, see below
  // 
  // Possible options: 
  //   force - true to always update the DOM even if the bounds haven't changed; default false
  setBounds: function(rect, immediately, options) {
    if (!isRect(rect)) {
      Utils.trace('Group.setBounds: rect is not a real rectangle!', rect);
      return;
    }
    
    if (!options)
      options = {};
    
    rect.width = Math.max( 110, rect.width );
    rect.height = Math.max( 125, rect.height);
    
    var titleHeight = this.$titlebar.height();
    
    // ___ Determine what has changed
    var css = {};
    var titlebarCSS = {};
    var contentCSS = {};

    if (rect.left != this.bounds.left || options.force)
      css.left = rect.left;
      
    if (rect.top != this.bounds.top || options.force)
      css.top = rect.top;
      
    if (rect.width != this.bounds.width || options.force) {
      css.width = rect.width;
      titlebarCSS.width = rect.width;
      contentCSS.width = rect.width;
    }

    if (rect.height != this.bounds.height || options.force) {
      css.height = rect.height;
      contentCSS.height = rect.height - titleHeight;
    }
      
    if (iQ.isEmptyObject(css))
      return;
      
    var offset = new Point(rect.left - this.bounds.left, rect.top - this.bounds.top);
    this.bounds = new Rect(rect);

    // ___ Deal with children
    if (css.width || css.height) {
      this.arrange({animate: !immediately}); //(immediately ? 'sometimes' : true)});
    } else if (css.left || css.top) {
      this._children.forEach(function(child) {
        var box = child.getBounds();
        child.setPosition(box.left + offset.x, box.top + offset.y, immediately);
      });
    }

    // ___ Update our representation
    if (immediately) {
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
      
      this.$titlebar.animate(titlebarCSS, {
        duration: 350
      });
      
      this.$content.animate(contentCSS, {
        duration: 350
      });
    }
    
    this.adjustTitleSize();

    this._updateDebugBounds();

    if (!this.isNewTabsGroup())
      this.setTrenches(rect);

    this.save();
  },
    
  // ----------
  // Function: setZ
  // Set the Z order for the group's container, as well as its children. 
  setZ: function(value) {
    this.zIndex = value;

    iQ(this.container).css({zIndex: value});

    if (this.$debug) 
      this.$debug.css({zIndex: value + 1});

    var count = this._children.length;
    if (count) {
      var topZIndex = value + count + 1;
      var zIndex = topZIndex;
      var self = this;
      this._children.forEach(function(child) {
        if (child == self.topChild)
          child.setZ(topZIndex + 1);
        else {
          child.setZ(zIndex);
          zIndex--;
        }
      });
    }
  },
    
  // ----------
  // Function: close
  // Closes the group, removing (but not closing) all of its children.
  close: function() {
    this.removeAll();
    this._sendToSubscribers("close");
    Groups.unregister(this);
    this.removeTrenches();
    iQ(this.container).fadeOut(function() {
      iQ(this).remove();
      Items.unsquish();
    });

    Storage.deleteGroup(Utils.getCurrentWindow(), this.id);
  },
  
  // ----------  
  // Function: closeAll
  // Closes the group and all of its children.
  closeAll: function() {
    var self = this;
    if (this._children.length) {
      var toClose = iQ.merge([], this._children);
      toClose.forEach(function(child) {
        child.removeOnClose(self);
        child.close();
      });
    } 
    
    if (!this.locked.close)
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
      if (a.isAnItem) {
        item = a;
        $el = iQ(a.container);  
      } else {
        $el = iQ(a);
        item = Items.item($el);
      }
      
      Utils.assertThrow('shouldn\'t already be in another group', !item.parent || item.parent == this);

      item.removeTrenches();
        
      if (!dropPos) 
        dropPos = {top:window.innerWidth, left:window.innerHeight};
        
      if (typeof(options) == 'undefined')
        options = {};
        
      var self = this;
      
      var wasAlreadyInThisGroup = false;
      var oldIndex = this._children.indexOf(item);
      if (oldIndex != -1) {
        this._children.splice(oldIndex, 1); 
        wasAlreadyInThisGroup = true;
      }
  
      // TODO: You should be allowed to drop in the white space at the bottom and have it go to the end
      // (right now it can match the thumbnail above it and go there)
      function findInsertionPoint(dropPos){
        if (self.shouldStack(self._children.length + 1))
          return 0;
          
        var best = {dist: Infinity, item: null};
        var index = 0;
        var box;
        self._children.forEach(function(child) {        
          box = child.getBounds();
          if (box.bottom < dropPos.top || box.top > dropPos.top)
            return;
          
          var dist = Math.sqrt( Math.pow((box.top+box.height/2)-dropPos.top,2) 
              + Math.pow((box.left+box.width/2)-dropPos.left,2) );
              
          if ( dist <= best.dist ){
            best.item = child;
            best.dist = dist;
            best.index = index;
          }
        });
  
        if ( self._children.length ){
          if (best.item) {
            box = best.item.getBounds();
            var insertLeft = dropPos.left <= box.left + box.width/2;
            if ( !insertLeft ) 
              return best.index+1;
            return best.index;
          }
          return self._children.length;
        }
        
        return 0;      
      }
      
      // Insert the tab into the right position.
      var index = findInsertionPoint(dropPos);
      this._children.splice( index, 0, item );
  
      item.setZ(this.getZ() + 1);
      $el.addClass("tabInGroup");
      if ( this.isNewTabsGroup() ) $el.addClass("inNewTabGroup")
      
      if (!wasAlreadyInThisGroup) {
        item.droppable(false);
        item.groupData = {};
    
        item.addOnClose(this, function() {
          self.remove($el);
        });
        
        item.setParent(this);
        
        if (typeof(item.setResizable) == 'function')
          item.setResizable(false);
          
        if (item.tab == Utils.activeTab)
          Groups.setActiveGroup(this);
      }
      
      if (!options.dontArrange)
        this.arrange();
      
      if ( this._nextNewTabCallback ){
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
    try {
      var $el;  
      var item;
       
      if (a.isAnItem) {
        item = a;
        $el = iQ(item.container);
      } else {
        $el = iQ(a);  
        item = Items.item($el);
      }
      
      if (typeof(options) == 'undefined')
        options = {};
      
      var index = this._children.indexOf(item);
      if (index != -1)
        this._children.splice(index, 1); 
      
      item.setParent(null);
      item.removeClass("tabInGroup");
      item.removeClass("inNewTabGroup")    
      item.removeClass("stacked");
      item.removeClass("stack-trayed");
      item.setRotation(0);
      item.setSize(item.defaultSize.x, item.defaultSize.y);
  
      item.droppable(true);    
      item.removeOnClose(this);
      
      if (typeof(item.setResizable) == 'function')
        item.setResizable(true);
  
      if (!this._children.length && !this.locked.close && !this.getTitle() && !options.dontClose){
        this.close();
      } else if (!options.dontArrange) {
        this.arrange();
      }
    } catch(e) {
      Utils.log(e);
    }
  },
  
  // ----------
  // Function: removeAll
  // Removes all of the group's children.
  removeAll: function() {
    var self = this;
    var toRemove = iQ.merge([], this._children);
    toRemove.forEach(function(child) {
      self.remove(child, {dontArrange: true});
    });
  },
    
  // ----------  
  // Function: setNewTabButtonBounds
  // Used for positioning the "new tab" button in the "new tabs" group.
  setNewTabButtonBounds: function(box, immediately) {
    if (!immediately)
      this.$ntb.animate(box.css(), {
        duration: 320,
        easing: 'tabcandyBounce'
      });
    else
      this.$ntb.css(box.css());
  },
  
  // ----------
  // Function: hideExpandControl
  // Hide the control which expands a stacked group into a quick-look view.
  hideExpandControl: function(){
    this.$expander.hide();
  },

  // ----------
  // Function: showExpandControl
  // Show the control which expands a stacked group into a quick-look view.
  showExpandControl: function(){
    var childBB = this.getChild(0).getBounds();
    var dT = childBB.top - this.getBounds().top;
    var dL = childBB.left - this.getBounds().left;
    
    this.$expander
        .show()
        .css({
          opacity: .2,
          top: dT + childBB.height + Math.min(7, (this.getBounds().bottom-childBB.bottom)/2),
          // TODO: Why the magic -6? because the childBB.width seems to be over-sizing itself.
          // But who can blame an object for being a bit optimistic when self-reporting size.
          // It has to impress the ladies somehow.
          left: dL + childBB.width/2 - this.$expander.width()/2 - 6,
        });
  },

  // ----------  
  // Function: shouldStack
  // Returns true if the group, given "count", should stack (instead of grid). 
  shouldStack: function(count) {
    if (count <= 1)
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
  // Function: arrange
  // Lays out all of the children. 
  // 
  // Parameters: 
  //   options - passed to <Items.arrange> or <_stackArrange>
  arrange: function(options) {
    if (this.expanded) {
      this.topChild = null;
      var box = new Rect(this.expanded.bounds);
      box.inset(8, 8);
      Items.arrange(this._children, box, iQ.extend({}, options, {padding: 8, z: 99999}));
    } else {
      var bb = this.getContentBounds();
      var count = this._children.length;
      if (!this.shouldStack(count)) {
        var animate;
        if (!options || typeof(options.animate) == 'undefined') 
          animate = true;
        else 
          animate = options.animate;
    
        if (typeof(options) == 'undefined')
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

        if (this.isNewTabsGroup()) {
          arrangeOptions.count++;
        } else if (!count) {
          this.xDensity = 0;
          this.yDensity = 0;
          return;
        }
    
        var rects = Items.arrange(this._children, bb, arrangeOptions);
        
        // yDensity = (the distance of the bottom of the last tab to the top of the content area) 
        // / (the total available content height)
        this.yDensity = (rects[rects.length - 1].bottom - bb.top) / (bb.height);

        // xDensity = (the distance from the left of the content area to the right of the rightmost
        // tab) / (the total available content width)
        
        // first, find the right of the rightmost tab! luckily, they're in order.
        // TODO: does this change for rtl?
        var rightMostRight = 0;
        for each (var rect in rects) {
          if (rect.right > rightMostRight)
            rightMostRight = rect.right;
          else
            break;
        }
        this.xDensity = (rightMostRight - bb.left) / (bb.width);
        
        this._children.forEach(function(child, index) {
          if (!child.locked.bounds) {
            child.setBounds(rects[index], !animate);
            child.setRotation(0);
            if (options.z)
              child.setZ(options.z);
          }
        });
        
        if (this.isNewTabsGroup()) {
          var box = rects[rects.length - 1];
          box.left -= this.bounds.left;
          box.top -= this.bounds.top;
          this.setNewTabButtonBounds(box, !animate);
        }
        
        this._isStacked = false;
      } else
        this._stackArrange(bb, options);
    }
    
    if ( this._isStacked && !this.expanded) this.showExpandControl();
    else this.hideExpandControl();
  },
  
  // ----------
  // Function: _stackArrange
  // Arranges the children in a stack. 
  // 
  // Parameters: 
  //   bb - <Rect> to arrange within
  //   options - see below
  //
  // Possible "options" properties: 
  //   animate - whether to animate; default: true.
  _stackArrange: function(bb, options) { 
    var animate;
    if (!options || typeof(options.animate) == 'undefined') 
      animate = true;
    else 
      animate = options.animate;

    if (typeof(options) == 'undefined')
      options = {};

    var count = this._children.length;
    if (!count)
      return;
    
    var zIndex = this.getZ() + count + 1;
    
    var Pi = Math.acos(-1);
    var maxRotation = 35; // degress
    var scale = 0.8;
    var newTabsPad = 10;
    var w;
    var h; 
    var itemAspect = TabItems.tabHeight / TabItems.tabWidth;
    var bbAspect = bb.height / bb.width;

    // compute h and w. h and w are the dimensions of each of the tabs... in other words, the
    // height and width of the entire stack, modulo rotation.
    if (bbAspect > itemAspect) { // Tall, thin group
      w = bb.width * scale;
      h = w * itemAspect;
      // let's say one, because, even though there's more space, we're enforcing that with scale.
      this.xDensity = 1;
      this.yDensity = h / (bb.height * scale);
    } else { // Short, wide group
      h = bb.height * scale;
      w = h * (1 / itemAspect);
      this.yDensity = 1;
      this.xDensity = h / (bb.width * scale);
    }
    
    // x is the left margin that the stack will have, within the content area (bb)
    // y is the vertical margin
    var x = (bb.width - w) / 2;
    if (this.isNewTabsGroup())
      x -= (w + newTabsPad) / 2;
      
    var y = Math.min(x, (bb.height - h) / 2);
    var box = new Rect(bb.left + x, bb.top + y, w, h);
    
    var self = this;
    var children = [];
    this._children.forEach(function(child) {
      if (child == self.topChild)
        children.unshift(child);
      else
        children.push(child);
    });
    
    children.forEach(function(child, index) {
      if (!child.locked.bounds) {
        child.setZ(zIndex);
        zIndex--;
        
        child.addClass("stacked");
        child.setBounds(box, !animate);
        child.setRotation(self._randRotate(maxRotation, index));
      }
    });
    
    if (this.isNewTabsGroup()) {
      box.left += box.width + newTabsPad;
      box.left -= this.bounds.left;
      box.top -= this.bounds.top;
      this.setNewTabButtonBounds(box, !animate);
    }

    self._isStacked = true;
  },

  // ----------
  // Function: _randRotate
  // Random rotation generator for <_stackArrange>
  _randRotate: function(spread, index){
    if ( index >= this._stackAngles.length ){
      var randAngle = 5*index + parseInt( (Math.random()-.5)*1 );
      this._stackAngles.push(randAngle);
      return randAngle;          
    }
    
    if ( index > 5 ) index = 5;

    return this._stackAngles[index];
  },

  // ----------
  // Function: childHit
  // Called by one of the group's children when the child is clicked on. 
  // 
  // Returns an object:
  //   shouldZoom - true if the browser should launch into the tab represented by the child
  //   callback - called after the zoom animation is complete
  childHit: function(child) {
    var self = this;
    
    // ___ normal click
    if (!this._isStacked || this.expanded) {
      return {
        shouldZoom: true,
        callback: function() {
          self.collapse();
        }
      };
    }

    // ___ we're stacked, but command isn't held down
    /*if (!Keys.meta) {
      Groups.setActiveGroup(self);
      return { shouldZoom: true };      
    }*/
        
    Groups.setActiveGroup(self);
    return { shouldZoom: true };
    
    /*this.expand();
    return {};*/
  },
  
  expand: function(){
    var self = this;
    // ___ we're stacked, and command is held down so expand
    Groups.setActiveGroup(self);
    var startBounds = this.getChild(0).getBounds();
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
          
    if ( pos.top < 0 )  pos.top = 20;
    if ( pos.left < 0 ) pos.left = 20;      
    if ( pos.top+overlayHeight > window.innerHeight ) pos.top = window.innerHeight-overlayHeight-20;
    if ( pos.left+overlayWidth > window.innerWidth )  pos.left = window.innerWidth-overlayWidth-20;
    
    $tray
      .animate({
        width:  overlayWidth,
        height: overlayHeight,
        top: pos.top,
        left: pos.left
      }, {
        duration: 200, 
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
      .click(function() { // just in case
        self.collapse();
      });

    // There is a race-condition here. If there is
    // a mouse-move while the shield is coming up
    // it will collapse, which we don't want. Thus,
    // we wait a little bit before adding this event
    // handler.
    setTimeout(function(){
      $shield.mouseover(function() {
        self.collapse();
      });
    }, 200);
      
    this.expanded = {
      $tray: $tray,
      $shield: $shield,
      bounds: new Rect(pos.left, pos.top, overlayWidth, overlayHeight)
    };
    
    this.arrange();    
  },

  // ----------
  // Function: collapse
  // Collapses the group from the expanded "tray" mode. 
  collapse: function() {
    if (this.expanded) {
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
  // Function: _addHandlers
  // Helper routine for the constructor; adds various event handlers to the container. 
  _addHandlers: function(container) {
    var self = this;
    
    this.dropOptions.over = function(){
      if ( !this.isNewTabsGroup() )
        iQ(this.container).addClass("acceptsDrop");
    };
    this.dropOptions.drop = function(event){
      iQ(this.container).removeClass("acceptsDrop");
      this.add( drag.info.$el, {left:event.pageX, top:event.pageY} );
    };
    
    if (!this.locked.bounds)
      this.draggable();
    
    iQ(container)
      .mousedown(function(e){        
        self._mouseDown = {
          location: new Point(e.clientX, e.clientY),
          className: e.target.className
        };
      })    
      .mouseup(function(e){
        if (!self._mouseDown || !self._mouseDown.location || !self._mouseDown.className)
          return;
          
        // Don't zoom in on clicks inside of the controls.
        var className = self._mouseDown.className;
        if (className.indexOf('title-shield') != -1
            || className.indexOf('name') != -1
            || className.indexOf('close') != -1
            || className.indexOf('newTabButton') != -1
            || className.indexOf('stackExpander') != -1 ) {
          return;
        }
        
        var location = new Point(e.clientX, e.clientY);
      
        if (location.distance(self._mouseDown.location) > 1.0) 
          return;
          
        // Don't zoom in to the last tab for the new tab group.
        if ( self.isNewTabsGroup() ) 
          return;
        
        // Zoom into the last-active tab when the group
        // is clicked, but only for non-stacked groups.
        var activeTab = self.getActiveTab();
        if( !self._isStacked ){
          if ( activeTab ) 
            activeTab.zoomIn();
          else if (self.getChild(0))
            self.getChild(0).zoomIn();          
        }
          
        self._mouseDown = null;
    });
    
    this.droppable(true);
    
    this.$expander.click(function(){
      self.expand();
    });
  },

  // ----------  
  // Function: setResizable
  // Sets whether the group is resizable and updates the UI accordingly.
  setResizable: function(value){
    this.resizeOptions.minWidth = 90;
    this.resizeOptions.minHeight = 90;

    if (value) {
      this.$resizer.fadeIn();
      this.resizable(true);
    } else {
      this.$resizer.fadeOut();
      this.resizable(false);
    }
  },
  
  // ----------
  // Function: newTab
  // Creates a new tab within this group.
  newTab: function(url) {
    Groups.setActiveGroup(this);          
    var newTab = Tabs.open(url || "about:blank", true);
    
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
  // shown in the tab bar. It does it by sorting the children
  // of the group by the positions of their respective tabs in the
  // tab bar.
  reorderBasedOnTabOrder: function(){    
    var groupTabs = [];
    for ( var i=0; i<UI.tabBar.el.children.length; i++ ){
      var tab = UI.tabBar.el.children[i];
      if (!tab.collapsed)
        groupTabs.push(tab);
    }
     
    this._children.sort(function(a,b){
      return groupTabs.indexOf(a.tab.raw) - groupTabs.indexOf(b.tab.raw)
    });
    
    this.arrange({animate: false});
    // this.arrange calls this.save for us
  },
  
  // ----------
  // Function: setTopChild
  // Sets the <Item> that should be displayed on top when in stack mode.
  setTopChild: function(topChild){    
    this.topChild = topChild;
    
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
    if ( index < 0 )
    	index = this._children.length + index;
    if ( index >= this._children.length || index < 0 )
    	return null;
    return this._children[index];
  },

  // ----------
  // Function: getChildren
  // Returns all children.
  getChildren: function(){
    return this._children;
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
// Class: Groups
// Singelton for managing all <Group>s. 
window.Groups = {
  
  // ----------
  // Function: init
  // Sets up the object.
  init: function() {
    this.groups = [];
    this.nextID = 1;
    this._inited = false;
  },
  
  // ----------
  // Function: getNextID
  // Returns the next unused group ID. 
  getNextID: function() {
    var result = this.nextID;
    this.nextID++;
    this.save();
    return result;
  },

  // ----------
  // Function: getStorageData
  // Returns an object for saving Groups state to persistent storage. 
  getStorageData: function() {
    var data = {nextID: this.nextID, groups: []};
    this.groups.forEach(function(group) {
      data.groups.push(group.getStorageData());
    });
    
    return data;
  },
  
  // ----------
  // Function: saveAll
  // Saves Groups state, as well as the state of all of the groups.
  saveAll: function() {
    this.save();
    this.groups.forEach(function(group) {
      group.save();
    });
  },
  
  // ----------
  // Function: save
  // Saves Groups state. 
  save: function() {
    if (!this._inited) // too soon to save now
      return;

    Storage.saveGroupsData(Utils.getCurrentWindow(), {nextID:this.nextID});
  },

  // ----------  
  // Function: getBoundingBox
  // Given an array of DOM elements, returns a <Rect> with (roughly) the union of their locations.
  getBoundingBox: function Groups_getBoundingBox(els) {
    var el, b;
    var bounds = [iQ(el).bounds() for each (el in els)];
    var left   = min( [ b.left   for each (b in bounds) ] );
    var top    = min( [ b.top    for each (b in bounds) ] );
    var right  = max( [ b.right  for each (b in bounds) ] );
    var bottom = max( [ b.bottom for each (b in bounds) ] );
    
    return new Rect(left, top, right-left, bottom-top);
  },

  // ----------
  // Function: reconstitute
  // Restores to stored state, creating groups as needed.
  // If no data, sets up blank slate (including "new tabs" group).
  reconstitute: function(groupsData, groupData) {
    try {
      if (groupsData && groupsData.nextID)
        this.nextID = groupsData.nextID;
        
      if (groupData) {
        for (var id in groupData) {
          var group = groupData[id];
          if (this.groupStorageSanity(group)) {
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
      if (!group) {
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
  // Function: groupStorageSanity
  // Given persistent storage data for a group, returns true if it appears to not be damaged.
  groupStorageSanity: function(groupData) {
    // TODO: check everything 
    var sane = true;
    if (!isRect(groupData.bounds)) {
      Utils.log('Groups.groupStorageSanity: bad bounds', groupData.bounds);
      sane = false;
    }
    
    return sane;
  },
  
  // ----------
  // Function: getGroupWithTitle
  // Returns the <Group> that has the given title, or null if none found.
  // TODO: what if there are multiple groups with the same title??
  //       Right now, looks like it'll return the last one.
  getGroupWithTitle: function(title) {
    var result = null;
    this.groups.forEach(function(group) {
      if (group.getTitle() == title) {
        result = group;
        return false;
      }
    });
    
    return result;
  }, 
 
  // ----------
  // Function: getNewTabGroup
  // Returns the "new tabs" <Group>, or null if not found.
  getNewTabGroup: function() {
    var groupTitle = 'New Tabs';
    var array = this.groups.filter(function(group) {
      return group.getTitle() == groupTitle;
    });
    
    if (array.length) 
      return array[0];
      
    return null;
  },

  // ----------
  // Function: getBoundsForNewTabGroup 
  // Returns a <Rect> describing where the "new tabs" group should go. 
  getBoundsForNewTabGroup: function() {
    var pad = 0;
    var sw = window.innerWidth;
    var sh = window.innerHeight;
    var w = sw - (pad * 2);
    var h = TabItems.tabHeight * 0.9 + pad*2;
    return new Rect(pad, sh - (h + pad), w, h);
  },

  // ----------
  // Function: repositionNewTabGroup
  // Moves the "new tabs" group to where it should be. 
  repositionNewTabGroup: function() {
    var box = this.getBoundsForNewTabGroup();
    var group = this.getNewTabGroup();
    group.setBounds(box, true);
  },
  
  // ---------- 
  // Function: register
  // Adds the given <Group> to the list of groups we're tracking. 
  register: function(group) {
    Utils.assert('group', group);
    Utils.assert('only register once per group', this.groups.indexOf(group) == -1);
    this.groups.push(group);
  },
  
  // ----------  
  // Function: unregister
  // Removes the given <Group> from the list of groups we're tracking.
  unregister: function(group) {
    var index = this.groups.indexOf(group);
    if (index != -1)
      this.groups.splice(index, 1);  
    
    if (group == this._activeGroup)
      this._activeGroup = null;   
  },
  
  // ----------
  // Function: group
  // Given some sort of identifier, returns the appropriate group.
  // Currently only supports group ids. 
  group: function(a) {
    var result = null;
    this.groups.forEach(function(candidate) {
      if (candidate.id == a) {
        result = candidate;
        return false;
      }
    });
    
    return result;
  },
  
  // ----------  
  // Function: arrange
  // Arranges all of the groups into a grid. 
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
    this.groups.forEach(function(group) {
      if (group.locked.bounds)
        return; 
        
      group.setBounds(box, true);
      
      box.left += box.width + padding;
      i++;
      if (i % columns == 0) {
        box.left = startX;
        box.top += box.height + padding;
      }
    });
  },
  
  // ----------
  // Function: removeAll
  // Removes all tabs from all groups (which automatically closes all unnamed groups).
  removeAll: function() {
    var toRemove = iQ.merge([], this.groups);
    toRemove.forEach(function(group) {
      group.removeAll();
    });
  },
  
  // ----------
  // Function: newTab
  // Given a <TabItem>, files it in the appropriate group. 
  newTab: function(tabItem) {
    var group = this.getActiveGroup();
    if ( group == null )
      group = this.getNewTabGroup();
    
    var $el = iQ(tabItem.container);
    if (group) group.add($el);
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
  // Paramaters:
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
    if (!window.UI)
      return; // called too soon
      
    if (this._activeGroup)
      UI.tabBar.showOnlyTheseTabs( this._activeGroup._children );
    else if ( this._activeGroup == null)
      UI.tabBar.showOnlyTheseTabs( this.getOrphanedTabs(), {dontReorg: true});
  },
  
  // ----------
  // Function: getOrphanedTabs
  // Returns an array of all tabs that aren't in a group.
  getOrphanedTabs: function(){
    var tabs = TabItems.getItems();
    tabs = tabs.filter(function(tab){
      return tab.parent == null;
    });
    return tabs;
  }
  
};

// ----------
Groups.init();

})();
