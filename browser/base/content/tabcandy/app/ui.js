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
            // ZOOM!          
            var [w,h] = [$(this).width(), $(this).height()];
            var origPos = $(this).position();
            var scale = window.innerWidth/w;
            
            $(this).addClass("scale-animate").css({
              top: 0, left: 0,
              width:w*scale, height:h*scale
            }).bind("transitionend", function(e){
              // We will get one of this events for every property CSS-animated...
              // I choose one randomly (width) and only do things for that.
              if( e.originalEvent.propertyName != "width" ) return;

              // Switch tabs, and the re-size and re-position the animated
              // tab image.
              $(this).find("canvas").data("link").tab.focus();
              $(this)
                .removeClass("scale-animate")
                .css({top: origPos.top, left: origPos.left, width:w, height:h});
              Navbar.show();
            })
            // END ZOOM
            
          } else {
            $(this).find("canvas").data("link").tab.raw.pos = $(this).position();
          }
        }
      });
      
      $("<div class='close'>x</div>").appendTo($div);
      
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
  }
}


function ArrangeClass(name, func){ this.init(name, func); };
ArrangeClass.prototype = {
  init: function(name, func){
    this.$el = this._create(name);
    this.arrange = func;
    if(func) this.$el.click(func);
  },
  
  _create: function(name){
    return $("<a href='#'/>").text(name).appendTo("#actions");
  }
}

var anim = new ArrangeClass("Anim", function(){
  if( $("canvas:visible").eq(9).height() < 300 )
    $("canvas:visible").eq(9).data("link").animate({height:500}, 500);
  else
    $("canvas:visible").eq(9).data("link").animate({height:120}, 500);
    
  $("canvas:visible").eq(9).css({zIndex:99999});
})

var grid = new ArrangeClass("Grid", function(){    
  var x = 10;
  var y = 100;
  $(".tab:visible").each(function(i){
    $el = $(this);
    
    var oldPos = $el.find("canvas").data("link").tab.raw.pos;
    if( oldPos ){
      $el.css({top:oldPos.top, left:oldPos.left});
      return;
    }
    
    $el.css({top: y,left: x});
    x += $el.width() + 10;
    if( x > window.innerWidth - $el.width() ){
      x = 10;
      y += $el.height() + 30;
    }
    
    
  });
});



var Arrange = {
  init: function(){
    grid.arrange();
    
  }
}

function UIClass(){ this.init(); };
UIClass.prototype = {
  navbar: Navbar,
  tabbar: Tabbar,
  init: function(){
    Page.init();
    Arrange.init();

  }
}

var UI = new UIClass();
window.UI = UI;


})();

