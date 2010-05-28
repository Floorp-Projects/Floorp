// Title: items.js (revision-a)

// ##########
// Class: Item
// Superclass for all visible objects (<TabItem>s and <Group>s).
//
// If you subclass, in addition to the things Item provides, you need to also provide these methods: 
//   reloadBounds - function() 
//   setBounds - function(rect, immediately)
//   setZ - function(value)
//   close - function() 
//   addOnClose - function(referenceObject, callback)
//   removeOnClose - function(referenceObject)
//   save - function()
//
// ... and this property: 
//   defaultSize - a Point
//   locked - an object (see below)
//
// Make sure to call _init() from your subclass's constructor. 
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
  // If <debug> is true, this will be the iQ object for the visible rectangle. 
  this.$debug = null;
  
  // Variable: container
  // The outermost DOM element that describes this item on screen.
  this.container = null;
  
  // Variable: locked
  // Affects whether an item can be pushed, closed, renamed, etc
  //
  // The object may have properties to specify what can't be changed: 
  //   .bounds - true if it can't be pushed, dragged, resized, etc
  //   .close - true if it can't be closed
  //   .title - true if it can't be renamed
  this.locked = null;
  
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
    Utils.assert('container must be a DOM element', Utils.isDOMElement(container));
    Utils.assert('Subclass must provide reloadBounds', typeof(this.reloadBounds) == 'function');
    Utils.assert('Subclass must provide setBounds', typeof(this.setBounds) == 'function');
    Utils.assert('Subclass must provide setZ', typeof(this.setZ) == 'function');
    Utils.assert('Subclass must provide close', typeof(this.close) == 'function');
    Utils.assert('Subclass must provide addOnClose', typeof(this.addOnClose) == 'function');
    Utils.assert('Subclass must provide removeOnClose', typeof(this.removeOnClose) == 'function');
    Utils.assert('Subclass must provide save', typeof(this.save) == 'function');
    Utils.assert('Subclass must provide defaultSize', isPoint(this.defaultSize));
    Utils.assert('Subclass must provide locked', this.locked);
    
    this.container = container;
    
    if(this.debug) {
      this.$debug = iQ('<div>')
        .css({
          border: '2px solid green',
          zIndex: -10,
          position: 'absolute'
        })
        .appendTo('body');
    }
    
    this.reloadBounds();        
    Utils.assert('reloadBounds must set up this.bounds', this.bounds);

    iQ(this.container).data('item', this);
  },
  
  // ----------
  // Function: getBounds
  // Returns a copy of the Item's bounds as a <Rect>.
  getBounds: function() {
    Utils.assert('this.bounds', isRect(this.bounds));
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
    Utils.assert('this.bounds', isRect(this.bounds));
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
    Utils.assert('this.bounds', isRect(this.bounds));
    this.setBounds(new Rect(this.bounds.left, this.bounds.top, width, height), immediately);
  },

  // ----------
  // Function: setUserSize
  // Remembers the current size as one the user has chosen. 
  setUserSize: function() {
    Utils.assert('this.bounds', isRect(this.bounds));
    this.userSize = new Point(this.bounds.width, this.bounds.height);
    this.save();
  },
  
  // ----------
  // Function: getZ
  // Returns the zIndex of the Item.
  getZ: function() {
    return parseInt(iQ(this.container).css('zIndex'));
  },

  // ----------
  // Function: setRotation
  // Rotates the object to the given number of degrees.
  setRotation: function(degrees) {
    var value = "rotate(%deg)".replace(/%/, degrees);
    iQ(this.container).css({"-moz-transform": value});
  },
    
  // ----------
  // Function: setParent
  //
  setParent: function(parent) {
    this.parent = parent;
    this.save();
  },

  // ----------  
  // Function: pushAway
  // Pushes all other items away so none overlap this Item.
  pushAway: function() {
    var buffer = 2;
    
    var items = Items.getTopLevelItems();
    iQ.each(items, function(index, item) {
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
    
      iQ.each(items, function(index, item) {
        if(item == baseItem || item.locked.bounds)
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
      iQ.each(items, function(index, item) {
        var data = item.pushAwayData;
        if(data.generation == 0 || item.locked.bounds)
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
      iQ.each(items, function(index, item) {
        if(item.locked.bounds)
          return;
          
        var data = item.pushAwayData;
        var bounds = data.bounds;
        newPageBounds = (newPageBounds ? newPageBounds.union(bounds) : new Rect(bounds));
      });
      
      var wScale = pageBounds.width / newPageBounds.width;
      var hScale = pageBounds.height / newPageBounds.height;
      var scale = Math.min(hScale, wScale);
      iQ.each(items, function(index, item) {
        if(item.locked.bounds)
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
    var pairs = [];
    iQ.each(items, function(index, item) {
      var data = item.pushAwayData;
      pairs.push({
        item: item,
        bounds: data.bounds
      });
    });
    
    Items.unsquish(pairs);

    // ___ Apply changes
    iQ.each(items, function(index, item) {
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
    return iQ(el).data('item');
  },
  
  // ----------  
  // Function: getTopLevelItems
  // Returns an array of all Items not grouped into groups. 
  getTopLevelItems: function() {
    var items = [];
    
    iQ('.tab, .group').each(function() {
      var $this = iQ(this);
      var item = $this.data('item');  
      if(item && !item.parent && !$this.hasClass('phantom'))
        items.push(item);
    });
    
    return items;
  }, 

  // ----------
  // Function: getPageBounds
  // Returns a <Rect> defining the area of the page <Item>s should stay within. 
  getPageBounds: function() {
    var top = 0;
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
  //   items - an array of <Item>s. Can be null if the pretend and count options are set.
  //   bounds - a <Rect> defining the space to arrange within
  //   options - an object with various properites (see below)
  //
  // Possible "options" properties: 
  //   animate - whether to animate; default: true.
  //   z - the z index to set all the items; default: don't change z.
  //   pretend - whether to collect and return the rectangle rather than moving the items; default: false
  //   count - overrides the item count for layout purposes; default: the actual item count
  //   padding - pixels between each item
  //     
  // Returns: 
  //   the list of rectangles if the pretend option is set; otherwise null
  arrange: function(items, bounds, options) {
    var animate;
    if(!options || typeof(options.animate) == 'undefined') 
      animate = true;
    else 
      animate = options.animate;

    if(typeof(options) == 'undefined')
      options = {};
    
    var rects = null;
    if(options.pretend)
      rects = [];
      
    var tabAspect = TabItems.tabHeight / TabItems.tabWidth;
    var count = options.count || (items ? items.length : 0);
    if(!count)
      return rects;
      
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
      tabWidth = Math.min(Math.min(TabItems.tabWidth, bounds.width / count), bounds.height / tabAspect);
      tabHeight = tabWidth * tabAspect;
    }
    
    var box = new Rect(bounds.left, bounds.top, tabWidth, tabHeight);
    var row = 0;
    var column = 0;
    var immediately;
    
    var a;
    for(a = 0; a < count; a++) {
/*
      if(animate == 'sometimes')
        immediately = (typeof(item.groupData.row) == 'undefined' || item.groupData.row == row);
      else
*/
        immediately = !animate;
        
      if(rects)
        rects.push(new Rect(box));
      else if(items && a < items.length) {
        var item = items[a];
        if(!item.locked.bounds) {
          item.setBounds(box, immediately);
          item.setRotation(0);
          if(options.z)
            item.setZ(options.z);
        }
      }
      
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
    }
    
    return rects;
  },
  
  // ----------
  // Function: unsquish
  // Checks to see which items can now be unsquished. 
  //
  // Parameters: 
  //   pairs - an array of objects, each with two properties: item and bounds. The bounds are 
  //     modified as appropriate, but the items are not changed. If pairs is null, the
  //     operation is performed directly on all of the top level items. 
  //   ignore - an <Item> to not include in calculations (because it's about to be closed, for instance)
  unsquish: function(pairs, ignore) {
    var pairsProvided = (pairs ? true : false);
    if(!pairsProvided) {
      var items = Items.getTopLevelItems();
      pairs = [];
      iQ.each(items, function(index, item) {
        pairs.push({
          item: item,
          bounds: item.getBounds()
        });
      });
    }
  
    var pageBounds = Items.getPageBounds();
    iQ.each(pairs, function(index, pair) {
      var item = pair.item;
      if(item.locked.bounds || item == ignore)
        return;
        
      var bounds = pair.bounds;
      var newBounds = new Rect(bounds);

      var newSize;
      if(isPoint(item.userSize)) 
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
        iQ.each(pairs, function(index, pair2) {
          if(pair2 == pair || pair2.item == ignore)
            return;
            
          var bounds2 = pair2.bounds;
          if(bounds2.intersects(newBounds)) {
            blocked = true;
            return false;
          }
        });
        
        if(!blocked) {
          pair.bounds.copy(newBounds);
        }
      }
    });

    if(!pairsProvided) {
      iQ.each(pairs, function(index, pair) {
        pair.item.setBounds(pair.bounds);
      });
    }
  }
};

window.Items.init();

