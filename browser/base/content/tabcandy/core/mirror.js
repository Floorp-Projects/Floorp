// Title: mirror.js
(function(){

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

function _isIframe(doc){
  var win = doc.defaultView;
  return win.parent != win;
}

// ----------
var TabCanvas = function(tab, canvas){ this.init(tab, canvas) }
TabCanvas.prototype = {
  init: function(tab, canvas){
    this.tab = tab;
    this.canvas = canvas;
    this.window = window;
            
    $(canvas).data("link", this);

    var w = $(canvas).width();
    var h = $(canvas).height();
    $(canvas).attr({width:w, height:h});
      
    var self = this;
    this.paintIt = function(evt) { 
      // note that "window" is unreliable in this context, so we'd use self.window if needed
      self.tab.mirror.triggerPaint();
/*       self.window.Utils.log('paint it', self.tab.url); */
    };
  },
  
  attach: function() {
    this.tab.contentWindow.addEventListener("MozAfterPaint", this.paintIt, false);
  },
     
  detach: function() {
    try {
      this.tab.contentWindow.removeEventListener("MozAfterPaint", this.paintIt, false);
    } catch(e) {
      // ignore
    }
  },
  
  paint: function(evt){
    var $ = this.window.$;
    if( $ == null ) {
      Utils.log('null $ in paint');
      return;
    }
    
    var $canvas = $(this.canvas);
    var ctx = this.canvas.getContext("2d");
  
    var w = $canvas.attr('width');
    var h = $canvas.attr('height');
    if(!w || !h)
      return;
  
    var fromWin = this.tab.contentWindow;
    if(fromWin == null) {
      Utils.log('null fromWin in paint');
      return;
    }
    
    var scaler = w/fromWin.innerWidth;
  
    // TODO: Potentially only redraw the dirty rect? (Is it worth it?)

    ctx.save();
    ctx.scale(scaler, scaler);
    try{
      ctx.drawWindow( fromWin, fromWin.scrollX, fromWin.scrollY, w/scaler, h/scaler, "#fff" );
    } catch(e){
      Utils.error('paint', e);   
    }
    
    ctx.restore();
  },
  
  animate: function(options, duration){
    Utils.log('on animate', this.tab.contentWindow.location.href);

    // TODO: This doesn't seem to scale the rest of the interface elements at the same
    // width, leaving unfortunately long trails.

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

// ##########
// Class: Mirror
// A single tab in the browser and its visual representation in the tab candy window. 
// Note that it implements the <Subscribable> interface.
function Mirror(tab, manager) {
  this.tab = tab;
  this.manager = manager;
  
  var html = "<div class='tab'>" +
              "<div class='favicon'><img/></div>" +
              "<div class='thumb'><div class='thumbShadow'></div><canvas/></div>" +
              "<span class='tab-title'>&nbsp;</span>" +              
             "</div>";
             
  
  var div = $(html)
    .data("tab", this.tab)
    .appendTo("body");
      
  this.needsPaint = 0;
  this.canvasSizeForced = false;
  this.el = div.get(0);
  this.favEl = $('.favicon>img', div).get(0);
  this.nameEl = $('.tab-title', div).get(0);
  this.canvasEl = $('.thumb canvas', div).get(0);
      
  var doc = this.tab.contentDocument;
  if( !_isIframe(doc) ) {
    this.tabCanvas = new TabCanvas(this.tab, this.canvasEl);    
    this.tabCanvas.attach();
    this.triggerPaint();
  }
  
  this.tab.mirror = this;
  this.manager._customize(div);
}

Mirror.prototype = $.extend(new Subscribable(), {  
  // ----------
  // Function: triggerPaint
  // Forces the mirror in question to update its thumbnail. 
  triggerPaint: function() {
  	var date = new Date();
  	this.needsPaint = date.getTime();
  },
  
  // ----------
  // Function: foreceCanvasSize
  // Repaints the thumbnail with the given resolution, and forces it 
  // to stay that resolution until unforceCanvasSize is called. 
  forceCanvasSize: function(w, h) {
    this.canvasSizeForced = true;
    var $canvas = $(this.canvasEl);
    $canvas.attr('width', w);
    $canvas.attr('height', h);
    this.tabCanvas.paint();
  },
  
  // ----------
  // Function: unforceCanvasSize
  // Stops holding the thumbnail resolution; allows it to shift to the
  // size of thumbnail on screen. Note that this call does not nest, unlike
  // <TabMirror.resumePainting>; if you call forceCanvasSize multiple 
  // times, you just need a single unforce to clear them all. 
  unforceCanvasSize: function() {
    this.canvasSizeForced = false;
  }
});

// ##########
// Class: TabMirror
// A singleton that manages all of the <Mirror>s in the system.
var TabMirror = function( ){ this.init() }
TabMirror.prototype = {
  // ----------
  // Function: init
  // Set up the necessary tracking to maintain the <Mirror>s.
  init: function(){
    var self = this;
    
    // When a tab is opened, create the mirror
    Tabs.onOpen(function() { 
      var tab = this;
      setTimeout(function() { // Marshal event from chrome thread to DOM thread
        self.update(tab);
      }, 1);
    });

    // When a tab is updated, update the mirror
    Tabs.onReady( function(evt){
      var tab = evt.tab;
      setTimeout(function() { // Marshal event from chrome thread to DOM thread
        self.update(tab);
      }, 1);
    });
    
    // When a tab is closed, unlink.    
    Tabs.onClose( function(){
      var tab = this;
      setTimeout(function() { // Marshal event from chrome thread to DOM thread
        self.unlink(tab);
      }, 1);
    });
    
    // For each tab, create the link.
    Tabs.forEach(function(tab){
      self.link(tab);
    });    
     
    this.paintingPaused = 0;
    this.heartbeatIndex = 0;  
    this._fireNextHeartbeat();    
  },
  
  _heartbeat: function() {
    try {
      var now = Utils.getMilliseconds();
      var count = Tabs.length;
      if(count && this.paintingPaused <= 0) {
        this.heartbeatIndex++;
        if(this.heartbeatIndex >= count)
          this.heartbeatIndex = 0;
          
        var tab = Tabs[this.heartbeatIndex];
        var mirror = tab.mirror; 
        if(mirror) {
          var iconUrl = tab.raw.linkedBrowser.mIconURL;
          var label = tab.raw.label;
          $fav = $(mirror.favEl);
          $name = $(mirror.nameEl);
          $canvas = $(mirror.canvasEl);
          
          if(iconUrl != $fav.attr("src")) { 
            $fav.attr("src", iconUrl);
            mirror.triggerPaint();
          }
            
          if($name.text() != label) {
            $name.text(label);
            mirror.triggerPaint();
          }
          
          if(tab.url != mirror.url) {
            var oldURL = mirror.url;
            mirror.url = tab.url;
            mirror._sendToSubscribers('urlChanged', {oldURL: oldURL, newURL: tab.url});
            mirror.triggerPaint();
          }
          
          if(!mirror.canvasSizeForced) {
            var w = $canvas.width();
            var h = $canvas.height();
            if(w != $canvas.attr('width') || h != $canvas.attr('height')) {
              $canvas.attr('width', w);
              $canvas.attr('height', h);
              mirror.triggerPaint();
            }
          }
          
          if(mirror.needsPaint) {
            mirror.tabCanvas.paint();
            
            if(Utils.getMilliseconds() - mirror.needsPaint > 5000)
              mirror.needsPaint = 0;
          }
        }
      }
    } catch(e) {
      Utils.error('heartbeat', e);
    }
    
    this._fireNextHeartbeat();
  },
  
  _fireNextHeartbeat: function() {
    var self = this;
    window.setTimeout(function() {
      self._heartbeat();
    }, 100);
  },   
    
  _customize: function(func){
    // pass
    // This gets set by add-ons/extensions to MirrorTab
  },
  
  _createEl: function(tab){
    new Mirror(tab, this); // sets tab.mirror to itself
  },
  
  update: function(tab){
    this.link(tab);

    if(tab.mirror && tab.mirror.tabCanvas)
      tab.mirror.triggerPaint();
  },
  
  link: function(tab){
    // Don't add duplicates
    if(tab.mirror)
      return false;
    
    // Add the tab to the page
    this._createEl(tab);
    return true;
  },
  
  unlink: function(tab){
    var mirror = tab.mirror;
    if(mirror) {
      mirror._sendOnClose();
      var tabCanvas = mirror.tabCanvas;
      if(tabCanvas)
        tabCanvas.detach();
      
      $(mirror.el).remove();
      
      tab.mirror = null;
    }
  }  
};

// ----------
window.TabMirror = {
  _private: new TabMirror(), 
  
  // Function: customize
  // Allows you to customize the tab representations as they are created. 
  // 
  // Parameters: 
  //   func - a callback function that will be called every time a new 
  //     tab or tabs are created. func should take in one parameter, a 
  //     jQuery object representing the div enclosing the tab in question. 
  //     This jQuery object may be singular or multiple, depending on 
  //     the number of tabs being created. 
  customize: function(func) {
    // Apply the custom handlers to all existing elements
    // TODO: Make this modular: so that it only exists in one place.
    //       No breaking DRY!
    func($("div.tab"));
    
    // Apply it to all future elements.
    TabMirror.prototype._customize = func;
  },

  // Function: pausePainting
  // Tells the TabMirror to stop updating thumbnails (so you can do
  // animations without thumbnail paints causing stutters). 
  // pausePainting can be called multiple times, but every call to 
  // pausePainting needs to be mirrored with a call to <resumePainting>. 
  pausePainting: function() {
    this._private.paintingPaused++;
  },
  
  // Function: resumePainting
  // Undoes a call to <pausePainting>. For instance, if you called 
  // pausePainting three times in a row, you'll need to call resumePainting
  // three times before the TabMirror will start updating thumbnails again. 
  resumePainting: function() {
    this._private.paintingPaused--;
  }
};

})();
