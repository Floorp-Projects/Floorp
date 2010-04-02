// ##########
// An Item is an object that adheres to an interface consisting of these methods: 
//   getContainer: function() 
//   getBounds: function() 
//   setBounds: function(rect, immediately)
//   setPosition: function(left, top, immediately) 
//   setSize: function(width, height, immediately)
//   close: function() 
//   addOnClose: function(referenceObject, callback)
//   removeOnClose: function(referenceObject)
// 
// In addition, it must have these properties:
//   isAnItem, set to true (set by Item)
//   defaultSize, a Point
//    
// Its container must also have a jQuery data named 'item' that points to the item.

window.Item = function() {
  this.isAnItem = true;
};

window.Item.prototype = { 
  // ----------  
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
  }
};  

// ##########
window.Items = {
  // ----------  
  item: function(el) {
    return $(el).data('item');
  },
  
  // ----------  
  getTopLevelItems: function() {
    var items = [];
    
    $('.tab, .group').each(function() {
      $this = $(this);
      if(!$this.data('group'))
        items.push($this.data('item'));
    });
    
    return items;
  }
};

