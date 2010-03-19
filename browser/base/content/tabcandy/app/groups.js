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

function Group(){}
Group.prototype = {
  _children: [],
  _container: null,
  _padding: 30,
  
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
  
  create: function(listOfEls){
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
      })

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
  },
  
  add: function(el){
    if($.inArray(el, this._children) == -1)
      this._children.push( el );

    $(el).droppable("disable");
    
    this._updateGroup();
    this.arrange();
  },
  
  remove: function(el){
    $(el).data("toRemove", true);
    this._children = this._children.filter(function(child){
      if( $(child).data("toRemove") == true ){
        $(child).data("group", null);
        scaleTab( $(child), 160/$(child).width());
        $(child).droppable("enable");        
        return false;
      }
      else return true;
    })
    $(el).data("toRemove", false);
    
    if( this._children.length == 0 ){
      this._container.fadeOut(function() $(this).remove());
    } else {
      this.arrange();
    }
    
  },
  
  _updateGroup: function(){
    var self = this;
    this._children.forEach(function(el){
      $(el).data("group", self);
    });    
  },
  
  arrange: function(){
    if( this._children.length < 2 ) return;
    var bb = this._getContainerBox();
    var tab;

    // TODO: This is an hacky heuristic. Fix this layout algorithm.
    var pad = 10;
    var w = parseInt(Math.sqrt(((bb.height+pad) * (bb.width+pad))/(this._children.length+4)));
    var h = w * (2/3);

    var x=pad;
    var y=pad;
    for([,tab] in Iterator(this._children)){      
      scaleTab( $(tab), w/$(tab).width());
      $(tab).animate({
        top:y+bb.top, left:x+bb.left,
      }, 250);
      
      x += w+pad;
      if( x+w+pad > $(this._container).width()){x = pad;y += h+pad;}
    }
    
  },
  
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
        $dragged.data("group").remove($dragged);
        $dragged.removeClass("willGroup");
      },
      drop: function(){
        $dragged.removeClass("willGroup");
        self.add( $dragged.get(0) )
      },
      accept: ".tab",
    });
        
    $(container).resizable({
      handles: "se",
      aspectRatio: true,
      stop: function(){
        self.arrange();
      } 
    })
    
    }
}

var zIndex = 100;
var $dragged = null;
var timeout = null;

window.Groups = {
  Group: Group, 
  
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
            group.create( [target, dragged] );            
          } else {
            group.add( dragged.get(0) )
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
  }  
};

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