// Title: groups.js (revision-a)
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
// Class: Group
// A single group in the tab candy window. Descended from <Item>.
// Note that it implements the <Subscribable> interface.
window.Group = function(listOfEls, options) {
  if(typeof(options) == 'undefined')
    options = {};

  this._children = []; // an array of Items
  this.defaultSize = new Point(TabItems.tabWidth * 1.5, TabItems.tabHeight * 1.5);
  this.locked = options.locked || false;
  this.isAGroup = true;
  this.id = options.id || Groups.getNextID();
  this.userSize = options.userSize || null;
  this._isStacked = false;
  this._stackAngles = [0];
  this.expanded = null;

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
  var html = "<div class='titlebar'><input class='name' value='"
      + (options.title || "")
      + "'/><div class='close'></div></div>";
       
  this.$titlebar = $(html)        
    .appendTo($container);
    
  this.$titlebar.css({
      position: "absolute",
    });
    
  var $close = $('.close', this.$titlebar).click(function() {
    self.closeAll();
  });
  
  // ___ Title 
  var titleUnfocus = function() {
    if(!self.getTitle()) {
      self.$title
        .addClass("defaultName")
        .val(self.defaultName);
    } else {
      self.$title.css({"background":"none"})
        .animate({"paddingLeft":1, "easing":"linear"}, 340);
    }
  };
  
  var handleKeyPress = function(e){
    if( e.which == 13 ) { // return
      self.$title.blur()
        .addClass("transparentBorder")
        .one("mouseout", function(){
          self.$title.removeClass("transparentBorder");
        });
    }
  }
  
  this.$title = $('.name', this.$titlebar)
    .css({backgroundRepeat: 'no-repeat'})
    .blur(titleUnfocus)
    .focus(function() {
      if(self.locked) {
        self.$title.blur();
        return;
      }  
      self.$title.select();
      if(!self.getTitle()) {
        self.$title
          .removeClass("defaultName")
          .val('');
      }
    })
    .keydown(handleKeyPress);
  
  titleUnfocus();
  
  if(this.locked)
    this.$title.addClass('name-locked');

  // ___ Content
  this.$content = $('<div class="group-content"/>')
    .css({
      left: 0,
      top: this.$titlebar.height(),
      position: 'absolute'
    })
    .appendTo($container);
  
  // ___ locking
  if(this.locked) {
    $container.css({cursor: 'default'});    
/*     $close.hide(); */
  }
    
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
  defaultName: "name this group...",
  
  // ----------  
  getStorageData: function() {
    var data = {
      bounds: this.getBounds(), 
      userSize: this.userSize,
      locked: this.locked, 
      title: this.getTitle(),
      id: this.id
    };
    
    return data;
  },
  
  // ----------
  getTitle: function() {
    var value = (this.$title ? this.$title.val() : '');
    return (value == this.defaultName ? '' : value);
  },

  // ----------  
  setValue: function(value) {
    this.$title.val(value); 
  },

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
        this.arrange({animate: !immediately}); //(immediately ? 'sometimes' : true)});
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

    if(this.$debug) 
      this.$debug.css({zIndex: value + 1});

    $.each(this._children, function(index, child) {
      child.setZ(value + 1);
    });
  },
    
  // ----------
  close: function() {
    this.removeAll();
    this._sendOnClose();
    Groups.unregister(this);
    $(this.container).fadeOut(function() {
      $(this).remove();
    });
  },
  
  // ----------  
  closeAll: function() {
    if(this._children.length) {
      var toClose = $.merge([], this._children);
      $.each(toClose, function(index, child) {
        child.close();
      });
    } else 
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
    var item;
    var $el;
    if(a.isAnItem) {
      item = a;
      $el = $(a.container);
    } else {
      $el = $(a);
      item = Items.item($el);
    }    
    
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
      $.each(self._children, function(index, child) {        
        box = child.getBounds();
        if(box.bottom < dropPos.top || box.top > dropPos.top)
          return;
        
        var dist = Math.sqrt( Math.pow((box.top+box.height/2)-dropPos.top,2) + Math.pow((box.left+box.width/2)-dropPos.left,2) );
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
    
    var oldIndex = $.inArray(item, this._children);
    if(oldIndex != -1)
      this._children.splice(oldIndex, 1); 

    // Insert the tab into the right position.
    var index = findInsertionPoint(dropPos);
    this._children.splice( index, 0, item );

    $el.droppable("disable");
    item.setZ(this.getZ() + 1);
    item.groupData = {};

    item.addOnClose(this, function() {
      self.remove($el);
    });
    
    item.parent = this;
    $el.addClass("tabInGroup");
    
    if(typeof(item.setResizable) == 'function')
      item.setResizable(false);
    
    if(!options.dontArrange)
      this.arrange();
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
    
    item.parent = null;
    $el.removeClass("tabInGroup");
    
    item.setSize(item.defaultSize.x, item.defaultSize.y);

    $el.droppable("enable");    
    item.removeOnClose(this);
    
    if(typeof(item.setResizable) == 'function')
      item.setResizable(true);

    if(this._children.length == 0 && !this.locked && !this.getTitle()){  
      this.close();
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
    var count = this._children.length;
    if(!count)
      return;

    var bb = this.getContentBounds();
    bb.inset(6, 6);

    if((bb.width * bb.height) / count > 7000) {
      Items.arrange(this._children, bb, options);
      this._isStacked = false;
    } else
      this._stackArrange(bb, options);
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
    var y = Math.min(x, (bb.height - h) / 2);
    var box = new Rect(bb.left + x, bb.top + y, w, h);
    
    var self = this;
    $.each(this._children, function(index, child) {
      if(!child.locked) {
        child.setZ(zIndex);
        zIndex--;
        
        child.setBounds(box, !animate);
        child.setRotation(self._randRotate(35, index));
      }
    });
    
    self._isStacked = true;
  },

  // ----------
  _randRotate: function(spread, index){
    if( index >= this._stackAngles.length ){
      var randAngle = parseInt( ((Math.random()+.6)/1.3)*spread-(spread/2) );
      this._stackAngles.push(randAngle);
      return randAngle;          
    }

    return this._stackAngles[index];
  },

  // ----------
  childHit: function(child) {
    var self = this;
    
    // ___ normal click
    if(!this._isStacked || this.expanded) {
      setTimeout(function() {
        self.collapse();
      }, 200);
      
      return false;
    }
      
    // ___ we're stacked, so expand
    var startBounds = child.getBounds();
    var $tray = $("<div />").css({
      "backgroundColor": "rgba(0,0,0,0)",
      top: startBounds.top,
      left: startBounds.left,
      width: startBounds.width,
      height: startBounds.height,
      position: "absolute",
      zIndex: 99998
    }).appendTo("body");

    var w = 240;
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
    
    $tray.animate({
      width:  overlayWidth,
      height: overlayHeight,
      top: pos.top,
      left: pos.left
    },170).addClass("overlay");

    var box = new Rect(pos.left, pos.top, overlayWidth, overlayHeight);
    box.inset(8, 8);
    Items.arrange(this._children, box, {padding: 8, z: 99999});
    
    var $shield = $('<div />')
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
      $shield: $shield
    };

    return true;
  },

  // ----------
  collapse: function() {
    if(this.expanded) {
      this.expanded.$tray.remove();
      this.expanded.$shield.remove();
      this.expanded = null;
            
      this.arrange({z: this.getZ() + 1});
    }
  },
  
  // ----------  
  _addHandlers: function(container) {
    var self = this;
    
    if(!this.locked) {
      $(container).draggable({
        scroll: false,
        cancel: '.close, .name',
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
    }
    
    $(container).droppable({
      tolerance: "intersect",
      over: function(){
        drag.info.$el.addClass("willGroup");
      },
      out: function(){
        var group = drag.info.item.parent;
        if(group) {
          group.remove(drag.info.$el);
        }
          
        drag.info.$el.removeClass("willGroup");
      },
      drop: function(event){
        drag.info.$el.removeClass("willGroup");
        self.add( drag.info.$el, {left:event.pageX, top:event.pageY} );
      },
      accept: ".tab", //".tab, .group",
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
        minWidth: 60,
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
      $(this.container).resizable('disable');
    }
  }
});

// ##########
var DragInfo = function(element) {
  this.el = element;
  this.$el = $(this.el);
  this.item = Items.item(this.el);
  this.parent = this.item.parent;
  
  this.$el.data('isDragging', true);
  this.item.setZ(99999);
};

DragInfo.prototype = {
  // ----------  
  drag: function(e, ui) {
    if(this.item.isAGroup) {
      var bb = this.item.getBounds();
      bb.left = ui.position.left;
      bb.top = ui.position.top;
      this.item.setBounds(bb, true);
    } else
      this.item.reloadBounds();
      
    if(this.parent && this.parent.expanded) {
      this.item.locked = true;
      this.parent.collapse();
      this.item.locked = false;
    }
  },

  // ----------  
  stop: function() {
    this.$el.data('isDragging', false);    

    if(this.parent && !this.parent.locked && this.parent != this.item.parent 
        && this.parent._children.length == 1 && !this.parent.getTitle()) {
      this.parent.remove(this.parent._children[0]);
    }
     
    if(this.item && !this.$el.hasClass('willGroup') && !this.item.parent) {
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
// Class: Groups
// Singelton for managing all <Group>s. 
window.Groups = {
  // ----------  
  dragOptions: {
    scroll: false,
    cancel: '.close',
    refreshPositions: true,
    start: function(e, ui) {
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
    tolerance: "intersect",
    greedy: true,
    drop: function(e){
      $target = $(e.target);  
      drag.info.$el.removeClass("willGroup")   
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
  init: function() {
    this.groups = [];
    this.nextID = 1;
  },
  
  // ----------
  getNextID: function() {
    var result = this.nextID;
    this.nextID++;
    return result;
  },

  // ----------
  getStorageData: function() {
    var data = {nextID: this.nextID, groups: []};
    $.each(this.groups, function(index, group) {
      data.groups.push(group.getStorageData());
    });
    
    return data;
  },
  
  // ----------
  reconstitute: function(data) {
    if(data && data.nextID)
      this.nextID = data.nextID;
      
    if(data && data.groups) {
      $.each(data.groups, function(index, group) {
        new Group([], $.extend({}, group, {dontPush: true})); 
      });
    } else {
      var box = this.getBoundsForNewTabGroup();
      new Group([], {bounds: box, title: 'New Tabs', locked: true, dontPush: true}); 
    }
  },
  
  // ----------
  getNewTabGroup: function() {
    var groupTitle = 'New Tabs';
    var array = jQuery.grep(this.groups, function(group) {
      return group.getTitle() == groupTitle;
    });
    
    if(array.length) 
      return array[0];
      
    return null;
  },

  // ----------
  getBoundsForNewTabGroup: function() {
    var pad = 5;
    var sw = window.innerWidth;
    var sh = window.innerHeight;
    var w = sw - (pad * 2);
    var h = TabItems.tabHeight;
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
  // Function: group
  // Given some sort of identifier, returns the appropriate group.
  // Currently only supports group ids. 
  group: function(a) {
    var result = null;
    $.each(this.groups, function(index, candidate) {
      if(candidate.id == a) {
        result = candidate;
        return false;
      }
    });
    
    return result;
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
  },
  
  // ----------
  newTab: function(tabItem) {
    var group = this.getNewTabGroup();
    var $el = $(tabItem.container);
    if(group) 
      group.add($el);
  }
};

// ----------
Groups.init();

// ##########
$(".tab").data('isDragging', false)
  .draggable(window.Groups.dragOptions)
  .droppable(window.Groups.dropOptions);

})();