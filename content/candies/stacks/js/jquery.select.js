function Selector(options){ this.init(options) }
Selector.prototype = {
  init: function(options){
    var self = this;
    options.onSelect = function(a,b){ self.showMenu(a,b); };
    options.onStart = function(){ self.hideMenu(250) };
/*     options.onMove = function(a){ self.updateSelection(a) }; */
    options.acceptMouseDown = function(a){ return self.acceptMouseDown(a); };
    this.lasso = new Lasso(options);
  },
  
  startFadeOutTimer: function() {
    var self = this;
    this.timeout = setTimeout(function() { 
      self.timeout = null;
      self.hideMenu(2000); 
    }, 1000);
  }, 
  
  cancelFadeOutTimer: function() {
    if(this.timeout) {
      clearTimeout(this.timeout);
      this.timeout = null;
    }
  },
    
  hideMenu: function(time) {
    var self = this;
    if(this.menu) {
      this.menu.fadeOut(time, function() {
        if(self.menu) {
          self.menu.remove();
          self.menu = null;
        }
      });
    }
    
    this.cancelFadeOutTimer();
  },
  
  showMenu: function( selectedEls, pos ){
    if( pos == null || selectedEls.length == 0 ) return;
    var self = this;
    
    this.updateSelection(selectedEls);
    this.cancelFadeOutTimer();
    
    this.menu = $("<div class='lasso-menu'>").appendTo("body");
    this.menu.css({
      position:"fixed",
      zIndex: 9999,
      top:pos.y-this.menu.height()/2,
      left:pos.x-this.menu.width()/2
    });
    
    this.menu.mouseover(function() { 
      self.menu.stop();
      self.menu.css({'opacity':1}); 
      self.cancelFadeOutTimer(); 
    });
    
    this.menu.mouseout(function() {
      self.startFadeOutTimer(); 
    });
    
    $('<div>Group</div>')
      .appendTo(this.menu)
      .mousedown(function() {
        group(selectedEls);
      });
       
    $('<div>Close</div>')
      .appendTo(this.menu)
      .mousedown(function() {
        close(selectedEls);
      }); 
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
    $(el).animate({top: pos.top, left: pos.left, position: "absolute"}, 250);
  }
}

function close(els){
  for( var i=0; i<els.length; i++){
    var el = els[i];
    var tab = Tabs.tab(el);
    tab.close();
  }
}


$(function(){
  var select = new Selector({
    targets: ".tab",
    container: "html",
  });
});