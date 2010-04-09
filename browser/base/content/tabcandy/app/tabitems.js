// ##########
window.TabItem = function(container, tab) {
  this._init(container);
  this.tab = tab;
  this.defaultSize = new Point(TabItems.tabWidth, TabItems.tabHeight);
};

window.TabItem.prototype = $.extend(new Item(), {
  // ----------  
  reloadBounds: function() {
    this.bounds = Utils.getBounds(this.container);
    this._updateDebugBounds();
  },
  
  // ----------  
  setBounds: function(rect, immediately) {
    var css = {};

    if(rect.left != this.bounds.left)
      css.left = rect.left;
      
    if(rect.top != this.bounds.top)
      css.top = rect.top;
      
    if(rect.width != this.bounds.width) {
      css.width = rect.width;
      var scale = rect.width / TabItems.tabWidth;
      css.fontSize = TabItems.fontSize * scale;
    }

    if(rect.height != this.bounds.height)
      css.height = rect.height; 
      
    if($.isEmptyObject(css))
      return;
      
    this.bounds.copy(rect);

    if(immediately) {
      $(this.container).css(css);
    } else {
      TabMirror.pausePainting();
      $(this.container).animate(css, {complete: function() {
        TabMirror.resumePainting();
      }}).dequeue();
    }

    this._updateDebugBounds();
  },

  // ----------
  setZ: function(value) {
    $(this.container).css({zIndex: value});
  },
    
  // ----------
  close: function() {
    this.tab.close();
  },
  
  // ----------
  addOnClose: function(referenceObject, callback) {
    this.tab.mirror.addOnClose(referenceObject, callback);      
  },

  // ----------
  removeOnClose: function(referenceObject) {
    this.tab.mirror.removeOnClose(referenceObject);      
  }
});

// ##########
window.TabItems = {
  tabWidth: 160,
  tabHeight: 120, 
  fontSize: 9,

  init: function() {
    var self = this;
    
    function mod($div){
      if(window.Groups) {        
        $div.data('isDragging', false);
        $div.draggable(window.Groups.dragOptions);
        $div.droppable(window.Groups.dropOptions);
      }
      
      $div.mousedown(function(e) {
        if(!Utils.isRightClick(e))
          self.lastMouseDownTarget = e.target;
      });
        
      $div.mouseup(function(e) { 
        var same = (e.target == self.lastMouseDownTarget);
        self.lastMouseDownTarget = null;
        if(!same)
          return;
          
        if(e.target.className == "close") {
          $(this).find("canvas").data("link").tab.close(); }
        else {
          if(!$(this).data('isDragging')) {

            // ZOOM! 
            var orig = {
              width: $(this).width(),
              height:  $(this).height(),
              pos: $(this).position()
            }

            var scale = window.innerWidth/orig.width;
            
            var tab = Tabs.tab(this);
            var mirror = tab.mirror;
            
            var overflow = $("body").css("overflow");
            $("body").css("overflow", "hidden");
            
            function onZoomDone(){
              $(this).find("canvas").data("link").tab.focus();
              $(this).css({
                top:   orig.pos.top,
                left:  orig.pos.left,
                width: orig.width,
                height:orig.height,
                })
                .removeClass("front");  
              Navbar.show();    
              $("body").css("overflow", overflow);              
            }
  
            $(this)
              .addClass("front")
              .animate({
                top:    -10,
                left:   0,
                easing: "easein",
                width:  orig.width*scale,
                height: orig.height*scale
                }, 200, onZoomDone);
            
          } else {
            $(this).find("canvas").data("link").tab.raw.pos = $(this).position();
          }
        }
      });
      
      $("<div class='close'><img src='img/closetab.png'/></div>").appendTo($div);
      $("<div class='expander'><img src='img/expand.png'/></div>").appendTo($div);
  
      if($div.length == 1) {
        var p = Page.findOpenSpaceFor($div); // TODO shouldn't know about Page
        $div.css({left: p.x, top: p.y});
      }
      
      $div.each(function() {
        var tab = Tabs.tab(this);
        $(this).data('tabItem', new TabItem(this, tab));
      });
      
      // TODO: Figure out this really weird bug?
      // Why is that:
      //    $div.find("canvas").data("link").tab.url
      // returns chrome://tabcandy/content/candies/original/index.html for
      // every $div (which isn't right), but that
      //   $div.bind("test", function(){
      //      var url = $(this).find("canvas").data("link").tab.url;
      //   });
      //   $div.trigger("test")
      // returns the right result (i.e., the per-tab URL)?
      // I'm so confused...
      // Although I can use the trigger trick, I was thinking about
      // adding code in here which sorted the tabs into groups.
      // -- Aza
    }
    
    window.TabMirror.customize(mod);
  }
};

TabItems.init();