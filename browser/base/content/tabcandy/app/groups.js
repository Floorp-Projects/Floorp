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
function Group(){}
Group.prototype = {
  _children: [],
  _container: null,
  _padding: 30,
  
  // ----------  
  _getBoundingBox: function(){
    var els = this._children;
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
  create: function(listOfEls, options) {
    var self = this;
    this._children = $(listOfEls).toArray();

    var boundingBox = this._getBoundingBox();
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
      $.each(self._children, function(index, child) {
        var tab = Tabs.tab(child);
        tab.close();
      });
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
    
    this._addHandlers(container);
    this._updateGroup();

    var els = this._children;
    this._children = [];
    for(var i in els){
      this.add( els[i] );
    }
    
    // ___ Push other objects away
    if(!options || !options.suppressPush)
      this.pushAway(); 
  },
  
  // ----------  
  pushAway: function() {
  	return; //doesn't work in this version of groups.js
    var buffer = 10;
    
    var items = Items.getTopLevelItems();
    $.each(items, function(index, item) {
      var data = {};
      data.bounds = item.getBounds();
      data.startBounds = new Rect(data.bounds);
      item.pushAwayData = data;
    });
    
    var itemsToPush = [this];
    this.pushAwayData.anchored = true;

    var pushOne = function(baseItem) {
      var bb = new Rect(baseItem.pushAwayData.bounds);
      bb.inset(-buffer, -buffer);
      var bbc = bb.center();
    
      $.each(items, function(index, item) {
        if(item == baseItem)
          return;
          
        var data = item.pushAwayData;
        if(data.anchored)
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
          itemsToPush.push(item);
        }
      });
    };   
    
    var a;
    for(a = 0; a < 500 && itemsToPush.length; a++)
      pushOne(itemsToPush.shift());         

    $.each(items, function(index, item) {
      var data = item.pushAwayData;
      if(!data.bounds.equals(data.startBounds))
        item.setPosition(data.bounds.left, data.bounds.top);
    });
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
  setPosition: function(left, top) {
    var box = this.getBounds();
    var offset = new Point(left - box.left, top - box.top);
    
    $.each(this._children, function(index, value) {
      var $el = $(this);
      box = Utils.getBounds(this);
      $el.animate({left: box.left + offset.x, top: box.top + offset.y});
    });
        
    var bb = Utils.getBounds(this._container);
    $(this._container).animate({left: bb.left + offset.x, top: bb.top + offset.y});
  },

  // ----------  
  add: function($el, dropPos){
/*     Utils.assert('add expects jQuery objects', Utils.isJQuery($el)); */
    var el = $el.get(0);
    
    if( typeof(dropPos) == "undefined" ) dropPos = {top:window.innerWidth, left:window.innerHeight};
    var self = this;
    
    // TODO: You should be allowed to drop in the white space at the bottom and have it go to the end
    // (right now it can match the thumbnail above it and go there)
    function findInsertionPoint(dropPos){
      var best = {dist: Infinity, el: null};
      var index = 0;
      for each(var child in self._children){
        var pos = $(child).position();
        var [w, h] = [$(child).width(), $(child).height()];
        var dist = Math.sqrt( Math.pow((pos.top+h/2)-dropPos.top,2) + Math.pow((pos.left+w/2)-dropPos.left,2) );
/*         Utils.log( index, dist ); */
        if( dist <= best.dist ){
          best.el = child;
          best.dist = dist;
          best.index = index;
        }
        index += 1;
      }

      if( self._children.length > 0 ){
        var insertLeft = dropPos.left <= $(best.el).position().left + $(best.el).width()/2;
        if( !insertLeft ) return best.index+1
        else return best.index
      }
      return 0;
      
    }
    
    var oldIndex = $.inArray(el, this._children);
    if(oldIndex != -1)
      this._children.splice(oldIndex, 1); 

    var index = findInsertionPoint(dropPos);
    this._children.splice( index, 0, el );

    $(el).droppable("disable");

    if( typeof(Tabs) != "undefined" ){
      var tab = Tabs.tab(el);
      tab.mirror.addOnClose(el, function() {
        self.remove($el);
      });      
    }
    
    this._updateGroup();
    this.arrange();
  },
  
  // ----------  
  remove: function(el){
    var self = this;
    $(el).data("toRemove", true);
    this._children = this._children.filter(function(child){
      if( $(child).data("toRemove") == true ){
        $(child).data("group", null);
        scaleTab( $(child), 160/$(child).width());
        $(child).droppable("enable");    
        if( typeof(Tabs) != "undefined" ){
          var tab = Tabs.tab(child);
          tab.mirror.removeOnClose(el);              
        }
        return false;
      }
      else return true;
    });
    
    $(el).data("toRemove", false);
    
    if( this._children.length == 0 ){
      this._container.fadeOut(function() $(this).remove());
    } else {
      this.arrange();
    }
    
  },
  
  // ----------
  // TODO: could be a lot more efficient by unwrapping the remove routine
  removeAll: function() {
    var self = this;
    $.each(this._children, function(index, child) {
      self.remove(child);
    });
  },
  
  // ----------  
  _updateGroup: function(){
    var self = this;
    this._children.forEach(function(el){
      $(el).data("group", self);
    });    
  },
  
  // ----------  
  arrange: function(options){
    if( options && options.animate == false ) animate = false;
    else animate = true;
    
    //Utils.log(speed);
    /*if( this._children.length < 2 ) return;*/
    var bb = this._getContainerBox();
    var aTab = $(this._children[0]);

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
    for each(var tab in this._children){
      var sizeOptions = {width:tabW, height:tabH, top:y+bb.top, left:x+bb.left};
      
      if( animate ) $(tab).animate(sizeOptions).dequeue();
      else $(tab).css(sizeOptions).dequeue()
      
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
        self._children.forEach(function(el){
          $(el).data("origPosition", $(el).position());
        });
      },
      drag: function(e, ui){
        var origPos = $(container).data("origPosition");
        dX = ui.offset.left - origPos.left;
        dY = ui.offset.top - origPos.top;
        $(self._children).each(function(){
          $(this).css({
            left: $(this).data("origPosition").left + dX,
            top:  $(this).data("origPosition").top + dY
          })
        })
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
      } 
    })
    
    }
}

// ##########
var zIndex = 100;
var $dragged = null;
var timeout = null;

window.Groups = {
  Group: Group, 
  
  // ----------  
  dragOptions: {
    start:function(){
      $dragged = $(this);
      $dragged.data('isDragging', true);
    },
    stop: function(){
      $dragged.data('isDragging', false);
      $(this).css({zIndex: zIndex});
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
            var group = new Group();
            group.create([target, dragged]);            
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
    var w = (totalWidth / columns) - padding;
    var h = (totalHeight / rows) - padding;
    var x = startX;
    var y = startY;
    
    $groups.each(function(i) {
      $(this).css({left: x, top: y, width: w, height: h});
      
      $(this).data('group').arrange();
      
      x += w + padding;
      if(i % columns == columns - 1) {
        x = startX;
        y += h + padding;
      }
    });
  },
  
  // ----------
  removeAll: function() {
    var $groups = $('.group');
    $groups.each(function() {
      var group = $(this).data('group');
      group.removeAll();
    });
  }
};

// ##########
window.Items = {
  // ----------  
  getTopLevelItems: function() {
    var items = [];
    
    $('.tab').each(function() {
      $this = $(this);
      if(!$this.data('group'))
        items.push($this.data('tabItem'));
    });
    
    $('.group').each(function() {
      items.push($(this).data('group'));
    });
    
    return items;
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