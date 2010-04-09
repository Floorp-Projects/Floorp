(function(){

var numCmp = function(a,b){ return a-b; }

function min(list){ return list.slice().sort(numCmp)[0]; }
function max(list){ return list.slice().sort(numCmp).reverse()[0]; }

function isEventOverElement(event, el){
  var hit = {nodeName: null};
  var isOver = false;
  
  var hiddenEls = [];
  while(hit.nodeName != "BODY" && hit.nodeName != "HTML"){
    hit = document.elementFromPoint(event.clientX, event.clientY);
    if( hit == el ){
      isOver = true;
      break;
    }
    $(hit).hide();
    hiddenEls.push(hit);
  }
  
  var hidden;
  [$(hidden).show() for([,hidden] in Iterator(hiddenEls))];
  return isOver;
}

// ##########
window.Group = function(listOfEls, options) {
  if(typeof(options) == 'undefined')
    options = {};

  this._children = []; // an array of Items
  this._padding = 30;
  this.defaultSize = new Point(TabItems.tabWidth * 1.5, TabItems.tabHeight * 1.5);

  var self = this;

  var boundingBox = this._getBoundingBox(listOfEls);
  var padding = 30;
  var rectToBe = new Rect(
    boundingBox.left-padding,
    boundingBox.top-padding,
    boundingBox.width+padding*2,
    boundingBox.height+padding*2
  );

  var $container = options.container; 
  if(!$container) {
    $container = $('<div class="group" />')
      .css({position: 'absolute'})
      .css(rectToBe);
  }
  
  $container
    .css({zIndex: -100})
    .appendTo("body")
    .dequeue();    
    
  // ___ Resizer
  this.$resizer = $("<div class='resizer'/>")
    .css({
      position: "absolute",
      width: 16, height: 16,
      bottom: 0, right: 0,
    })
    .appendTo($container)
    .hide();

  // ___ Titlebar
    this.$titlebar = $("<div class='titlebar'><input class='name' value=''/><div class='close'>x</div></div>")
    .appendTo($container)
  
  this.$titlebar.css({
      position: "absolute",
    });
    
  $('.close', this.$titlebar).click(function() {
    self.close();
  });

  // On delay, show the title bar.
  var shouldShow = false;
  $container.mouseover(function(){
    shouldShow = true;
    setTimeout(function(){
      if( shouldShow == false ) return;
      $container.find("input").focus();
      self.$titlebar
        .css({width: $container.width()})
        .animate({ opacity: 1}).dequeue();        
    }, 500);
  }).mouseout(function(e){
    shouldShow = false;
    if( isEventOverElement(e, $container.get(0) )) return;
    self.$titlebar.animate({opacity:0}).dequeue();
  })

  // ___ Content
  this.$content = $('<div class="group-content"/>')
    .css({
      left: 0,
      top: this.$titlebar.height(),
      position: 'absolute'
    })
    .appendTo($container);
  
  // ___ Superclass initialization
  this._init($container.get(0));

  if(this.$debug) 
    this.$debug.css({zIndex: -1000});
  
  // ___ Children
  $.each(listOfEls, function(index, el) {  
    self.add(el, null, options);
  });

  // ___ Finish Up
  this._addHandlers($container);
  this.setResizable(true);
  
  Groups.register(this);
  
  this.setBounds(rectToBe);
  
  // ___ Push other objects away
  if(!options.dontPush)
    this.pushAway();   
};

// ----------
window.Group.prototype = $.extend(new Item(), new Subscribable(), {  
  // ----------  
  _getBoundingBox: function(els) {
    var el;
    var boundingBox = {
      top:    min( [$(el).position().top  for([,el] in Iterator(els))] ),
      left:   min( [$(el).position().left for([,el] in Iterator(els))] ),
      bottom: max( [$(el).position().top  for([,el] in Iterator(els))] )  + $(els[0]).height(),
      right:  max( [$(el).position().left for([,el] in Iterator(els))] ) + $(els[0]).width(),
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
    var titleHeight = this.$titlebar.height();
    
    // ___ Determine what has changed
    var css = {};
    var titlebarCSS = {};
    var contentCSS = {};

    if(rect.left != this.bounds.left)
      css.left = rect.left;
      
    if(rect.top != this.bounds.top) 
      css.top = rect.top;
      
    if(rect.width != this.bounds.width) {
      css.width = rect.width;
      titlebarCSS.width = rect.width;
      contentCSS.width = rect.width;
    }

    if(rect.height != this.bounds.height) {
      css.height = rect.height; 
      contentCSS.height = rect.height - titleHeight; 
    }
      
    if($.isEmptyObject(css))
      return;
      
    var offset = new Point(rect.left - this.bounds.left, rect.top - this.bounds.top);
    this.bounds = new Rect(rect);

    // ___ Deal with children
    if(this._children.length) {
      if(css.width || css.height) {
        this.arrange({animate: !immediately});
      } else if(css.left || css.top) {
        $.each(this._children, function(index, child) {
          var box = child.getBounds();
          child.setPosition(box.left + offset.x, box.top + offset.y, immediately);
        });
      }
    }
          
    // ___ Update our representation
    if(immediately) {
      $(this.container).css(css);
      this.$titlebar.css(titlebarCSS);
      this.$content.css(contentCSS);
    } else {
      TabMirror.pausePainting();
      $(this.container).animate(css, {complete: function() {
        TabMirror.resumePainting();
      }}).dequeue();
      
      this.$titlebar.animate(titlebarCSS).dequeue();        
      this.$content.animate(contentCSS).dequeue();        
    }

    this._updateDebugBounds();
  },
  
  // ----------
  setZ: function(value) {
    $(this.container).css({zIndex: value});
    $.each(this._children, function(index, child) {
      child.setZ(value + 1);
    });
  },
    
  // ----------  
  close: function() {
    var toClose = $.merge([], this._children);
    $.each(toClose, function(index, child) {
      child.close();
    });
  },
  
  // ----------  
  add: function($el, dropPos, options) {
    Utils.assert('add expects jQuery objects', Utils.isJQuery($el));
    
    if(!dropPos) 
      dropPos = {top:window.innerWidth, left:window.innerHeight};
      
    if(typeof(options) == 'undefined')
      options = {};
      
    var self = this;
    
    // TODO: You should be allowed to drop in the white space at the bottom and have it go to the end
    // (right now it can match the thumbnail above it and go there)
    function findInsertionPoint(dropPos){
      var best = {dist: Infinity, item: null};
      var index = 0;
      var box;
      for each(var child in self._children){
        box = child.getBounds();
        var dist = Math.sqrt( Math.pow((box.top+box.height/2)-dropPos.top,2) + Math.pow((box.left+box.width/2)-dropPos.left,2) );
/*         Utils.log( index, dist ); */
        if( dist <= best.dist ){
          best.item = child;
          best.dist = dist;
          best.index = index;
        }
        index += 1;
      }

      if( self._children.length > 0 ){
        box = best.item.getBounds();
        var insertLeft = dropPos.left <= box.left + box.width/2;
        if( !insertLeft ) 
          return best.index+1;
        else 
          return best.index;
      }
      
      return 0;      
    }
    
    var item = Items.item($el);
    var oldIndex = $.inArray(item, this._children);
    if(oldIndex != -1)
      this._children.splice(oldIndex, 1); 

    var index = findInsertionPoint(dropPos);
    this._children.splice( index, 0, item );

    $el.droppable("disable");
    item.setZ(this.getZ() + 1);

    item.addOnClose(this, function() {
      self.remove($el);
    });
    
    $el.data("group", this)
       .addClass("tabInGroup");
    
    if(typeof(item.setResizable) == 'function')
      item.setResizable(false);
    
    if(!options.dontArrange)
      this.arrange();
  },
  
  // ----------  
  // The argument a can be an Item, a DOM element, or a jQuery object
  remove: function(a, options) {
    var $el;  
    var item;

    if(a.isAnItem) {
      item = a;
      $el = $(item.container);
    } else {
      $el = $(a);  
      item = Items.item($el);
    }
    
    if(typeof(options) == 'undefined')
      options = {};
    
    var index = $.inArray(item, this._children);
    if(index != -1)
      this._children.splice(index, 1); 

    $el.data("group", null);
    item.setSize(item.defaultSize.x, item.defaultSize.y);
    $el.droppable("enable");    
    item.removeOnClose(this);
    
    if(typeof(item.setResizable) == 'function')
      item.setResizable(true);

    if(this._children.length == 0 ){  
      this._sendOnClose();
      Groups.unregister(this);
      $(this.container).fadeOut(function() {
        $(this).remove();
      });
    } else if(!options.dontArrange) {
      this.arrange();
    }
  },
  
  // ----------
  removeAll: function() {
    var self = this;
    var toRemove = $.merge([], this._children);
    $.each(toRemove, function(index, child) {
      self.remove(child, {dontArrange: true});
    });
  },
    
  // ----------  
  arrange: function(options) {
    if( options && options.animate == false ) 
      animate = false;
    else 
      animate = true;

    if(typeof(options) == 'undefined')
      options = {};
    
    var bb = this.getContentBounds();

    var count = this._children.length;
    var bbAspect = bb.width/bb.height;
    var tabAspect = 4/3; 
    
    function howManyColumns( numRows, count ){ return Math.ceil(count/numRows) }
    
    var count = this._children.length;
    var best = {cols: 0, rows:0, area:0};
    for(var numRows=1; numRows<=count; numRows++){
      numCols = howManyColumns( numRows, count);
      var w = numCols*tabAspect;
      var h = numRows;
      
      // We are width constrained
      if( w/bb.width >= h/bb.height ) var scale = bb.width/w;
      // We are height constrained
      else var scale = bb.height/h;
      var w = w*scale;
      var h = h*scale;
            
      if( w*h >= best.area ){
        best.numRows = numRows;
        best.numCols = numCols;
        best.area = w*h;
        best.w = w;
        best.h = h;
      }
    }
    
    var padAmount = .1;
    var pad = padAmount * (best.w/best.numCols);
    var tabW = (best.w-pad)/best.numCols - pad;
    var tabH = (best.h-pad)/best.numRows - pad;
    
    var x = pad; var y=pad; var numInCol = 0;
    for each(var item in this._children){
      item.setBounds(new Rect(x + bb.left, y + bb.top, tabW, tabH), !animate);
      
      x += tabW + pad;
      numInCol += 1;
      if( numInCol >= best.numCols ) 
        [x, numInCol, y] = [pad, 0, y+tabH+pad];
    }
  },
  
  // ----------  
  _addHandlers: function(container){
    var self = this;
    
    $(container).draggable({
      start: function(){
        drag.info = new DragInfo(this);
      },
      drag: function(e, ui){
        drag.info.drag(e, ui);
      }, 
      stop: function() {
        drag.info.stop();
        drag.info = null;
      }
    });
    
    $(container).droppable({
      over: function(){
        drag.info.$el.addClass("willGroup");
      },
      out: function(){
        var $group = drag.info.$el.data("group");
        if($group)
          $group.remove(drag.info.$el);
        drag.info.$el.removeClass("willGroup");
      },
      drop: function(event){
        drag.info.$el.removeClass("willGroup");
        self.add( drag.info.$el, {left:event.pageX, top:event.pageY} );
      },
      accept: ".tab, .group",
    });
  },

  // ----------  
  setResizable: function(value){
    var self = this;
    
    if(value) {
      this.$resizer.fadeIn();
      $(this.container).resizable({
        handles: "se",
        aspectRatio: false,
        resize: function(){
          self.reloadBounds();
        },
        stop: function(){
          self.reloadBounds();
          self.pushAway();
        } 
      });
    } else {
      this.$resizer.fadeOut();
      $(this.container).resizable('disable');
    }
  }
});

// ##########
var DragInfo = function(element) {
  this.el = element;
  this.$el = $(this.el);
  this.item = Items.item(this.el);
  
  this.$el.data('isDragging', true);
  this.item.setZ(9999);
};

DragInfo.prototype = {
  // ----------  
  drag: function(e, ui) {
    this.item.reloadBounds();
  },

  // ----------  
  stop: function() {
    this.$el.data('isDragging', false);    
    if(this.item && !this.$el.hasClass('willGroup') && !this.$el.data('group')) {
      this.item.setZ(drag.zIndex);
      drag.zIndex++;
      
      this.item.reloadBounds();
      this.item.pushAway();
    }
  }
};

var drag = {
  info: null,
  zIndex: 100
};

// ##########
window.Groups = {
  groups: [],
  
  // ----------  
  dragOptions: {
    start: function() {
      drag.info = new DragInfo(this);
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
    accept: ".tab",
    tolerance: "pointer",
    greedy: true,
    drop: function(e){
      $target = $(e.target);  
      drag.info.$el.removeClass("willGroup")   
      var phantom = $target.data("phantomGroup")
      
      var group = $target.data("group");
      if( group == null ){
        phantom.removeClass("phantom");
        phantom.removeClass("group-content");
        var group = new Group([$target, drag.info.$el], {container:phantom});
      } else 
        group.add( drag.info.$el );      
    },
    over: function(e){
      var $target = $(e.target);

      function elToRect($el){
       return new Rect( $el.position().left, $el.position().top, $el.width(), $el.height() );
      }

      var height = elToRect($target).height * 1.5 + 20;
      var width = elToRect($target).width * 1.5 + 20;
      var unionRect = elToRect($target).union( elToRect(drag.info.$el) );

      var newLeft = unionRect.left + unionRect.width/2 - width/2;
      var newTop = unionRect.top + unionRect.height/2 - height/2;

      $(".phantom").remove();
      var phantom = $("<div class='group phantom group-content'/>").css({
        width: width,
        height: height,
        position:"absolute",
        top: newTop,
        left: newLeft,
        zIndex: -99
      }).appendTo("body").hide().fadeIn();
      $target.data("phantomGroup", phantom);      
    },
    out: function(e){      
      $(e.target).data("phantomGroup").fadeOut(function(){
        $(this).remove();
      });
    }
  }, 
  
  // ----------  
  register: function(group) {
    Utils.assert('only register once per group', $.inArray(group, this.groups) == -1);
    this.groups.push(group);
  },
  
  // ----------  
  unregister: function(group) {
    var index = $.inArray(group, this.groups);
    if(index != -1)
      this.groups.splice(index, 1);     
  },
  
  // ----------  
  arrange: function() {
    var count = this.groups.length;
    var columns = Math.ceil(Math.sqrt(count));
    var rows = ((columns * columns) - count >= columns ? columns - 1 : columns); 
    var padding = 12;
    var startX = padding;
    var startY = Page.startY;
    var totalWidth = window.innerWidth - startX;
    var totalHeight = window.innerHeight - startY;
    var box = new Rect(startX, startY, 
        (totalWidth / columns) - padding,
        (totalHeight / rows) - padding);
    
    $.each(this.groups, function(index, group) {
      group.setBounds(box, true);
      
      box.left += box.width + padding;
      if(index % columns == columns - 1) {
        box.left = startX;
        box.top += box.height + padding;
      }
    });
  },
  
  // ----------
  removeAll: function() {
    var toRemove = $.merge([], this.groups);
    $.each(toRemove, function(index, group) {
      group.removeAll();
    });
  }
};

// ##########
$(".tab").data('isDragging', false)
  .draggable(window.Groups.dragOptions)
  .droppable(window.Groups.dropOptions);

})();