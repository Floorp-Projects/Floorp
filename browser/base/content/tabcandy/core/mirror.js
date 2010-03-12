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

function getMilliseconds() {
	var date = new Date();
	return date.getTime();
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
    
/*     this.paint(null); */
    
    var self = this;
    this.paintIt = function(evt) { 
      self.onPaint(evt); 
    };
    
    // Don't mirror chrome tabs.
    //if( window.location.protocol == "chrome:" ) return;
    
/*     Utils.log('attaching', tab.contentWindow.location.href); */
    tab.contentWindow.addEventListener("MozAfterPaint", this.paintIt, false);

/*
    tab.contentDocument.addEventListener("onload", function() { 
      Utils.log('onload', this.tab.contentWindow.location.href);
    }, false);
*/
    
    $(window).unload(function() {
      self.detach();
    });
  },
  
  detach: function() {
    this.tab.contentWindow.removeEventListener("MozAfterPaint", this.paintIt, false);
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
/*     Utils.log('paint: ' + this.tab.url); */
    var scaler = w/fromWin.innerWidth;
  
    // TODO: Potentially only redraw the dirty rect? (Is it worth it?)
    var now = getMilliseconds();
/*     Utils.log('now', now - this.lastDraw); */
    if( this.lastDraw == null || now - this.lastDraw > this.RATE_LIMIT ){
      var startTime = getMilliseconds();
      ctx.save();
      ctx.scale(scaler, scaler);
      try{
        ctx.drawWindow( fromWin, fromWin.scrollX, fromWin.scrollY, w/scaler, h/scaler, "#fff" );
      } catch(e){
        
      }
      
      ctx.restore();
      var elapsed = (getMilliseconds()) - startTime;
      //Utils.log( this.window.location.host + " " + elapsed );
      this.lastDraw = getMilliseconds();
    }
    ctx.restore();      
  },
  
  onPaint: function(evt){
/*
    if(this.tab.contentWindow == null || this.tab.contentWindow.location.protocol != 'chrome:')
      Utils.trace('on paint', this.tab.contentWindow.location.href);
*/
/*     this.paint(evt);     */
    
    if(!this.tab.mirror.needsPaint) {
      if(this.tab.contentWindow == null || this.tab.contentWindow.location.protocol != 'chrome:')
        this.tab.mirror.needsPaint = getMilliseconds();
    }
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
    
/*     Utils.log(Tabs); */
    
    Tabs.onOpen(function(evt) {
      Utils.log('mirror onOpen', evt.tab.url);
    });
    
    Tabs.onLoad(function(evt) {
      Utils.log('mirror onLoad', evt.tab.url);
    });

    // When a tab is updated, update the mirror
    Tabs.onReady( function(evt){
      Utils.log('mirror onready');
      self.update(evt.tab);
    });
    
    // When a tab is closed, unlink.    
    Tabs.onClose( function(){
      Utils.log('mirror onclose');
      self.unlink(this);
    });
    
    // For each tab, create the link.
    Tabs.forEach(function(tab){
      self.link(tab);
    });    
     
    this.heartbeatIndex = 0;  
    this._fireNextHeartbeat();    
  },
  
  _heartbeat: function() {
    try {
      var now = getMilliseconds();
      var count = Tabs.length;
      if(count) {
        this.heartbeatIndex++;
        if(this.heartbeatIndex >= count)
          this.heartbeatIndex = 0;
          
        var tab = Tabs[this.heartbeatIndex];
        var mirror = tab.mirror; 
        if(mirror && mirror.needsPaint) {
/*           Utils.log('needsPaint', now - mirror.needsPaint, tab.url); */
  /*
          var canvas = $('.thumb', mirror.el).get(0);
          var tabCanvas = $(canvas).data("link");
          tabCanvas.paint();
  */
          mirror.tabCanvas.paint();
  
          if(mirror.needsPaint + 5000 < now)
            mirror.needsPaint = 0;
        }
      }
    } catch(e) {
      Utils.error(e);
    }
    
    this._fireNextHeartbeat();
  },
  
  _fireNextHeartbeat: function() {
    var self = this;
    window.setTimeout(function() {
      self._heartbeat();
    }, 100);
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
/*     Utils.trace('_createEl'); */
    var div = $("<div class='tab'><span class='name'>&nbsp;</span><img class='fav'/><canvas class='thumb'/></div>")
      .data("tab", tab)
      .appendTo("body");
      
    if( tab.url.match("chrome:") ){
      div.hide();
    }     
    
    this._customize(div);
    
    tab.mirror = {}; 
    tab.mirror.needsPaint = 0;
    tab.mirror.el = div.get(0);
/*
    tab.mirror.favEl = $('.fav', div).get(0);
    tab.mirror.nameEl = $('.name', div).get(0);
*/

    var self = this;
    
    function updateAttributes(){
      var iconUrl = tab.raw.linkedBrowser.mIconURL;
      var label = tab.raw.label;
      $fav = $('.fav', div)
      $name = $('.name', div);
      
      if(iconUrl != $fav.attr("src")) $fav.attr("src", iconUrl);
      if( $name.text() != label ) {
        $name.text(label);
/*         self._updateEl(tab);  */
/*
        var canvas = $('.thumb', el).get(0);
        var tabCanvas = $(canvas).data("link");
        tabCanvas.paint();
*/
        
/*         Utils.trace('update', label); */
      }
    }    
    
    var timer = setInterval( updateAttributes, 500 );
    div.data("timer", timer);
    
    this._updateEl(tab);
  },
  
  _updateEl: function(tab){
/*     Utils.log('_udateEl', tab.url); */
    var mirror = tab.mirror;
/*
    if(mirror.tabCanvas)
      mirror.tabCanvas.detach();
*/
  
    if(!mirror.tabCanvas) {     
      var canvas = $('.thumb', mirror.el).get(0);
      mirror.tabCanvas = new TabCanvas(tab, canvas);    
    }
    
    mirror.needsPaint = getMilliseconds();
  },
  
  update: function(tab){
/*     Utils.log('update', tab.url); */
    var doc = tab.contentDocument;
    this.link(tab);

    if( !_isIframe(doc) ){
      this._updateEl(tab);
    }
  },
  
  link: function(tab){
/*   	Utils.log('link', tab.url); */
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
