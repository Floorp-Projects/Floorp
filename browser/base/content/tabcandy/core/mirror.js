/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mirror.js.
 *
 * The Initial Developer of the Original Code is
 * Aza Raskin <aza@mozilla.com>
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Ian Gilman <ian@iangilman.com>
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// **********
// Title: mirror.js

(function(){

function _isIframe(doc){
  var win = doc.defaultView;
  return win.parent != win;
}

// ##########
// Class: TabCanvas
// Takes care of the actual canvas for the tab thumbnail
var TabCanvas = function(tab, canvas){ 
  this.init(tab, canvas);
};

TabCanvas.prototype = {
  // ----------
  // Function: init
  init: function(tab, canvas){
    this.tab = tab;
    this.canvas = canvas;
    this.window = window;
            
    var $canvas = iQ(canvas).data("link", this);

    var w = $canvas.width();
    var h = $canvas.height();
    canvas.width = w;
    canvas.height = h;
      
    var self = this;
    this.paintIt = function(evt) { 
      // note that "window" is unreliable in this context, so we'd use self.window if needed
      self.tab.mirror.triggerPaint();
/*       self.window.Utils.log('paint it', self.tab.url); */
    };
  },
  
  // ----------
  // Function: attach
  attach: function() {
    this.tab.contentWindow.addEventListener("MozAfterPaint", this.paintIt, false);
  },
     
  // ----------
  // Function: detach
  detach: function() {
    try {
      this.tab.contentWindow.removeEventListener("MozAfterPaint", this.paintIt, false);
    } catch(e) {
      // ignore
    }
  },
  
  // ----------
  // Function: paint
  paint: function(evt){
    var ctx = this.canvas.getContext("2d");
  
    var w = this.canvas.width;
    var h = this.canvas.height;
    if (!w || !h)
      return;
  
    var fromWin = this.tab.contentWindow;
    if (fromWin == null) {
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
  }
};

// ##########
// Class: Mirror
// A single tab in the browser and its visual representation in the tab candy window. 
// Note that it implements the <Subscribable> interface.
function Mirror(tab, manager) {
/*   Utils.log('creating a mirror'); */
  this.tab = tab;
  this.manager = manager;
  
  var $div = iQ('<div>')
    .data("tab", this.tab)
    .addClass('tab')
    .html("<div class='favicon'><img/></div>" +
          "<div class='thumb'><div class='thumbShadow'></div><canvas/></div>" +
          "<span class='tab-title'>&nbsp;</span>"
    )
    .appendTo('body');
    
  this.needsPaint = 0;
  this.canvasSizeForced = false;
  this.el = $div.get(0);
  this.favEl = iQ('.favicon>img', $div).get(0);
  this.nameEl = iQ('.tab-title', $div).get(0);
  this.canvasEl = iQ('.thumb canvas', $div).get(0);
      
  var doc = this.tab.contentDocument;
  if ( !_isIframe(doc) ) {
    this.tabCanvas = new TabCanvas(this.tab, this.canvasEl);    
    this.tabCanvas.attach();
    this.triggerPaint();
  }
  
/*   Utils.log('applying mirror'); */
  this.tab.mirror = this;
  this.manager._customize(this);
/*   Utils.log('done creating mirror'); */
}

Mirror.prototype = iQ.extend(new Subscribable(), {  
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
    this.canvasEl.width = w;
    this.canvasEl.height = h;
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
var TabMirror = function() {
  if (window.Tabs) {
    this.init();
  }
  else { 
    var self = this;
    TabsManager.addSubscriber(this, 'load', function() {
      self.init();
    });
  }
};

TabMirror.prototype = {
  // ----------
  // Function: init
  // Set up the necessary tracking to maintain the <Mirror>s.
  init: function(){
    var self = this;

    // When a tab is opened, create the mirror
    Tabs.onOpen(function() { 
      var tab = this;
      iQ.timeout(function() { // Marshal event from chrome thread to DOM thread
        self.update(tab);
      }, 1);
    });

    // When a tab is updated, update the mirror
    Tabs.onReady( function(evt){
      var tab = evt.tab;
      iQ.timeout(function() { // Marshal event from chrome thread to DOM thread
        self.update(tab);
      }, 1);
    });

    // When a tab is closed, unlink.    
    Tabs.onClose( function(){
      var tab = this;
      iQ.timeout(function() { // Marshal event from chrome thread to DOM thread
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
/*       Utils.log('heartbeat', this.paintingPaused); */
      var now = Utils.getMilliseconds();
      var count = Tabs.length;
      if (count && this.paintingPaused <= 0) {
        this.heartbeatIndex++;
        if (this.heartbeatIndex >= count)
          this.heartbeatIndex = 0;
          
        var tab = Tabs[this.heartbeatIndex];
        var mirror = tab.mirror; 
/*         Utils.log('heartbeat mirror check'); */
        if (mirror) {
/*           Utils.log('hasMirror'); */
          var iconUrl = tab.raw.linkedBrowser.mIconURL;
          if ( iconUrl == null ){
            iconUrl = "chrome://mozapps/skin/places/defaultFavicon.png";
          }

          var label = tab.raw.label;
          var $name = iQ(mirror.nameEl);
          var $canvas = iQ(mirror.canvasEl);
          
          if (iconUrl != mirror.favEl.src) { 
            mirror.favEl.src = iconUrl;
            mirror.triggerPaint();
          }
            
          if ($name.text() != label) {
            $name.text(label);
            mirror.triggerPaint();
          }
          
          if (tab.url != mirror.url) {
            var oldURL = mirror.url;
            mirror.url = tab.url;
            mirror._sendToSubscribers('urlChanged', {oldURL: oldURL, newURL: tab.url});
            mirror.triggerPaint();
          }
          
          if (!mirror.canvasSizeForced) {
            var w = $canvas.width();
            var h = $canvas.height();
            if (w != mirror.canvasEl.width || h != mirror.canvasEl.height) {
              mirror.canvasEl.width = w;
              mirror.canvasEl.height = h;
              mirror.triggerPaint();
            }
          }
          
          if (mirror.needsPaint) {
/*             Utils.log('aboutToPaint'); */
            mirror.tabCanvas.paint();
            
            if (Utils.getMilliseconds() - mirror.needsPaint > 5000)
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
    iQ.timeout(function() {
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

    if (tab.mirror && tab.mirror.tabCanvas)
      tab.mirror.triggerPaint();
  },
  
  link: function(tab){
    // Don't add duplicates
    if (tab.mirror)
      return false;
    
    // Add the tab to the page
    this._createEl(tab);
    return true;
  },
  
  unlink: function(tab){
    var mirror = tab.mirror;
    if (mirror) {
      mirror._sendOnClose();
      var tabCanvas = mirror.tabCanvas;
      if (tabCanvas)
        tabCanvas.detach();
      
      iQ(mirror.el).remove();
      
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
  //     <Mirror> representing the tab in question. 
  customize: function(func) {
    // Apply the custom handlers to all existing elements
    iQ('div.tab').each(function(elem) {
      var tab = Tabs.tab(elem);
      func(tab.mirror);
    });
    
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
  },

  // Function: isPaintingPaused
  // Returns a boolean indicating whether painting
  // is paused or not.   
  isPaintingPaused: function() {
    return this._private.paintingPause > 0;
  }
};

})();
