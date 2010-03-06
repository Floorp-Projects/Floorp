(function(){
/* Utils.log('top of tab mirror file'); */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

function _isIframe(doc){
  var win = doc.defaultView;
  return win.parent != win;
}


var TabCanvas = function(tab, canvas){ this.init(tab, canvas) }
TabCanvas.prototype = {
  init: function(tab, canvas){
    this.tab = tab;
    this.canvas = canvas;
    this.window = window;
        
    this.RATE_LIMIT = 250; // To refresh the thumbnail any faster than this. In ms.
    this.lastDraw = null;
    
    $(canvas).data("link", this);

    var w = $(canvas).width();
    var h = $(canvas).height();
    $(canvas).attr({width:w, height:h});
    
    this.paint(null);
    
    var self = this;
    var paintIt = function(evt){self.onPaint(evt) };
    
    // Don't mirror chrome tabs.
    //if( window.location.protocol == "chrome:" ) return;
    
/*     Utils.log('attaching', tab.contentWindow.location.href); */
    tab.contentWindow.addEventListener("MozAfterPaint", paintIt, false);

/*
    tab.contentDocument.addEventListener("onload", function() { 
      Utils.log('onload', this.tab.contentWindow.location.href);
    }, false);
*/
    
    $(window).unload(function(){
      tab.contentWindow.removeEventListener("MozAfterPaint", paintIt, false);
    })
  },

  
  paint: function(evt){
    var $ = this.window.$;
    if( $ == null ) return;
    var $canvas = $(this.canvas);
    var ctx = this.canvas.getContext("2d");
  
    var w = $canvas.width();
    var h = $canvas.height();
  
    var fromWin = this.tab.contentWindow;
    if( fromWin == null || fromWin.location.protocol == "chrome:") return;
/*     Utils.trace('paint: ' + this.tab.url); */
    var scaler = w/fromWin.innerWidth;
  
    // TODO: Potentially only redraw the dirty rect? (Is it worth it?)
    var now = new Date();
    if( this.lastDraw == null || now - this.lastDraw > this.RATE_LIMIT ){
      var startTime = new Date();
      ctx.save();
      ctx.scale(scaler, scaler);
      try{
        ctx.drawWindow( fromWin, fromWin.scrollX, fromWin.scrollY, w/scaler, h/scaler, "#fff" );
      } catch(e){
        
      }
      
      ctx.restore();
      var elapsed = (new Date()) - startTime;
      //Utils.log( this.window.location.host + " " + elapsed );
      this.lastDraw = new Date();
    }
    ctx.restore();      
  },
  
  onPaint: function(evt){
/*
    if(this.tab.contentWindow == null || this.tab.contentWindow.location.protocol != 'chrome:')
      Utils.trace('on paint', this.tab.contentWindow.location.href);
*/
    this.paint(evt);    
  },
  
  animate: function(options, duration){
/*
    if(this.tab.contentWindow == null || this.tab.contentWindow.location.protocol != 'chrome:')
      Utils.log('on animate', this.tab.contentWindow.location.href);
*/
    var self = this;
    if( duration == null ) duration = 0;
        
    var $canvas = $(this.canvas);
    var w = $canvas.width();
    var h = $canvas.height();
    
    var newW = (w/h)*options.height;
    var newH = options.height;
    
    $canvas.width(w);
    $canvas.height(h);    
    $canvas.animate({width:newW, height:newH}, duration, function(){
      $canvas.attr("width", newW);
      $canvas.attr("height", newH);
      self.paint(null);      
    } );
    this.paint(null);
  }
}

var TabMirror = function( ){ this.init() }
TabMirror.prototype = {
  init: function(){
/*   	Utils.log('creating tab mirror'); */
    var self = this;
    
    // When a tab is updated, update the mirror
    Tabs.onReady( function(evt){
      self.update(evt.tab);
    });
    
    // When a tab is closed, unlink.    
    Tabs.onClose( function(){
      self.unlink(this);
    });
    
    // For each tab, create the link.
    Tabs.forEach(function(tab){
      self.link(tab);
    });    
    
  },
  
  _getEl: function(tab){
    mirror = null;
    $(".tab").each(function(){
      if( $(this).data("tab") == tab ){
        mirror = this;
        return;
      }
    });
    return mirror;
  },
  
  _customize: function(func){
    // pass
    // This gets set by add-ons/extensions to MirrorTab
  },
  
  _createEl: function(tab){
    var div = $("<div class='tab'><span class='name'>&nbsp;</span><img class='fav'/><canvas class='thumb'/></div>")
      .data("tab", tab)
      .appendTo("body");
      
    if( tab.url.match("chrome:") ){
      div.hide();
    }     
    
    this._customize(div);

    function updateAttributes(){
      var iconUrl = tab.raw.linkedBrowser.mIconURL;
      var label = tab.raw.label;
      $fav = $('.fav', div)
      $name = $('.name', div);
      
      if(iconUrl != $fav.attr("src")) $fav.attr("src", iconUrl);
      if( $name.text() != label ) $name.text(label);
    }    
    
    var timer = setInterval( updateAttributes, 500 );
    div.data("timer", timer);
    
    this._updateEl(tab);
  },
  
  _updateEl: function(tab){
    var el = this._getEl(tab);
    
    new TabCanvas(tab, $('.thumb', el).get(0) );    
  },
  
  update: function(tab){
    var doc = tab.contentDocument;
    this.link(tab);

    if( !_isIframe(doc) ){
      this._updateEl(tab);
    }
  },
  
  link: function(tab){
/*   	Utils.trace('link'); */
/*   	Utils.log('link: ' + tab.url); */
    // Don't add duplicates
    var dup = this._getEl(tab)
    if( dup ) return false;
    
    /*// Don't do anything that starts with a chrome URL
    if( tab.contentWindow.location.protocol == "chrome:" ){
      return false;
    }*/
    
    // Add the tab to the page
    this._createEl(tab);
    return true;
  },
  
  unlink: function(tab){
    $(".tab").each(function(){
      if( $(this).data("tab") == tab ){
        clearInterval( $(this).data("timer") );
        $(this).remove();
      }
    });    
  }
  
}

new TabMirror()
window.TabMirror = {}
window.TabMirror.customize = function(func){
  // Apply the custom handlers to all existing elements
  // TODO: Make this modular: so that it only exists in one place.
  //       No breaking DRY!
  func($("div.tab"));
  
  // Apply it to all future elements.
  TabMirror.prototype._customize = func;
};

})();
