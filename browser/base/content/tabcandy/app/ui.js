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
    var isDragging = false;
    
    var zIndex = 100;
    function mod($div){
      $div.draggable({
        start:function(){ isDragging = true; },
        stop: function(){
          isDragging = false;
          $(this).css({zIndex: zIndex});
          zIndex += 1;
        },
        zIndex: 999,
      }).mouseup(function(e){
        if( e.target.className == "close" ){
          $(this).find("canvas").data("link").tab.close(); }
        else {
          if( !isDragging ){
            Navbar.show();
            $(this).find("canvas").data("link").tab.focus();            
          } else {
            $(this).find("canvas").data("link").tab.raw.pos = $(this).position();
          }
        }
      });
      
      $("<div class='close'>x</div>").appendTo($div)
        
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
      var $found = $(".tab .name:not(:icontains(" + $search.val() + "))").parent();
      if( $search.val().length > 1 ){
        $found.addClass("unhighlight");
        $(".tab .name:icontains(" + $search.val() + ")").parent().removeClass("unhighlight");      
      }
      else {
        $(".tab .name").parent().removeClass("unhighlight");
      }
      
      console.log( $found.length );
      if ( $found.length == 1 ) {      
        $found.animate({top:0, left:0})
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
window.aza = ArrangeClass


})();

