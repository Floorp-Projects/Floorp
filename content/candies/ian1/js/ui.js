(function(){

$.expr[':'].icontains = function(obj, index, meta, stack){
return (obj.textContent || obj.innerText || jQuery(obj).text() || '').toLowerCase().indexOf(meta[3].toLowerCase()) >= 0;
};

Navbar = {
  get el(){
    var win = Utils.activeWindow;
    var navbar = win.gBrowser.ownerDocument.getElementById("navigator-toolbox");
    return navbar;      
  },
  show: function(){ this.el.collapsed = false; },
  hide: function(){ this.el.collapsed = true;}
}

var Tabbar = {
  get el(){ return window.Tabs[0].raw.parentNode; },
  hide: function(){ this.el.collapsed = true },
  show: function(){ this.el.collapsed = false }
}

var Page = {
  init: function(){
    function mod($div, tab){
      if(window.Groups) {        
        $div.draggable(window.Groups.dragOptions);
        $div.droppable(window.Groups.dropOptions);
      }
        
      $div.mouseup(function(e){
        if( e.target.className == "close" ){
          $(this).find("canvas").data("link").tab.close(); }
        else {
          if(!$(this).data('isDragging')) {
            Navbar.show();
            $(this).find("canvas").data("link").tab.focus();            
          } else {
            $(this).find("canvas").data("link").tab.raw.pos = $(this).position();
          }
        }
      });
      
      $("<div class='close'>x</div>").appendTo($div);
      
      if(Arrange.initialized) {
        var p = Page.findOpenSpaceFor($div);
        $div.css({left: p.x, top: p.y});
      }
    }
    
    window.TabMirror.customize(mod);
    
    Utils.homeTab.raw.maxWidth = 60;
    Utils.homeTab.raw.minWidth = 60;
    
    Tabs.onClose(function(){
      Utils.homeTab.focus();
      return false;
    })
    
    $("#tabbar").toggle(
      function(){Tabbar.hide()},
      function(){Tabbar.show()}      
    )
    
    Page.initSearch();
  },
  
  initSearch: function(){
    $search = $(".search input");
    $search.val("").focus();
    $search.keydown(function(evt){
           
      if( evt.which == 13 ){
                
        Navbar.show();
        
        if ($(".tab:not(.unhighlight)").length == 1) {
          $(".tab:not(.unhighlight)").find("canvas").data("link").tab.focus();
        } else {
          Tabs.open( "http://google.com/search?q=" + $search.val() ).focus();
        }
        $search.val("")          
        $(".tab .name").parent().removeClass("unhighlight");
        
      }
    });
    
    $search.keyup(function(evt){
      if( $search.val().length > 0 )
        var $found = $(".tab .name:not(:icontains(" + $search.val() + "))").parent();
      else 
        var $found = [];
      
      if( $search.val().length > 1 ){
        $found.addClass("unhighlight");
        $(".tab .name:icontains(" + $search.val() + ")").parent().removeClass("unhighlight");      
      }
      else {
        $(".tab .name").parent().removeClass("unhighlight");
      }
      
      if ( $found.length == 1 ) {
        $found.click();
        //$found.animate({top:0, left:0})
      }  
      
    });
    
    Utils.homeTab.onFocus(function(){
      $search.val("").focus();
      Navbar.hide();
    });
    
    $(window).blur(function(){
      Navbar.show();
    })
  },
  
  findOpenSpaceFor: function($div) {
    var w = window.innerWidth;
    var h = 0;
    var startX = 30;
    var startY = 100;
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
      return { 'x': startX, 'y': startY };
      
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
    
    for(var y = startY; y < h; y += strideY) {
      for(var x = startX; x < w; x += strideX) {
        if(isEmptyBox(x, y)) {
          for(; y > startY + 1; y--) {
            if(!isEmptyBox(x, y - 1))
              break;
          }

          for(; x > startX + 1; x--) {
            if(!isEmptyBox(x - 1, y))
              break;
          }

          return { 'x': x, 'y': y };
        }
      }
    }

    return { 'x': startX, 'y': h };        
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
var anim = new ArrangeClass("Anim", function(){
  if( $("canvas:visible").eq(9).height() < 300 )
    $("canvas:visible").eq(9).data("link").animate({height:500}, 500);
  else
    $("canvas:visible").eq(9).data("link").animate({height:120}, 500);
    
  $("canvas:visible").eq(9).css({zIndex:99999});
})

//----------------------------------------------------------
var grid = new ArrangeClass("Grid", function(value) {
  var immediately = false;
  if(typeof(value) == 'boolean')
    immediately = value;

  var startX = 30;         
  var x = startX;
  var y = 100;
  $(".tab:visible").each(function(i){
    $el = $(this);
    
    if(immediately)
      $el.css({top: y,left: x});
    else {
      TabMirror.pausePainting();
      $el.animate({top: y,left: x}, 500, null, function() {
        TabMirror.resumePainting();
      });
    }
      
    x += $el.width() + 30;
    if( x > window.innerWidth - ($el.width() + startX)){ // includes allowance for the box shadow
      x = startX;
      y += $el.height() + 30;
    }
  });
});

//----------------------------------------------------------
var stack = new ArrangeClass("Stack", function() {
  var startX = 30;
  var startY = 100;
  var x = startX;
  var y = startY;
  $(".tab:visible").each(function(i){
    $el = $(this);
    TabMirror.pausePainting();
    $el.animate({'top': y, 'left': x, '-moz-transform': 'rotate(40deg)'}, 500, null, function() {
      TabMirror.resumePainting();
    });
  });
});

//----------------------------------------------------------
var site = new ArrangeClass("Site", function() {
  var startX = 30;
  var startY = 100;
  var x = startX;
  var y = startY;
  var x2; 
  var y2;
  var groups = [];
  $(".tab:visible").each(function(i) {
    $el = $(this);
    var tab = Tabs.tab(this);
    
    var group = $el.data('group');
    if(group)
      group.remove(this);
      
    var url = tab.url; 
    var domain = url.split('/')[2]; 
    var domainParts = domain.split('.');
    var mainDomain = domainParts[domainParts.length - 2];
    if(groups[mainDomain]) 
      groups[mainDomain].push(this);
    else 
      groups[mainDomain] = [this];
  });
  
  var leftovers = [];
  for(key in groups) {
    var set = groups[key];
    if(set.length > 1) {
      group = new Groups.Group();
      group.create(set);            
    } else
      leftovers.push(set[0]);
  }
  
  if(leftovers.length > 1) {
    group = new Groups.Group();
    group.create(leftovers);            
  }
  
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

