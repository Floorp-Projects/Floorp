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
 * The Original Code is iq.js.
 *
 * The Initial Developer of the Original Code is
 * Ian Gilman <ian@iangilman.com>.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Aza Raskin <aza@mozilla.com>
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>
 *
 * Some portions copied from:
 * jQuery JavaScript Library v1.4.2
 * http://jquery.com/
 * Copyright 2010, John Resig
 * Dual licensed under the MIT or GPL Version 2 licenses.
 * http://jquery.org/license
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
// Title: iq.js
// Various helper functions, in the vein of jQuery.

(function( window, undefined ) {

var iQ = function(selector, context) {
    // The iQ object is actually just the init constructor 'enhanced'
    return new iQ.fn.init( selector, context );
  },

  // Map over iQ in case of overwrite
  _iQ = window.iQ,

  // Use the correct document accordingly with window argument (sandbox)
  document = window.document,

  // A central reference to the root iQ(document)
  rootiQ,

  // A simple way to check for HTML strings or ID strings
  // (both of which we optimize for)
  quickExpr = /^[^<]*(<[\w\W]+>)[^>]*$|^#([\w-]+)$/,

  // Match a standalone tag
  rsingleTag = /^<(\w+)\s*\/?>(?:<\/\1>)?$/,

  rclass = /[\n\t]/g,
  rspace = /\s+/;

// ##########
// Class: iQ.fn
// An individual element or group of elements.
iQ.fn = iQ.prototype = {
  // ----------
  // Function: init
  // You don't call this directly; this is what's called by iQ().
  // It works pretty much like jQuery(), with a few exceptions,
  // most notably that you can't use strings with complex html,
  // just simple tags like '<div>'.
  init: function( selector, context ) {
    var match, elem, ret, doc;

    // Handle $(""), $(null), or $(undefined)
    if ( !selector ) {
      return this;
    }

    // Handle $(DOMElement)
    if ( selector.nodeType ) {
      this.context = this[0] = selector;
      this.length = 1;
      return this;
    }

    // The body element only exists once, optimize finding it
    if ( selector === "body" && !context ) {
      this.context = document;
      this[0] = document.body;
      this.selector = "body";
      this.length = 1;
      return this;
    }

    // Handle HTML strings
    if ( typeof selector === "string" ) {
      // Are we dealing with HTML string or an ID?
      match = quickExpr.exec( selector );

      // Verify a match, and that no context was specified for #id
      if ( match && (match[1] || !context) ) {

        // HANDLE $(html) -> $(array)
        if ( match[1] ) {
          doc = (context ? context.ownerDocument || context : document);

          // If a single string is passed in and it's a single tag
          // just do a createElement and skip the rest
          ret = rsingleTag.exec( selector );

          if ( ret ) {
            if ( Utils.isPlainObject( context ) ) {
              Utils.assert('does not support HTML creation with context', false);
            } else {
              selector = [ doc.createElement( ret[1] ) ];
            }

          } else {
              Utils.assert('does not support complex HTML creation', false);
          }

          return Utils.merge( this, selector );

        // HANDLE $("#id")
        } else {
          elem = document.getElementById( match[2] );

          if ( elem ) {
            this.length = 1;
            this[0] = elem;
          }

          this.context = document;
          this.selector = selector;
          return this;
        }

      // HANDLE $("TAG")
      } else if ( !context && /^\w+$/.test( selector ) ) {
        this.selector = selector;
        this.context = document;
        selector = document.getElementsByTagName( selector );
        return Utils.merge( this, selector );

      // HANDLE $(expr, $(...))
      } else if ( !context || context.iq ) {
        return (context || rootiQ).find( selector );

      // HANDLE $(expr, context)
      // (which is just equivalent to: $(context).find(expr)
      } else {
        return iQ( context ).find( selector );
      }

    // HANDLE $(function)
    // Shortcut for document ready
    } else if ( Utils.isFunction( selector ) ) {
      Utils.log('iQ does not support ready functions');
      return null;
    }

    if (selector.selector !== undefined) {
      this.selector = selector.selector;
      this.context = selector.context;
    }

    // this used to be makeArray:
    var ret = this || [];
    if ( selector != null ) {
      // The window, strings (and functions) also have 'length'
      // The extra typeof function check is to prevent crashes
      // in Safari 2 (See: #3039)
      if ( selector.length == null || typeof selector === "string" || Utils.isFunction(selector) || (typeof selector !== "function" && selector.setInterval) ) {
        Array.prototype.push.call( ret, selector );
      } else {
        Utils.merge( ret, selector );
      }
    }
    return ret;

  },

  // Start with an empty selector
  selector: "",

  // The current version of iQ being used
  iq: "1.4.2",

  // The default length of a iQ object is 0
  length: 0,

  // ----------
  // Function: get
  // Get the Nth element in the matched element set OR
  // Get the whole matched element set as a clean array
  get: function( num ) {
    return num == null ?

      // Return a 'clean' array
      // was toArray
      Array.prototype.slice.call( this, 0 ) :

      // Return just the object
      ( num < 0 ? this[ num + this.length ] : this[ num ] );
  },

  // ----------
  // Function: each
  // Execute a callback for every element in the matched set.
  each: function( callback ) {
    if ( !Utils.isFunction(callback) ) {
      Utils.assert("each's argument must be a function", false);
      return null;
    }
    for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
      callback(elem);
    }
    return this;
  },

  // ----------
  // Function: addClass
  // Adds the given class(es) to the receiver.
  addClass: function( value ) {
    if ( Utils.isFunction(value) ) {
      Utils.assert('does not support function argument', false);
      return null;
    }

    if ( value && typeof value === "string" ) {
      for ( var i = 0, l = this.length; i < l; i++ ) {
        var elem = this[i];
        if ( elem.nodeType === 1 ) {
          (value || "").split( rspace ).forEach(function(className) {
            elem.classList.add(className);
          });
        }
      }
    }

    return this;
  },

  // ----------
  // Function: removeClass
  // Removes the given class(es) from the receiver.
  removeClass: function( value ) {
    if ( Utils.isFunction(value) ) {
      Utils.assert('does not support function argument', false);
      return null;
    }

    if ( (value && typeof value === "string") || value === undefined ) {
      for ( var i = 0, l = this.length; i < l; i++ ) {
        var elem = this[i];
        if ( elem.nodeType === 1 && elem.className ) {
          if ( value ) {
            (value || "").split(rspace).forEach(function(className) {
              elem.classList.remove(className);
            });
          } else {
            elem.className = "";
          }
        }
      }
    }

    return this;
  },

  // ----------
  // Function: hasClass
  // Returns true is the receiver has the given css class.
  hasClass: function( selector ) {
    for ( var i = 0, l = this.length; i < l; i++ ) {
      if ( this[i].classList.contains( selector ) ) {
        return true;
      }
    }
    return false;
  },

  // ----------
  // Function: find
  // Searches the receiver and its children, returning a new iQ object with
  // elements that match the given selector.
  find: function( selector ) {
    var ret = [], length = 0;

    for ( var i = 0, l = this.length; i < l; i++ ) {
      length = ret.length;
      try {
        Utils.merge(ret, this[i].querySelectorAll( selector ) );
      } catch(e) {
        Utils.log('iQ.find error (bad selector)', e);
      }

      if ( i > 0 ) {
        // Make sure that the results are unique
        for ( var n = length; n < ret.length; n++ ) {
          for ( var r = 0; r < length; r++ ) {
            if ( ret[r] === ret[n] ) {
              ret.splice(n--, 1);
              break;
            }
          }
        }
      }
    }

    return iQ(ret);
  },

  // ----------
  // Function: remove
  // Removes the receiver from the DOM.
  remove: function(unused) {
    Utils.assert('does not accept a selector', unused === undefined);
    for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
      if ( elem.parentNode ) {
         elem.parentNode.removeChild( elem );
      }
    }

    return this;
  },

  // ----------
  // Function: empty
  // Removes all of the reciever's children and HTML content from the DOM.
  empty: function() {
    for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
      while ( elem.firstChild ) {
        elem.removeChild( elem.firstChild );
      }
    }

    return this;
  },

  // ----------
  // Function: width
  // Returns the width of the receiver.
  width: function(unused) {
    Utils.assert('does not yet support setting', unused === undefined);
    return parseInt(this.css('width'));
  },

  // ----------
  // Function: height
  // Returns the height of the receiver.
  height: function(unused) {
    Utils.assert('does not yet support setting', unused === undefined);
    return parseInt(this.css('height'));
  },

  // ----------
  // Function: position
  // Returns an object with the receiver's position in left and top properties.
  position: function(unused) {
    Utils.assert('does not yet support setting', unused === undefined);
    return {
      left: parseInt(this.css('left')),
      top: parseInt(this.css('top'))
    };
  },

  // ----------
  // Function: bounds
  // Returns a <Rect> with the receiver's bounds.
  bounds: function(unused) {
    Utils.assert('does not yet support setting', unused === undefined);
    var p = this.position();
    return new Rect(p.left, p.top, this.width(), this.height());
  },

  // ----------
  // Function: data
  // Pass in both key and value to attach some data to the receiver;
  // pass in just key to retrieve it.
  data: function(key, value) {
    var data = null;
    if (value === undefined) {
      Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
      data = this[0].iQData;
      return (data ? data[key] : null);
    }

    for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
      data = elem.iQData;

      if (!data)
        data = elem.iQData = {};

      data[key] = value;
    }

    return this;
  },

  // ----------
  // Function: html
  // Given a value, sets the receiver's innerHTML to it; otherwise returns what's already there.
  // TODO: security
  html: function(value) {
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    if (value === undefined)
      return this[0].innerHTML;

    this[0].innerHTML = value;
    return this;
  },

  // ----------
  // Function: text
  // Given a value, sets the receiver's textContent to it; otherwise returns what's already there.
  text: function(value) {
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    if (value === undefined) {
      return this[0].textContent;
    }

    return this.empty().append( (this[0] && this[0].ownerDocument || document).createTextNode(value));
  },

  // ----------
  // Function: val
  // Given a value, sets the receiver's value to it; otherwise returns what's already there.
  val: function(value) {
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    if (value === undefined) {
      return this[0].value;
    }

    this[0].value = value;
    return this;
  },

  // ----------
  // Function: appendTo
  // Appends the receiver to the result of iQ(selector).
  appendTo: function(selector) {
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    iQ(selector).append(this);
    return this;
  },

  // ----------
  // Function: append
  // Appends the result of iQ(selector) to the receiver.
  append: function(selector) {
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    var object = iQ(selector);
    Utils.assert('does not yet support multi-objects (or null objects)', object.length == 1);
    this[0].appendChild(object[0]);
    return this;
  },

  // ----------
  // Function: attr
  // Sets or gets an attribute on the element(s).
  attr: function(key, value) {
    try {
      Utils.assert('string key', typeof key === 'string');
      if (value === undefined) {
        Utils.assert('retrieval does not support multi-objects (or null objects)', this.length == 1);
        return this[0].getAttribute(key);
      }
      for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
        elem.setAttribute(key, value);
      }
    } catch(e) {
      Utils.log(e);
    }

    return this;
  },

  // ----------
  // Function: css
  // Sets or gets CSS properties on the receiver. When setting certain numerical properties,
  // will automatically add "px".
  //
  // Possible call patterns:
  //   a: object, b: undefined - sets with properties from a
  //   a: string, b: undefined - gets property specified by a
  //   a: string, b: string/number - sets property specified by a to b
  css: function(a, b) {
    var properties = null;

    if (typeof a === 'string') {
      var key = a;
      if (b === undefined) {
        Utils.assert('retrieval does not support multi-objects (or null objects)', this.length == 1);

        var substitutions = {
          'MozTransform': '-moz-transform',
          'zIndex': 'z-index'
        };

        return window.getComputedStyle(this[0], null).getPropertyValue(substitutions[key] || key);
      }
      properties = {};
      properties[key] = b;
    } else {
      properties = a;
    }

    var pixels = {
      'left': true,
      'top': true,
      'right': true,
      'bottom': true,
      'width': true,
      'height': true
    };

    for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
      for (var key in properties) {
        var value = properties[key];
        if (pixels[key] && typeof(value) != 'string')
          value += 'px';

        if (key.indexOf('-') != -1)
          elem.style.setProperty(key, value, '');
        else
          elem.style[key] = value;
      }
    }

    return this;
  },

  // ----------
  // Function: animate
  // Uses CSS transitions to animate the element.
  //
  // Parameters:
  //   css - an object map of the CSS properties to change
  //   options - an object with various properites (see below)
  //
  // Possible "options" properties:
  //   duration - how long to animate, in milliseconds
  //   easing - easing function to use. Possibilities include 'tabcandyBounce', 'easeInQuad'.
  //     Default is 'ease'.
  //   complete - function to call once the animation is done, takes nothing in, but "this"
  //     is set to the element that was animated.
  animate: function(css, options) {
    try {
      Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);

      if (!options)
        options = {};

      var easings = {
        tabcandyBounce: 'cubic-bezier(0.0, 0.63, .6, 1.29)',
        easeInQuad: 'ease-in', // TODO: make it a real easeInQuad, or decide we don't care
        fast: 'cubic-bezier(0.7,0,1,1)'
      };

      var duration = (options.duration || 400);
      var easing = (easings[options.easing] || 'ease');

      // The latest versions of Firefox do not animate from a non-explicitly set
      // css properties. So for each element to be animated, go through and
      // explicitly define 'em.
      var rupper = /([A-Z])/g;
      this.each(function(elem){
        var cStyle = window.getComputedStyle(elem, null);
        for (var prop in css){
          prop = prop.replace( rupper, "-$1" ).toLowerCase();
          iQ(elem).css(prop, cStyle.getPropertyValue(prop));
        }
      });


      this.css({
        '-moz-transition-property': 'all', // TODO: just animate the properties we're changing
        '-moz-transition-duration': (duration / 1000) + 's',
        '-moz-transition-timing-function': easing
      });

      this.css(css);

      var self = this;
      Utils.timeout(function() {
        self.css({
          '-moz-transition-property': 'none',
          '-moz-transition-duration': '',
          '-moz-transition-timing-function': ''
        });

        if (Utils.isFunction(options.complete))
          options.complete.apply(self);
      }, duration);
    } catch(e) {
      Utils.log(e);
    }

    return this;
  },

  // ----------
  // Function: fadeOut
  // Animates the receiver to full transparency. Calls callback on completion.
  fadeOut: function(callback) {
    try {
      Utils.assert('does not yet support duration', Utils.isFunction(callback) || callback === undefined);
      this.animate({
        opacity: 0
      }, {
        duration: 400,
        complete: function() {
          iQ(this).css({display: 'none'});
          if (Utils.isFunction(callback))
            callback.apply(this);
        }
      });
    } catch(e) {
      Utils.log(e);
    }

    return this;
  },

  // ----------
  // Function: fadeIn
  // Animates the receiver to full opacity.
  fadeIn: function() {
    try {
      this.css({display: ''});
      this.animate({
        opacity: 1
      }, {
        duration: 400
      });
    } catch(e) {
      Utils.log(e);
    }

    return this;
  },

  // ----------
  // Function: hide
  // Hides the receiver.
  hide: function() {
    try {
      this.css({display: 'none', opacity: 0});
    } catch(e) {
      Utils.log(e);
    }

    return this;
  },

  // ----------
  // Function: show
  // Shows the receiver.
  show: function() {
    try {
      this.css({display: '', opacity: 1});
    } catch(e) {
      Utils.log(e);
    }

    return this;
  },

  // ----------
  // Function: bind
  // Binds the given function to the given event type. Also wraps the function
  // in a try/catch block that does a Utils.log on any errors.
  bind: function(type, func) {
    Utils.assert('does not support eventData argument', Utils.isFunction(func));

    var handler = function(event) {
      try {
        return func.apply(this, [event]);
      } catch(e) {
        Utils.log(e);
      }
    };

    for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
      if (!elem.iQEventData)
        elem.iQEventData = {};

      if (!elem.iQEventData[type])
        elem.iQEventData[type] = [];

      elem.iQEventData[type].push({
        original: func,
        modified: handler
      });

      elem.addEventListener(type, handler, false);
    }

    return this;
  },

  // ----------
  // Function: one
  // Binds the given function to the given event type, but only for one call;
  // automatically unbinds after the event fires once.
  one: function(type, func) {
    Utils.assert('does not support eventData argument', Utils.isFunction(func));

    var handler = function(e) {
      iQ(this).unbind(type, handler);
      return func.apply(this, [e]);
    };

    return this.bind(type, handler);
  },

  // ----------
  // Function: unbind
  // Unbinds the given function from the given event type.
  unbind: function(type, func) {
    Utils.assert('Must provide a function', Utils.isFunction(func));

    for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
      var handler = func;
      if (elem.iQEventData && elem.iQEventData[type]) {
        for (var a = 0, count = elem.iQEventData[type].length; a < count; a++) {
          var pair = elem.iQEventData[type][a];
          if (pair.original == func) {
            handler = pair.modified;
            elem.iQEventData[type].splice(a, 1);
            break;
          }
        }
      }

      elem.removeEventListener(type, handler, false);
    }

    return this;
  }
};

// ----------
// Give the init function the iQ prototype for later instantiation
iQ.fn.init.prototype = iQ.fn;

// ----------
// Create various event aliases
(function() {
  var events = [
    'keyup',
    'keydown',
    'mouseup',
    'mousedown',
    'mouseover',
    'mouseout',
    'mousemove',
    'click',
    'resize',
    'change',
    'blur',
    'focus'
  ];

  events.forEach(function(event) {
    iQ.fn[event] = function(func) {
      return this.bind(event, func);
    };
  });
})();

// ----------
// All iQ objects should point back to these
rootiQ = iQ(document);

// ----------
// Expose iQ to the global object
window.iQ = iQ;

})(window);
