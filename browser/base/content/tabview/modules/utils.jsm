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
 * The Original Code is utils.js.
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
// Title: utils.js

(function(){

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

// Get this in a way where we can load the page automatically
// where it doesn't need to be focused...
var homeWindow = Cc["@mozilla.org/embedcomp/window-watcher;1"]
    .getService(Ci.nsIWindowWatcher)
    .activeWindow;

var consoleService = Cc["@mozilla.org/consoleservice;1"]
    .getService(Components.interfaces.nsIConsoleService);

// ##########
// Class: Point
// A simple point. 
//
// Constructor: Point
// If a is a Point, creates a copy of it. Otherwise, expects a to be x, 
// and creates a Point with it along with y. If either a or y are omitted, 
// 0 is used in their place. 
window.Point = function(a, y) {
  if(isPoint(a)) {
    this.x = a.x;
    this.y = a.y;
  } else {
    this.x = (Utils.isNumber(a) ? a : 0);
    this.y = (Utils.isNumber(y) ? y : 0);
  }
};

// ----------
window.isPoint = function(p) {
  return (p && Utils.isNumber(p.x) && Utils.isNumber(p.y));
};

window.Point.prototype = { 
  // ---------- 
  distance: function(point) { 
    var ax = Math.abs(this.x - point.x);
    var ay = Math.abs(this.y - point.y);
    return Math.sqrt((ax * ax) + (ay * ay));
  },

  // ---------- 
  plus: function(point) { 
    return new Point(this.x + point.x, this.y + point.y);
  }
};

// ##########  
// Class: Rect
// A simple rectangle. 
//
// Constructor: Rect
// If a is a Rect, creates a copy of it. Otherwise, expects a to be left, 
// and creates a Rect with it along with top, width, and height. 
window.Rect = function(a, top, width, height) {
  // Note: perhaps 'a' should really be called 'rectOrLeft'
  if(isRect(a)) {
    this.left = a.left;
    this.top = a.top;
    this.width = a.width;
    this.height = a.height;
  } else {
    this.left = a;
    this.top = top;
    this.width = width;
    this.height = height;
  }
};

// ----------
window.isRect = function(r) {
  return (r 
      && Utils.isNumber(r.left)
      && Utils.isNumber(r.top)
      && Utils.isNumber(r.width)
      && Utils.isNumber(r.height));
};

window.Rect.prototype = {
  // ----------
  get right() {
    return this.left + this.width;
  },
  
  // ----------
  set right(value) {
    this.width = value - this.left;
  },

  // ----------
  get bottom() {
    return this.top + this.height;
  },
  
  // ----------
  set bottom(value) {
    this.height = value - this.top;
  },

  // ----------
  get xRange() {
    return new Range(this.left,this.right);
  },
  
  // ----------
  get yRange() {
    return new Range(this.top,this.bottom);
  },

  // ----------
  intersects: function(rect) {
    return (rect.right > this.left
        && rect.left < this.right
        && rect.bottom > this.top
        && rect.top < this.bottom);      
  },
  
  // ----------
  intersection: function(rect) {
    var box = new Rect(Math.max(rect.left, this.left), Math.max(rect.top, this.top), 0, 0);
    box.right = Math.min(rect.right, this.right);
    box.bottom = Math.min(rect.bottom, this.bottom);
    if(box.width > 0 && box.height > 0)
      return box;
      
    return null;
  },
  
  // ----------
  // Function: containsPoint
  // Returns a boolean denoting if the <Point> is inside of
  // the bounding rect.
  // 
  // Paramaters
  //  - A <Point>
  containsPoint: function(point){
    return( point.x > this.left
         && point.x < this.right
         && point.y > this.top
         && point.y < this.bottom )
  },
  
  // ----------
  // Function: contains
  // Returns a boolean denoting if the <Rect> is contained inside
  // of the bounding rect.
  //
  // Paramaters
  //  - A <Rect>
  contains: function(rect){
    return( rect.left > this.left
         && rect.right < this.right
         && rect.top > this.top
         && rect.bottom < this.bottom )
  },
  
  // ----------
  center: function() {
    return new Point(this.left + (this.width / 2), this.top + (this.height / 2));
  },
  
  // ----------
  size: function() {
    return new Point(this.width, this.height);
  },
  
  // ----------
  position: function() {
    return new Point(this.left, this.top);
  },
  
  // ----------
  area: function() {
    return this.width * this.height;
  },
  
  // ----------
  // Function: inset
  // Makes the rect smaller (if the arguments are positive) as if a margin is added all around
  // the initial rect, with the margin widths (symmetric) being specified by the arguments.
  //
  // Paramaters
  //  - A <Point> or two arguments: x and y
  inset: function(a, b) {
    if(typeof(a.x) != 'undefined' && typeof(a.y) != 'undefined') {
      b = a.y; 
      a = a.x;
    }
    
    this.left += a;
    this.width -= a * 2;
    this.top += b;
    this.height -= b * 2;
  },
  
  // ----------
  // Function: offset
  // Moves (translates) the rect by the given vector.
  //
  // Paramaters
  //  - A <Point> or two arguments: x and y
  offset: function(a, b) {
    if(typeof(a.x) != 'undefined' && typeof(a.y) != 'undefined') {
      this.left += a.x;
      this.top += a.y;
    } else {
      this.left += a;
      this.top += b;
    }
  },
  
  // ----------
  equals: function(a) {
    return (a.left == this.left
        && a.top == this.top
        && a.width == this.width
        && a.height == this.height);
  },
  
  // ----------
  union: function(a){
    var newLeft = Math.min(a.left, this.left);
    var newTop = Math.min(a.top, this.top);
    var newWidth = Math.max(a.right, this.right) - newLeft;
    var newHeight = Math.max(a.bottom, this.bottom) - newTop;
    var newRect = new Rect(newLeft, newTop, newWidth, newHeight); 
  
    return newRect;
  },
  
  // ----------
  copy: function(a) {
    this.left = a.left;
    this.top = a.top;
    this.width = a.width;
    this.height = a.height;
  },
  
  // ----------
  // Function: css
  // Returns an object with the dimensions of this rectangle, suitable for passing into iQ.fn.css.
  // You could of course just pass the rectangle straight in, but this is cleaner.
  css: function() {
    return {
      left: this.left,
      top: this.top,
      width: this.width,
      height: this.height
    };
  }
};

// ##########  
// Class: Range
// A physical interval, with a min and max.
//
// Constructor: Range
// Creates a Range with the given min and max
window.Range = function(min, max) {
  if (isRange(min) && !max) { // if the one variable given is a range, copy it.
    this.min = min.min;
    this.max = min.max;
  } else {
    this.min = min || 0;
    this.max = max || 0;
  }
};

// ----------
window.isRange = function(r) {
  return (r 
      && Utils.isNumber(r.min)
      && Utils.isNumber(r.max));
};

window.Range.prototype = {  
  // Variable: extent
  // Equivalent to max-min
  get extent() {
    return (this.max - this.min);
  },

  set extent(extent) {
    this.max = extent - this.min;
  },

  // ----------
  // Function: contains
  // Whether the <Range> contains the given value or not
  //
  // Paramaters
  //  - a number or <Range>
  contains: function(value) {
    return Utils.isNumber(value) ?
      ( value >= this.min && value <= this.max ) :
      ( value.min >= this.min && value.max <= this.max );
  },
  // ----------
  // Function: containsWithin
  // Whether the <Range>'s interior contains the given value or not
  //
  // Paramaters
  //  - a number or <Range>
  containsWithin: function(value) {
    return Utils.isNumber(value) ?
      ( value > this.min && value < this.max ) :
      ( value.min > this.min && value.max < this.max );
  },
  // ----------
  // Function: overlaps
  // Whether the <Range> overlaps with the given <Range> or not.
  //
  // Paramaters
  //  - a number or <Range>
  overlaps: function(value) {
    return Utils.isNumber(value) ? 
      this.contains(value) :
      ( value.min <= this.max && this.min <= value.max );
  },
};

// ##########
// Class: Subscribable
// A mix-in for allowing objects to collect subscribers for custom events. 
// Currently supports only onClose. 
// TODO generalize for any number of events
window.Subscribable = function() {
  this.subscribers = {};
  this.onCloseSubscribers = null;
};

window.Subscribable.prototype = {
  // ----------
  // Function: addSubscriber
  // The given callback will be called when the Subscribable fires the given event.
  // The refObject is used to facilitate removal if necessary. 
  addSubscriber: function(refObject, eventName, callback) {
    if(!this.subscribers[eventName])
      this.subscribers[eventName] = [];
      
    var subs = this.subscribers[eventName];
    var existing = iQ.grep(subs, function(element) {
      return element.refObject == refObject;
    });
    
    if(existing.length) {
      Utils.assert('should only ever be one', existing.length == 1);
      existing[0].callback = callback;
    } else {  
      subs.push({
        refObject: refObject, 
        callback: callback
      });
    }
  },
  
  // ----------
  // Function: removeSubscriber
  // Removes the callback associated with refObject for the given event. 
  removeSubscriber: function(refObject, eventName) {
    if(!this.subscribers[eventName])
      return;
      
    this.subscribers[eventName] = iQ.grep(this.subscribers[eventName], function(element) {
      return element.refObject == refObject;
    }, true);
  },
  
  // ----------
  // Function: _sendToSubscribers
  // Internal routine. Used by the Subscribable to fire events.
  _sendToSubscribers: function(eventName, eventInfo) {
    if(!this.subscribers[eventName])
      return;
      
    var self = this;
    var subsCopy = iQ.merge([], this.subscribers[eventName]);
    iQ.each(subsCopy, function(index, object) { 
      object.callback(self, eventInfo);
    });
  },
  
  // ----------
  // Function: addOnClose
  // The given callback will be called when the Subscribable fires its onClose.
  // The referenceElement is used to facilitate removal if necessary. 
  addOnClose: function(referenceElement, callback) {
    if(!this.onCloseSubscribers)
      this.onCloseSubscribers = [];
      
    var existing = iQ.grep(this.onCloseSubscribers, function(element) {
      return element.referenceElement == referenceElement;
    });
    
    if(existing.length) {
      Utils.assert('should only ever be one', existing.length == 1);
      existing[0].callback = callback;
    } else {  
      this.onCloseSubscribers.push({
        referenceElement: referenceElement, 
        callback: callback
      });
    }
  },
  
  // ----------
  // Function: removeOnClose
  // Removes the callback associated with referenceElement for onClose notification. 
  removeOnClose: function(referenceElement) {
    if(!this.onCloseSubscribers)
      return;
      
    this.onCloseSubscribers = iQ.grep(this.onCloseSubscribers, function(element) {
      return element.referenceElement == referenceElement;
    }, true);
  },
  
  // ----------
  // Function: _sendOnClose
  // Internal routine. Used by the Subscribable to fire onClose events.
  _sendOnClose: function() {
    if(!this.onCloseSubscribers)
      return;
      
    iQ.each(this.onCloseSubscribers, function(index, object) { 
      object.callback(this);
    });
  }
};

// ##########
// Class: Utils
// Singelton with common utility functions.
var Utils = {  
  // ___ Windows and Tabs

  // ----------
  // Variable: activeTab
  // The <Tabs> tab that represents the active tab in the active window.
  get activeTab(){
    try {
      var tabBrowser = this.getCurrentWindow().gBrowser;
      Utils.assert('tabBrowser', tabBrowser);
      
      var rawTab = tabBrowser.selectedTab;
      for( var i=0; i<Tabs.length; i++){
        if(Tabs[i].raw == rawTab)
          return Tabs[i];
      }
    } catch(e) {
      Utils.log(e);
    }
    
    return null;
  },
  
  // ----------
  // Function: getCurrentWindow
  // Returns the nsIDOMWindowInternal for the currently active window, 
  // i.e. the window belonging to the active page's DOM "window" object.
  getCurrentWindow: function() {
    var wm = Cc["@mozilla.org/appshell/window-mediator;1"]
             .getService(Ci.nsIWindowMediator);
    var browserEnumerator = wm.getEnumerator("navigator:browser");
    while (browserEnumerator.hasMoreElements()) {
      var browserWin = browserEnumerator.getNext();
      var tabbrowser = browserWin.gBrowser;
      let tabCandyContainer = browserWin.document.getElementById("tab-candy");
      if (tabCandyContainer && tabCandyContainer.contentWindow == window) {
        return browserWin;
      }
    }
    
    return null;
  },

  // ___ Files
  getInstallDirectory: function(id, callback) { 
    if (Cc["@mozilla.org/extensions/manager;1"]) {
      var extensionManager = Cc["@mozilla.org/extensions/manager;1"]  
                             .getService(Ci.nsIExtensionManager);  
      var file = extensionManager.getInstallLocation(id).getItemFile(id, "install.rdf"); 
      callback(file.parent);  
    }
    else {
      Components.utils.import("resource://gre/modules/AddonManager.jsm");
      AddonManager.getAddonByID(id, function(addon) {
        var fileStr = addon.getResourceURL("install.rdf");
        var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);  
        var url = ios.newURI(fileStr, null, null);
        callback(url.QueryInterface(Ci.nsIFileURL).file.parent);
      });
    }
  }, 
  
  getFiles: function(dir) {
    var files = [];
    if(dir.isReadable() && dir.isDirectory) {
      var entries = dir.directoryEntries;
      while(entries.hasMoreElements()) {
        var entry = entries.getNext();
        entry.QueryInterface(Ci.nsIFile);
        files.push(entry);
      }
    }
    
    return files;
  },

  // ___ Logging
  
  ilog: function(){ 
    Utils.log('!!ilog is no longer supported!!');
  },
  
  log: function() { // pass as many arguments as you want, it'll print them all
    var text = this.expandArgumentsForLog(arguments);
    consoleService.logStringMessage(text);
  }, 
  
  error: function() { // pass as many arguments as you want, it'll print them all
    var text = this.expandArgumentsForLog(arguments);
    Cu.reportError('tabcandy error: ' + text);
  }, 
  
  trace: function() { // pass as many arguments as you want, it'll print them all
    var text = this.expandArgumentsForLog(arguments);
    if(typeof(printStackTrace) != 'function')
      this.log(text + ' trace: you need to include stacktrace.js');
    else {
      var calls = printStackTrace();
      calls.splice(0, 3); // Remove this call and the printStackTrace calls
      this.log('trace: ' + text + '\n' + calls.join('\n'));
    }
  }, 
  
  assert: function(label, condition) {
    if(!condition) {
      var text;
      if(typeof(label) == 'undefined')
        text = 'badly formed assert';
      else
        text = 'tabcandy assert: ' + label;        
        
      if(typeof(printStackTrace) == 'function') {
        var calls = printStackTrace();
        text += '\n' + calls[3];
      }
      
      this.trace(text);
    }
  },
  
  // Function: assertThrow
  // Throws label as an exception if condition is false.
  assertThrow: function(label, condition) {
    if(!condition) {
      var text;
      if(typeof(label) == 'undefined')
        text = 'badly formed assert';
      else
        text = 'tabcandy assert: ' + label; 
        
      if(typeof(printStackTrace) == 'function') {
        var calls = printStackTrace();
        calls.splice(0, 3); // Remove this call and the printStackTrace calls
        text += '\n' + calls.join('\n');
      }
  
      throw text;       
    }
  },
  
  expandObject: function(obj) {
      var s = obj + ' = {';
      for(prop in obj) {
        var value;
        try {
          value = obj[prop]; 
        } catch(e) {
          value = '[!!error retrieving property]';
        }
        
        s += prop + ': ';
        if(typeof(value) == 'string')
          s += '\'' + value + '\'';
        else if(typeof(value) == 'function')
          s += 'function';
        else
          s += value;

        s += ", ";
      }
      return s + '}';
    }, 
    
  expandArgumentsForLog: function(args) {
    var s = '';
    var count = args.length;
    var a;
    for(a = 0; a < count; a++) {
      var arg = args[a];
      if(typeof(arg) == 'object')
        arg = this.expandObject(arg);
        
      s += arg;
      if(a < count - 1)
        s += '; ';
    }
    
    return s;
  },
  
  testLogging: function() {
    this.log('beginning logging test'); 
    this.error('this is an error');
    this.trace('this is a trace');
    this.log(1, null, {'foo': 'hello', 'bar': 2}, 'whatever');
    this.log('ending logging test');
  }, 
  
  // ___ Event
  isRightClick: function(event) {
    if(event.which)
      return (event.which == 3);
    if(event.button) 
      return (event.button == 2);
    
    return false;
  },
  
  // ___ Time
  getMilliseconds: function() {
    var date = new Date();
    return date.getTime();
  },
  
  // ___ Misc
  isDOMElement: function(object) {
    return (object && typeof(object.nodeType) != 'undefined' ? true : false);
  },
 
  // ----------
  // Function: isNumber
  // Returns true if the argument is a valid number. 
  isNumber: function(n) {
    return (typeof(n) == 'number' && !isNaN(n));
  },
  
  // ----------
  // Function: copy
  // Returns a copy of the argument. Note that this is a shallow copy; if the argument
  // has properties that are themselves objects, those properties will be copied by reference.
  copy: function(value) {
    if(value && typeof(value) == 'object') {
      if(iQ.isArray(value))
        return iQ.extend([], value);
        
      return iQ.extend({}, value);
    }
      
    return value;
  }
};

window.Utils = Utils;

window.Math.tanh = function tanh(x){
  var e = Math.exp(x);
  return (e - 1/e) / (e + 1/e); 
}

})();
