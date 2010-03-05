function Selector(options){ this.init(options) }
Selector.prototype = {
  init: function(options){
    var self = this;
    options.onSelect = function(a,b){ self.showMenu(a,b); };
    options.onStart = function(){ self.hideMenu() };
/*     options.onMove = function(a){ self.updateSelection(a) }; */
    options.acceptMouseDown = function(a){ return self.acceptMouseDown(a); };
    this.lasso = new Lasso(options);
  },
  
  hideMenu: function(){
    if( this.menu ) this.menu.remove();
  },
  
  showMenu: function( selectedEls, pos ){
    if( pos == null || selectedEls.length == 0 ) return;
    var self = this;
    
    self.updateSelection(selectedEls);
    
    this.menu = $("<div class='lasso-menu'>").appendTo("body");
    this.menu.css({
      position:"fixed",
      zIndex: 9999,
      top:pos.y-this.menu.height()/2,
      left:pos.x-this.menu.width()/2
    })
    .text("Group")
    .mouseout( function(){self.hideMenu()} )
    .mousedown( function(){group(selectedEls)} )
  }, 
  
  updateSelection: function(els) {
	  for( var i=0; i<els.length; i++){
	    var el = els[i];
	    $(el).css("-moz-box-shadow", "1px 1px 10px rgba(0,0,0,.5)");
	  }
  },

  acceptMouseDown: function(e) {
  	if( $(e.target).is('#nav') 
  	    || $(e.target).parents('#nav').length > 0) 
      return false;
      
    return true;
  }
}

function group(els){
  var startEl = $(els[0]);
  startEl.css("z-index", 0);
  for( var i=1; i<els.length; i++){
    var el = els[i];
    var pos = startEl.position();
    $(el).css("z-index", i*10);
    $(el).animate({top: pos.top+i*15, left: pos.left+i*15, position: "absolute"}, 250);
  }
}


$(function(){
  var select = new Selector({
    targets: ".tab",
    container: "html",
  });
});