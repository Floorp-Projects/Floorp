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
  this._children = [];
  this._container = null;
  this._padding = 30;

  var self = this;

  var boundingBox = this._getBoundingBox(listOfEls);
  var padding = 30;
  var container = $("<div class='group'/>")
    .css({
      position: "absolute",
      top: boundingBox.top-padding,
      left: boundingBox.left-padding,
      width: boundingBox.width+padding*2,
      height: boundingBox.height+padding*2,
      zIndex: -100,
      opacity: 0,
    })
    .data("group", this)
    .data('item', this)
    .appendTo("body")
    .animate({opacity:1.0}).dequeue();
  
/*
  var contentEl = $('<div class="group-content"/>')
    .appendTo(container);
*/
  
  var resizer = $("<div class='resizer'/>")
    .css({
      position: "absolute",
      width: 16, height: 16,
      bottom: 0, right: 0,
    }).appendTo(container);

  var titlebar = $("<div class='titlebar'><input class='name' value=''/><div class='close'>x</div></div>")
    .appendTo(container)
  
  titlebar.css({
      width: container.width(),
      position: "relative",
      top: -(titlebar.height()+2),
      left: -1,
    });
    
  $('.close', titlebar).click(function() {
    self.close();
  });

  // On delay, show the title bar.
  var shouldShow = false;
  container.mouseover(function(){
    shouldShow = true;
    setTimeout(function(){
      if( shouldShow == false ) return;
      container.find("input").focus();
      titlebar
        .css({width: container.width()})
        .animate({ opacity: 1}).dequeue();        
    }, 500);
  }).mouseout(function(e){
    shouldShow = false;
    if( isEventOverElement(e, container.get(0) )) return;
    titlebar.animate({opacity:0}).dequeue();
  })

  this._container = container;

  $.each(listOfEls, function(index, el) {  
    self.add(el);
  });

  this._addHandlers(container);
  
  Groups.register(this);
  
  // ___ Push other objects away
  if(!options || !options.suppressPush)
    this.pushAway();   
};

// ----------
window.Group.prototype = $.extend(new Item(), {  
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
  _getContainerBox: function(){
    var pos = $(this._container).position();
    var w = $(this._container).width();
    var h = $(this._container).height();
    return {
      top: pos.top,
      left: pos.left,
      bottom: pos.top + h,
      right: pos.left + w,
      height: h,
      width: w
    }
  },
  
  // ----------
  getContainer: function() {
    return this._container;
  },

  // ----------  
  getBounds: function() {
    var $titlebar = $('.titlebar', this._container);
    var bb = Utils.getBounds(this._container);
    var titleHeight = $titlebar.height();
    bb.top -= titleHeight;
    bb.height += titleHeight;
    return bb;
  },
  
  // ----------  
  setBounds: function(rect) {
    this.setPosition(rect.left, rect.top);
    this.setSize(rect.width, rect.height);
  },
  
  // ----------  
  setPosition: function(left, top, immediately) {
    var box = this.getBounds();
    var offset = new Point(left - box.left, top - box.top);
    
    $.each(this._children, function(index, child) {
      box = child.getBounds();
      child.setPosition(box.left + offset.x, box.top + offset.y, immediately);
    });
        
    box = Utils.getBounds(this._container);
    var options = {left: box.left + offset.x, top: box.top + offset.y};
    if(immediately)
      $(this._container).css(options);
    else
      $(this._container).animate(options);
  },

  // ----------  
  setSize: function(width, height) {
    var $titlebar = $('.titlebar', this._container);
    var titleHeight = $titlebar.height();
    $(this._container).animate({width: width, height: height - titleHeight});
    this.arrange();
  },

  // ----------  
  close: function() {
    $.each(this._children, function(index, child) {
      child.close();
    });
  },
  
  // ----------
  addOnClose: function(referenceObject, callback) {
    Utils.error('Group.addOnClose not implemented');
  },

  // ----------
  removeOnClose: function(referenceObject) {
    Utils.error('Group.removeOnClose not implemented');
  },
  
  // ----------  
  add: function($el, dropPos){
    Utils.assert('add expects jQuery objects', Utils.isJQuery($el));
    
    if( typeof(dropPos) == "undefined" ) 
      dropPos = {top:window.innerWidth, left:window.innerHeight};
      
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

    item.addOnClose(this, function() {
      self.remove($el);
    });      
    
    $el.data("group", this);
    this.arrange();
  },
  
  // ----------  
  // The argument a can be an Item, a DOM element, or a jQuery object
  remove: function(a, options) {
    var $el;  
    var item;

    if(a.isAnItem) {
      item = a;
      $el = $(item.getContainer());
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
    scaleTab( $el, 160/$el.width());
    $el.droppable("enable");    
    item.removeOnClose(this);
    
    if(this._children.length == 0 ){
      Groups.unregister(this);
      this._container.fadeOut(function() {
        $(this).remove();
      });
    } else if(!options.dontArrange) {
      this.arrange();
    }
  },
  
  // ----------
  removeAll: function() {
    while(this._children.length)
      this.remove(this._children[0], {dontArrange: true});
  },
    
  // ----------  
  arrange: function(options){
    if( options && options.animate == false ) animate = false;
    else animate = true;
    
    //Utils.log(speed);
    /*if( this._children.length < 2 ) return;*/
    var bb = this._getContainerBox();

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
      var sizeOptions = {width:tabW, height:tabH, top:y+bb.top, left:x+bb.left};
      
      if( animate ) $(item.getContainer()).animate(sizeOptions).dequeue();
      else $(item.getContainer()).css(sizeOptions).dequeue();
      
      x += tabW + pad;
      numInCol += 1;
      if( numInCol >= best.numCols ) [x, numInCol, y] = [pad, 0, y+tabH+pad];
    }
  },
  
  // ----------  
  _addHandlers: function(container){
    var self = this;
    
    $(container).draggable({
      start: function(){
        $(container).data("origPosition", $(container).position());
        $.each(self._children, function(index, child) {
          child.dragData = {startBounds: child.getBounds()};
        });
      },
      drag: function(e, ui){
        var origPos = $(container).data("origPosition");
        dX = ui.offset.left - origPos.left;
        dY = ui.offset.top - origPos.top;
        $.each(self._children, function(index, child) {
          child.setPosition(child.dragData.startBounds.left + dX, 
            child.dragData.startBounds.top + dY, 
            true);
        });
      }, 
      stop: function() {
        self.pushAway();
      }
    });
    
    $(container).droppable({
      over: function(){
        $dragged.addClass("willGroup");
      },
      out: function(){
        var $group = $dragged.data("group");
        if($group)
          $group.remove($dragged);
        $dragged.removeClass("willGroup");
      },
      drop: function(event){
        $dragged.removeClass("willGroup");
        self.add( $dragged, {left:event.pageX, top:event.pageY} )
      },
      accept: ".tab",
    });
        
    $(container).resizable({
      handles: "se",
      aspectRatio: false,
      resize: function(){
        self.arrange({animate: false});
      },
      stop: function(){
        self.arrange();
        self.pushAway();
      } 
    });
  }
});

// ##########
var zIndex = 100;
var $dragged = null;
var timeout = null;

window.Groups = {
  groups: [],
  
  // ----------  
  dragOptions: {
    start:function(){
      $dragged = $(this);
      $dragged.data('isDragging', true);
    },
    stop: function(){
      $dragged.data('isDragging', false);
      $(this).css({zIndex: zIndex});
      if(!$dragged.hasClass('willGroup') && !$dragged.data('group'))
        Items.item(this).pushAway();

      $dragged = null;          
      zIndex += 1;
    },
    zIndex: 999,
  },
  
  // ----------  
  dropOptions: {
    accept: ".tab",
    tolerance: "pointer",
    greedy: true,
    drop: function(e){
      $target = $(e.target);
  
      // Only drop onto the top z-index
      if( $target.css("zIndex") < $dragged.data("topDropZIndex") ) return;
      $dragged.data("topDropZIndex", $target.css("zIndex") );
      $dragged.data("topDrop", $target);
      
      // This strange timeout thing solves the problem of when
      // something is dropped onto multiple potential drop targets.
      // We wait a little bit to see get all drops, and then we have saved
      // the top-most one and drop onto that.
      clearTimeout( timeout );
      var dragged = $dragged;
      var target = $target;
      timeout = setTimeout( function(){
        dragged.removeClass("willGroup")   
  
        dragged.animate({
          top: target.position().top+15,
          left: target.position().left+15,      
        }, 100);
        
        setTimeout( function(){
          var group = $(target).data("group");
          if( group == null ){
            var group = new Group([target, dragged]);
          } else {
            group.add( dragged );
          }
          
        }, 100);
        
        
      }, 10 );
      
      
    },
    over: function(e){
      $dragged.addClass("willGroup");
      $dragged.data("topDropZIndex", 0);    
    },
    out: function(){      
      $dragged.removeClass("willGroup");
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
    var $groups = $('.group');
    var count = $groups.length;
    var columns = Math.ceil(Math.sqrt(count));
    var rows = ((columns * columns) - count >= columns ? columns - 1 : columns); 
    var padding = 12;
    var startX = padding;
    var startY = 100;
    var totalWidth = window.innerWidth - startX;
    var totalHeight = window.innerHeight - startY;
    var box = new Rect(startX, startY, 
        (totalWidth / columns) - padding,
        (totalHeight / rows) - padding);
    
    $.each(this.groups, function(index, group) {
      group.setBounds(box);
      
      box.left += box.width + padding;
      if(index % columns == columns - 1) {
        box.left = startX;
        box.top += box.height + padding;
      }
    });
  },
  
  // ----------
  removeAll: function() {
    while(this.groups.length)
      this.groups[0].removeAll();  
  }
};

// ##########
function scaleTab( el, factor ){  
  var $el = $(el);

  $el.animate({
    width: $el.width()*factor,
    height: $el.height()*factor,
    fontSize: parseInt($el.css("fontSize"))*factor,
  },250).dequeue();
}


$(".tab").data('isDragging', false)
  .draggable(window.Groups.dragOptions)
  .droppable(window.Groups.dropOptions);


})();