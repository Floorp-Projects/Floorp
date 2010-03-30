(function(){

// ##########
Navbar = {
  get el(){
    var win = Utils.activeWindow;
    var navbar = win.gBrowser.ownerDocument.getElementById("navigator-toolbox");
    return navbar;      
  },
  show: function(){ this.el.collapsed = false; },
  hide: function(){ this.el.collapsed = true;}
}

// ##########
var Tabbar = {
  get el(){ return window.Tabs[0].raw.parentNode; },
  hide: function(){ this.el.collapsed = true },
  show: function(){ this.el.collapsed = false }
}

// ##########
window.Page = {
  startX: 30, 
  startY: 70, 
  tabWidth: 160,
  tabHeight: 137, 
  
  init: function() {    
    Utils.homeTab.raw.maxWidth = 60;
    Utils.homeTab.raw.minWidth = 60;
    
    Tabs.onClose(function(){
      Utils.homeTab.focus();
      return false;
    });
    
    var lastTab = null;
    Tabs.onFocus(function(){
      // If we switched to TabCandy window...
      if( this.contentWindow == window && lastTab != null && lastTab.mirror != null){
        // If there was a lastTab we want to animate
        // its mirror for the zoom out.
        var $tab = $(lastTab.mirror.el);
        
        var [w,h, pos, z] = [$tab.width(), $tab.height(), $tab.position(), $tab.css("zIndex")];
        var scale = window.innerWidth / w;

        var overflow = $("body").css("overflow");
        $("body").css("overflow", "hidden");
        
        var mirror = lastTab.mirror;
        $tab.css({
            top: 0, left: 0,
            width: window.innerWidth,
            height: h * (window.innerWidth/w),
            zIndex: 999999,
        }).animate({
            top: pos.top, left: pos.left,
            width: w, height: h
        },250, '', function() {
          $tab.css("zIndex",z);
          $("body").css("overflow", overflow);
        });
      }
      lastTab = this;
    });
    
    $("#tabbar").toggle(
      function(){Tabbar.hide()},
      function(){Tabbar.show()}      
    );
  },
  
  findOpenSpaceFor: function($div) {
    var w = window.innerWidth;
    var h = 0;
    var bufferX = 30;
    var bufferY = 30;
    var rects = [];
    var r;
    var $el;
    $(".tab:visible").each(function(i) {
      if(this == $div.get(0))
        return;
        
      $el = $(this);
      r = {x: parseInt($el.css('left')),
        y: parseInt($el.css('top')),
        w: parseInt($el.css('width')) + bufferX,
        h: parseInt($el.css('height')) + bufferY};
        
      if(r.x + r.w > w)
        w = r.x + r.w;

      if(r.y + r.h > h)
        h = r.y + r.h;
  
      rects.push(r);
    });

    if(!h) 
      return { 'x': this.startX, 'y': this.startY };
      
    var canvas = document.createElement('canvas');
    $(canvas).attr({width:w,height:h});

    var ctx = canvas.getContext('2d');
    ctx.fillStyle = 'rgb(255, 255, 255)';
    ctx.fillRect(0, 0, w, h);
    ctx.fillStyle = 'rgb(0, 0, 0)';
    var count = rects.length;
    var a;
    for(a = 0; a < count; a++) {
      r = rects[a];
      ctx.fillRect(r.x, r.y, r.w, r.h);
    }
    
    var divWidth = parseInt($div.css('width')) + bufferX;
    var divHeight = parseInt($div.css('height')) + bufferY;
    var strideX = divWidth / 4;
    var strideY = divHeight / 4;
    var data = ctx.getImageData(0, 0, w, h).data;

    function isEmpty(x1, y1) {
      return (x1 >= 0 && y1 >= 0
          && x1 < w && y1 < h
          && data[(y1 * w + x1) * 4]);
    }

    function isEmptyBox(x1, y1) {
      return (isEmpty(x1, y1) 
          && isEmpty(x1 + (divWidth - 1), y1)
          && isEmpty(x1, y1 + (divHeight - 1))
          && isEmpty(x1 + (divWidth - 1), y1 + (divHeight - 1)));
    }
    
    for(var y = this.startY; y < h; y += strideY) {
      for(var x = this.startX; x < w; x += strideX) {
        if(isEmptyBox(x, y)) {
          for(; y > this.startY + 1; y--) {
            if(!isEmptyBox(x, y - 1))
              break;
          }

          for(; x > this.startX + 1; x--) {
            if(!isEmptyBox(x - 1, y))
              break;
          }

          return { 'x': x, 'y': y };
        }
  	  }
    }

    return { 'x': this.startX, 'y': h };        
  }
}


//----------------------------------------------------------
function ArrangeClass(name, func){ this.init(name, func); };
ArrangeClass.prototype = {
  init: function(name, func){
    this.$el = this._create(name);
    this.arrange = func;
    if(func) this.$el.click(func);
  },
  
  _create: function(name){
    return $("<a class='action' href='#'/>").text(name).appendTo("#actions");
  }
}


//----------------------------------------------------------
var grid = new ArrangeClass("Grid", function(value) {
  if(typeof(Groups) != 'undefined')
    Groups.removeAll();

  var immediately = false;
  if(typeof(value) == 'boolean')
    immediately = value;

  var box = new Rect(Page.startX, Page.startY, Page.tabWidth, Page.tabHeight); 
  $(".tab:visible").each(function(i){
    var item = Items.item(this);
    item.setBounds(box, immediately);
    
    box.left += box.width + 30;
    if( box.left > window.innerWidth - (box.width + Page.startX)){ // includes allowance for the box shadow
      box.left = Page.startX;
      box.top += box.height + 30;
    }
  });
});
    
//----------------------------------------------------------
var site = new ArrangeClass("Site", function() {
  Groups.removeAll();
  
  var groups = [];
  $(".tab:visible").each(function(i) {
    $el = $(this);
    var tab = Tabs.tab(this);
    
    var url = tab.url; 
    var domain = url.split('/')[2]; 
    var domainParts = domain.split('.');
    var mainDomain = domainParts[domainParts.length - 2];
    if(groups[mainDomain]) 
      groups[mainDomain].push($(this));
    else 
      groups[mainDomain] = [$(this)];
  });
  
  var createOptions = {dontPush: true, dontArrange: true};
  
  var leftovers = [];
  for(key in groups) {
    var set = groups[key];
    if(set.length > 1) {
      group = new Group(set, createOptions);
    } else
      leftovers.push(set[0]);
  }
  
/*   if(leftovers.length > 1) { */
    group = new Group(leftovers, createOptions);
/*   } */
  
  Groups.arrange();
});

//----------------------------------------------------------
var Arrange = {
  initialized: false,

  init: function(){
    this.initialized = true;
    grid.arrange(true);
  }
}

//----------------------------------------------------------
function UIClass(){ this.init(); };
UIClass.prototype = {
  navbar: Navbar,
  tabbar: Tabbar,
  init: function(){
    Page.init();
    Arrange.init();
  }
}

//----------------------------------------------------------
var UI = new UIClass();
window.UI = UI;

})();

