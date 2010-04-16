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
  // Function: getZ
  // Returns the zIndex of the Item.
  getZ: function() {
    return parseInt($(this.container).css('zIndex'));
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
        if(item == baseItem)
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
            if(center.y > bbc.y)
              offset.y = bb.bottom - box.top; 
            else
              offset.y = bb.top - box.bottom;
          } else {
            if(center.x > bbc.x)
              offset.x = bb.right - box.left; 
            else
              offset.x = bb.left - box.right;
          }
          
          bounds.offset(offset); 
          data.generation = baseData.generation + 1;
          itemsToPush.push(item);
        }
      });
    };   
    
    while(itemsToPush.length)
      pushOne(itemsToPush.shift());         

    $.each(items, function(index, item) {
      var data = item.pushAwayData;
      if(!data.bounds.equals(data.startBounds))
        item.setPosition(data.bounds.left, data.bounds.top);
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
      if(!$this.data('group'))
        items.push($this.data('item'));
    });
    
    return items;
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
      tabWidth = Math.min(bounds.width / count, bounds.height / tabAspect);
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

