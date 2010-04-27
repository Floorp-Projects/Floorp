// Title: items.js (revision-a)
// ##########
// An Item is an object that adheres to an interface consisting of these methods: 
//   reloadBounds: function() 
//   getBounds: function(), inherited from Item 
//   setBounds: function(rect, immediately)
//   setPosition: function(left, top, immediately), inherited from Item
//   setSize: function(width, height, immediately), inherited from Item
//   getZ: function(), inherited from Item  
//   setZ: function(value)
//   close: function() 
//   addOnClose: function(referenceObject, callback)
//   removeOnClose: function(referenceObject)
// 
// In addition, it must have these properties:
//   isAnItem, set to true (set by Item)
//   defaultSize, a Point
//   bounds, a Rect (set by Item in _init() via reloadBounds())
//   debug (set by Item)
//   $debug (set by Item in _init())
//   container, a DOM element (set by Item in _init())
//    
// Its container must also have a jQuery data named 'item' that points to the item. 
// This is set by Item in _init(). 


// Class: Item
// Superclass for all visible objects (tabs and groups).
window.Item = function() {
  // Variable: isAnItem
  // Always true for Items
  this.isAnItem = true;
  
  // Variable: bounds
  // The position and size of this Item, represented as a <Rect>. 
  this.bounds = null;
  
  // Variable: debug
  // When set to true, displays a rectangle on the screen that corresponds with bounds.
  // May be used for additional debugging features in the future.
  this.debug = false;
  
  // Variable: $debug
  // If <debug> is true, this will be the jQuery object for the visible rectangle. 
  this.$debug = null;
  
  // Variable: container
  // The outermost DOM element that describes this item on screen.
  this.container = null;
  
  // Variable: locked
  // Affects whether an item can be pushed, closed, renamed, etc
  this.locked = false;
  
  // Variable: parent
  // The group that this item is a child of
  this.parent = null;
  
  // Variable: userSize
  // A <Point> that describes the last size specifically chosen by the user.
  // Used by unsquish.
  this.userSize = null;
};

window.Item.prototype = { 
  // ----------  
  // Function: _init
  // Initializes the object. To be called from the subclass's intialization function. 
  //
  // Parameters: 
  //   container - the outermost DOM element that describes this item onscreen. 
  _init: function(container) {
    this.container = container;
    
    if(this.debug) {
      this.$debug = $('<div />')
        .css({
          border: '2px solid green',
          zIndex: -10,
          position: 'absolute'
        })
        .appendTo($('body'));
    }
    
    this.reloadBounds();        
    $(this.container).data('item', this);
  },
  
  // ----------
  // Function: getBounds
  // Returns a copy of the Item's bounds as a <Rect>.
  getBounds: function() {
    return new Rect(this.bounds);    
  },
  
  // ----------
  // Function: setPosition
  // Moves the Item to the specified location. 
  // 
  // Parameters: 
  //   left - the new left coordinate relative to the window
  //   top - the new top coordinate relative to the window
  //   immediately - if false or omitted, animates to the new position;
  //   otherwise goes there immediately
  setPosition: function(left, top, immediately) {
    this.setBounds(new Rect(left, top, this.bounds.width, this.bounds.height), immediately);
  },

  // ----------  
  // Function: setSize
  // Resizes the Item to the specified size. 
  // 
  // Parameters: 
  //   width - the new width in pixels
  //   height - the new height in pixels
  //   immediately - if false or omitted, animates to the new size;
  //   otherwise resizes immediately
  setSize: function(width, height, immediately) {
    this.setBounds(new Rect(this.bounds.left, this.bounds.top, width, height), immediately);
  },

  // ----------
  // Function: setUserSize
  // Remembers the current size as one the user has chosen. 
  setUserSize: function() {
    this.userSize = new Point(this.bounds.width, this.bounds.height);
  },
  
  // ----------
  // Function: getZ
  // Returns the zIndex of the Item.
  getZ: function() {
    return parseInt($(this.container).css('zIndex'));
  },

  // ----------
  // Function: setRotation
  // Rotates the object to the given number of degrees.
  setRotation: function(degrees) {
    var value = "rotate(%deg)".replace(/%/, degrees);
    $(this.container).css({"-moz-transform": value});
  },
    
  // ----------  
  // Function: pushAway
  // Pushes all other items away so none overlap this Item.
  pushAway: function() {
    var buffer = 10;
    
    var items = Items.getTopLevelItems();
    $.each(items, function(index, item) {
      var data = {};
      data.bounds = item.getBounds();
      data.startBounds = new Rect(data.bounds);
      data.generation = Infinity;
      item.pushAwayData = data;
    });
    
    var itemsToPush = [this];
    this.pushAwayData.generation = 0;

    var pushOne = function(baseItem) {
      var baseData = baseItem.pushAwayData;
      var bb = new Rect(baseData.bounds);
      bb.inset(-buffer, -buffer);
      var bbc = bb.center();
    
      $.each(items, function(index, item) {
        if(item == baseItem || item.locked)
          return;
          
        var data = item.pushAwayData;
        if(data.generation <= baseData.generation)
          return;
          
        var bounds = data.bounds;
        var box = new Rect(bounds);
        box.inset(-buffer, -buffer);
        if(box.intersects(bb)) {
          var offset = new Point();
          var center = box.center(); 
          if(Math.abs(center.x - bbc.x) < Math.abs(center.y - bbc.y)) {
/*             offset.x = Math.floor((Math.random() * 10) - 5); */
            if(center.y > bbc.y)
              offset.y = bb.bottom - box.top; 
            else
              offset.y = bb.top - box.bottom;
          } else {
/*             offset.y = Math.floor((Math.random() * 10) - 5); */
            if(center.x > bbc.x)
              offset.x = bb.right - box.left; 
            else
              offset.x = bb.left - box.right;
          }
          
          bounds.offset(offset); 
          data.generation = baseData.generation + 1;
          data.pusher = baseItem;
          itemsToPush.push(item);
        }
      });
    };   
    
    while(itemsToPush.length)
      pushOne(itemsToPush.shift());         

    // ___ Squish!
    var pageBounds = Items.getPageBounds();
    if(Items.squishMode == 'squish') {
      $.each(items, function(index, item) {
        var data = item.pushAwayData;
        if(data.generation == 0 || item.locked)
          return;
  
        function apply(item, posStep, posStep2, sizeStep) {
          var data = item.pushAwayData;
          if(data.generation == 0)
            return;
            
          var bounds = data.bounds;
          bounds.width -= sizeStep.x; 
          bounds.height -= sizeStep.y;
          bounds.left += posStep.x;
          bounds.top += posStep.y;
          
          if(!item.isAGroup) {
            if(sizeStep.y > sizeStep.x) {
              var newWidth = bounds.height * (TabItems.tabWidth / TabItems.tabHeight);
              bounds.left += (bounds.width - newWidth) / 2;
              bounds.width = newWidth;
            } else {
              var newHeight = bounds.width * (TabItems.tabHeight / TabItems.tabWidth);
              bounds.top += (bounds.height - newHeight) / 2;
              bounds.height = newHeight;
            }
          }
          
          var pusher = data.pusher;
          if(pusher)  
            apply(pusher, posStep.plus(posStep2), posStep2, sizeStep);
        }
  
        var bounds = data.bounds;
        var posStep = new Point();
        var posStep2 = new Point();
        var sizeStep = new Point();

        if(bounds.left < pageBounds.left) {      
          posStep.x = pageBounds.left - bounds.left;
          sizeStep.x = posStep.x / data.generation;
          posStep2.x = -sizeStep.x;                
        } else if(bounds.right > pageBounds.right) {      
          posStep.x = pageBounds.right - bounds.right;
          sizeStep.x = -posStep.x / data.generation;
          posStep.x += sizeStep.x;
          posStep2.x = sizeStep.x;
        }

        if(bounds.top < pageBounds.top) {      
          posStep.y = pageBounds.top - bounds.top;
          sizeStep.y = posStep.y / data.generation;
          posStep2.y = -sizeStep.y;                
        } else if(bounds.bottom > pageBounds.bottom) {      
          posStep.y = pageBounds.bottom - bounds.bottom;
          sizeStep.y = -posStep.y / data.generation;
          posStep.y += sizeStep.y;
          posStep2.y = sizeStep.y;
        }
  
        if(posStep.x || posStep.y || sizeStep.x || sizeStep.y) 
          apply(item, posStep, posStep2, sizeStep);
      });
    } else if(Items.squishMode == 'all') {
      var newPageBounds = null;
      $.each(items, function(index, item) {
        if(item.locked)
          return;
          
        var data = item.pushAwayData;
        var bounds = data.bounds;
        newPageBounds = (newPageBounds ? newPageBounds.union(bounds) : new Rect(bounds));
      });
      
      var wScale = pageBounds.width / newPageBounds.width;
      var hScale = pageBounds.height / newPageBounds.height;
      var scale = Math.min(hScale, wScale);
      $.each(items, function(index, item) {
        if(item.locked)
          return;
          
        var data = item.pushAwayData;
        var bounds = data.bounds;

        bounds.left -= newPageBounds.left;
        bounds.left *= scale;
        bounds.width *= scale;

        bounds.top -= newPageBounds.top;            
        bounds.top *= scale;
        bounds.height *= scale;
      });
    }

    // ___ Unsquish
    $.each(items, function(index, item) {
      if(item.locked)
        return;
        
      var data = item.pushAwayData;
      var bounds = data.bounds;
      var newBounds = new Rect(bounds);

      var newSize;
      if(item.userSize) 
        newSize = new Point(item.userSize);
      else
        newSize = new Point(TabItems.tabWidth, TabItems.tabHeight);
        
      if(item.isAGroup) {
          newBounds.width = Math.max(newBounds.width, newSize.x);
          newBounds.height = Math.max(newBounds.height, newSize.y);
      } else {
        if(bounds.width < newSize.x) {
          newBounds.width = newSize.x;
          newBounds.height = newSize.y;
        }
      }

      newBounds.left -= (newBounds.width - bounds.width) / 2;
      newBounds.top -= (newBounds.height - bounds.height) / 2;
      
      var offset = new Point();
      if(newBounds.left < pageBounds.left)
        offset.x = pageBounds.left - newBounds.left;
      else if(newBounds.right > pageBounds.right)
        offset.x = pageBounds.right - newBounds.right;

      if(newBounds.top < pageBounds.top)
        offset.y = pageBounds.top - newBounds.top;
      else if(newBounds.bottom > pageBounds.bottom)
        offset.y = pageBounds.bottom - newBounds.bottom;
        
      newBounds.offset(offset);

      if(!bounds.equals(newBounds)) {        
        var blocked = false;
        $.each(items, function(index, item2) {
          if(item2 == item)
            return;
            
          var data2 = item2.pushAwayData;
          var bounds2 = data2.bounds;
          if(bounds2.intersects(newBounds)) {
            blocked = true;
            return false;
          }
        });
        
        if(!blocked) {
          data.bounds = newBounds;
        }
      }
    });

    // ___ Apply changes
    $.each(items, function(index, item) {
      var data = item.pushAwayData;
      var bounds = data.bounds;
      if(!bounds.equals(data.startBounds)) {
        item.setBounds(bounds);
      }
    });
  },
  
  // ----------  
  // Function: _updateDebugBounds
  // Called by a subclass when its bounds change, to update the debugging rectangles on screen.
  // This functionality is enabled only by the debug property.
  _updateDebugBounds: function() {
    if(this.$debug) {
      this.$debug.css({
        left: this.bounds.left,
        top: this.bounds.top,
        width: this.bounds.width,
        height: this.bounds.height
      });
    }
  }  
};  

// ##########
// Class: Items
// Keeps track of all Items. 
window.Items = {
  // ----------
  // Variable: squishMode
  // How to deal when things go off the edge.
  // Options include: all, squish, push
  squishMode: 'squish', 
  
  // ----------
  // Function: init
  // Initialize the object
  init: function() {
  },
  
  // ----------  
  // Function: item
  // Given a DOM element representing an Item, returns the Item. 
  item: function(el) {
    return $(el).data('item');
  },
  
  // ----------  
  // Function: getTopLevelItems
  // Returns an array of all Items not grouped into groups. 
  getTopLevelItems: function() {
    var items = [];
    
    $('.tab, .group').each(function() {
      $this = $(this);
      var item = $this.data('item');  
      if(!item.parent && !$this.hasClass('phantom'))
        items.push(item);
    });
    
    return items;
  }, 

  // ----------
  // Function: getPageBounds
  // Returns a <Rect> defining the area of the page <Item>s should stay within. 
  getPageBounds: function() {
    var top = 20;
    var bottom = TabItems.tabHeight + 10; // MAGIC NUMBER: giving room for the "new tabs" group
    var width = Math.max(100, window.innerWidth);
    var height = Math.max(100, window.innerHeight - (top + bottom));
    return new Rect(0, top, width, height);
  },
  
  // ----------  
  // Function: arrange
  // Arranges the given items in a grid within the given bounds, 
  // maximizing item size but maintaining standard tab aspect ratio for each
  // 
  // Parameters: 
  //   items - an array of <Item>s
  //   bounds - a <Rect> defining the space to arrange within
  //   options - an object with options. If options.animate is false, doesn't animate, otherwise it does.
  arrange: function(items, bounds, options) {
    var animate;
    if(!options || typeof(options.animate) == 'undefined') 
      animate = true;
    else 
      animate = options.animate;

    if(typeof(options) == 'undefined')
      options = {};
    
    var tabAspect = TabItems.tabHeight / TabItems.tabWidth;
    var count = items.length;
    if(!count)
      return;
      
    var columns = 1;
    var padding = options.padding || 0;
    var yScale = 1.1; // to allow for titles
    var rows;
    var tabWidth;
    var tabHeight;
    var totalHeight;

    function figure() {
      rows = Math.ceil(count / columns);          
      tabWidth = (bounds.width - (padding * (columns - 1))) / columns;
      tabHeight = tabWidth * tabAspect; 
      totalHeight = (tabHeight * yScale * rows) + (padding * (rows - 1)); 
    } 
    
    figure();
    
    while(rows > 1 && totalHeight > bounds.height) {
      columns++; 
      figure();
    }
    
    if(rows == 1) {
      tabWidth = Math.min(bounds.width / Math.max(2, count), bounds.height / tabAspect);
      tabHeight = tabWidth * tabAspect;
    }
    
    var box = new Rect(bounds.left, bounds.top, tabWidth, tabHeight);
    var row = 0;
    var column = 0;
    var immediately;
    
    $.each(items, function(index, item) {
/*
      if(animate == 'sometimes')
        immediately = (typeof(item.groupData.row) == 'undefined' || item.groupData.row == row);
      else
*/
        immediately = !animate;
        
      item.setBounds(box, immediately);
      item.setRotation(0);
/*
      item.groupData.column = column;
      item.groupData.row = row;
*/
      
      box.left += box.width + padding;
      column++;
      if(column == columns) {
        box.left = bounds.left;
        box.top += (box.height * yScale) + padding;
        column = 0;
        row++;
      }
    });
  }
};

window.Items.init();

