// ##########
window.TabItem = function(container, tab) {
  this.container = container;
  this.tab = tab;
  
  this.defaultSize = new Point(TabItems.tabWidth, TabItems.tabHeight);
  
  $(this.container).data('item', this);
};

window.TabItem.prototype = $.extend(new Item(), {
  // ----------
  getContainer: function() {
    return this.container;
  },
  
  // ----------
  getBounds: function() {
    return Utils.getBounds(this.container);
  },
  
  // ----------  
  setBounds: function(rect, immediately) {
    var css = $.extend(this._getSizeCSS(rect.width, rect.height), {
      left: rect.left,
      top: rect.top,
    });
    
    this._change(css, immediately);
  },
  
  // ----------
  setPosition: function(left, top, immediately) {
    this._change({left: left, top: top}, immediately);
  },

  // ----------  
  setSize: function(width, height, immediately) {
    this._change(this._getSizeCSS(width, height), immediately);
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
  },
  
  // ----------  
  _getSizeCSS: function(width, height) {
    var scale = width / TabItems.tabWidth;
    
    return {
      width: width,
      height: height,
      fontSize: TabItems.fontSize * scale
    };
  },
  
  // ----------  
  _change: function(css, immediately) {
    if(immediately) 
      $(this.container).css(css);
    else {
      TabMirror.pausePainting();
      $(this.container).animate(css, {complete: function() {
        TabMirror.resumePainting();
      }}).dequeue();
    }
  }
});

// ##########
window.TabItems = {
  tabWidth: 160,
  tabHeight: 137, 
  fontSize: 9,

  init: function() {
    var self = this;
    
    function mod($div){
      Utils.log('mod');
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
            var ffVersion = parseFloat(navigator.userAgent.match(/\d{8}.*(\d\.\d)/)[1]);
            if( ffVersion < 3.7 ) Utils.error("css-transitions require Firefox 3.7+");
            
            // ZOOM! 
            var [w,h,z] = [$(this).width(), $(this).height(), $(this).css("zIndex")];
            var origPos = $(this).position();
            var zIndex = $(this).css("zIndex");
            var scale = window.innerWidth/w;
            
            var tab = Tabs.tab(this);
            var mirror = tab.mirror;
            //mirror.forceCanvasSize(w * scale/3, h * scale);
            
            var overflow = $("body").css("overflow");
            $("body").css("overflow", "hidden");
  
            $(this).css("zIndex",99999).animate({
              top: -10, left: 0, easing: "easein",
              width:w*scale, height:h*scale}, 200, function(){
                $(this).find("canvas").data("link").tab.focus();
                $(this)
                  .css({top: origPos.top, left: origPos.left, width:w, height:h, zIndex:z});  
                Navbar.show();    
                $("body").css("overflow", overflow);          
              });
            
          } else {
            $(this).find("canvas").data("link").tab.raw.pos = $(this).position();
          }
        }
      });
      
      $("<div class='close'>x</div>").appendTo($div);
  
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