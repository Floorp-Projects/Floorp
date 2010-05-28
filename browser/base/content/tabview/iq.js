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
 * The Original Code is iQ.
 *
 * The Initial Developer of the Original Code is
 * Ian Gilman <ian@iangilman.com>.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Some portions copied from:
 * jQuery JavaScript Library v1.4.2
 * http://jquery.com/
 * Copyright 2010, John Resig
 * Dual licensed under the MIT or GPL Version 2 licenses.
 * http://jquery.org/license
 *
 * ... which includes Sizzle.js
 * http://sizzlejs.com/
 * Copyright 2010, The Dojo Foundation
 * Released under the MIT, BSD, and GPL Licenses.
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

// ##########
// Title: iq.js
// jQuery, hacked down to just the bits we need.
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

	// Is it a simple selector
	isSimple = /^.[^:#\[\.,]*$/,

	// Check if a string has a non-whitespace character in it
	rnotwhite = /\S/,

	// Used for trimming whitespace
	rtrim = /^(\s|\u00A0)+|(\s|\u00A0)+$/g,

	// Match a standalone tag
	rsingleTag = /^<(\w+)\s*\/?>(?:<\/\1>)?$/,

	// Save a reference to some core methods
	toString = Object.prototype.toString,
	hasOwnProperty = Object.prototype.hasOwnProperty,
	push = Array.prototype.push,
	slice = Array.prototype.slice,
	indexOf = Array.prototype.indexOf;

var rclass = /[\n\t]/g,
	rspace = /\s+/,
	rreturn = /\r/g,
	rspecialurl = /href|src|style/,
	rtype = /(button|input)/i,
	rfocusable = /(button|input|object|select|textarea)/i,
	rclickable = /^(a|area)$/i,
	rradiocheck = /radio|checkbox/;

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
						if ( iQ.isPlainObject( context ) ) {
							Utils.assert('does not support HTML creation with context', false);
							selector = [ document.createElement( ret[1] ) ];
/* 							iQ.fn.attr.call( selector, context, true ); */

						} else {
							selector = [ doc.createElement( ret[1] ) ];
						}

					} else {
							Utils.assert('does not support complex HTML creation', false);
/* 					  ret = doc.createDocumentFragment([match */
						ret = buildFragment( [ match[1] ], [ doc ] );
						selector = (ret.cacheable ? ret.fragment.cloneNode(true) : ret.fragment).childNodes;
					}
					
					return iQ.merge( this, selector );
					
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
				return iQ.merge( this, selector );

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
		} else if ( iQ.isFunction( selector ) ) {
			Utils.log('iQ does not support ready functions');
			return null;
		}

		if (selector.selector !== undefined) {
			this.selector = selector.selector;
			this.context = selector.context;
		}

		return iQ.makeArray( selector, this );
	},
	
	// Start with an empty selector
	selector: "",

	// The current version of iQ being used
	iq: "1.4.2",

	// The default length of a iQ object is 0
	length: 0, 
	
  // ----------
  // Function: toArray
	toArray: function() {
		return slice.call( this, 0 );
	},

  // ----------
  // Function: get
	// Get the Nth element in the matched element set OR
	// Get the whole matched element set as a clean array
	get: function( num ) {
		return num == null ?

			// Return a 'clean' array
			this.toArray() :

			// Return just the object
			( num < 0 ? this.slice(num)[ 0 ] : this[ num ] );
	},

  // ----------
  // Function: pushStack
	// Take an array of elements and push it onto the stack
	// (returning the new matched element set)
	pushStack: function( elems, name, selector ) {
		// Build a new iQ matched element set
		var ret = iQ();

		if ( iQ.isArray( elems ) ) {
			push.apply( ret, elems );
		
		} else {
			iQ.merge( ret, elems );
		}

		// Add the old object onto the stack (as a reference)
		ret.prevObject = this;

		ret.context = this.context;

		if ( name === "find" ) {
			ret.selector = this.selector + (this.selector ? " " : "") + selector;
		} else if ( name ) {
			ret.selector = this.selector + "." + name + "(" + selector + ")";
		}

		// Return the newly-formed element set
		return ret;
	},

  // ----------
  // Function: each
	// Execute a callback for every element in the matched set.
	// (You can seed the arguments with an array of args, but this is
	// only used internally.)
	each: function( callback, args ) {
		return iQ.each( this, callback, args );
	},
	
  // ----------
  // Function: slice
	slice: function() {
		return this.pushStack( slice.apply( this, arguments ),
			"slice", slice.call(arguments).join(",") );
	},

  // ----------
  // Function: addClass
	addClass: function( value ) {
		if ( iQ.isFunction(value) ) {
		  Utils.assert('does not support function argument', false);
		  return null;
		}

		if ( value && typeof value === "string" ) {
			var classNames = (value || "").split( rspace );

			for ( var i = 0, l = this.length; i < l; i++ ) {
				var elem = this[i];

				if ( elem.nodeType === 1 ) {
					if ( !elem.className ) {
						elem.className = value;

					} else {
						var className = " " + elem.className + " ", setClass = elem.className;
						for ( var c = 0, cl = classNames.length; c < cl; c++ ) {
							if ( className.indexOf( " " + classNames[c] + " " ) < 0 ) {
								setClass += " " + classNames[c];
							}
						}
						elem.className = iQ.trim( setClass );
					}
				}
			}
		}

		return this;
	},

  // ----------
  // Function: removeClass
	removeClass: function( value ) {
		if ( iQ.isFunction(value) ) {
		  Utils.assert('does not support function argument', false);
		  return null;
		}

		if ( (value && typeof value === "string") || value === undefined ) {
			var classNames = (value || "").split(rspace);

			for ( var i = 0, l = this.length; i < l; i++ ) {
				var elem = this[i];

				if ( elem.nodeType === 1 && elem.className ) {
					if ( value ) {
						var className = (" " + elem.className + " ").replace(rclass, " ");
						for ( var c = 0, cl = classNames.length; c < cl; c++ ) {
							className = className.replace(" " + classNames[c] + " ", " ");
						}
						elem.className = iQ.trim( className );

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
	hasClass: function( selector ) {
		var className = " " + selector + " ";
		for ( var i = 0, l = this.length; i < l; i++ ) {
			if ( (" " + this[i].className + " ").replace(rclass, " ").indexOf( className ) > -1 ) {
				return true;
			}
		}

		return false;
	},

  // ----------
  // Function: find
	find: function( selector ) {
		var ret = [], length = 0;

		for ( var i = 0, l = this.length; i < l; i++ ) {
			length = ret.length;
      try {
        iQ.merge(ret, this[i].querySelectorAll( selector ) );
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
	width: function(unused) {
    Utils.assert('does not yet support setting', unused === undefined);
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    return this[0].clientWidth;
  },

  // ----------
  // Function: height
	height: function(unused) {
    Utils.assert('does not yet support setting', unused === undefined);
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    return this[0].clientHeight;
  },

  // ----------
  // Function: bounds
  bounds: function(unused) {
    Utils.assert('does not yet support setting', unused === undefined);
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    var el = this[0];
    return new Rect(
      parseInt(el.style.left) || el.offsetLeft, 
      parseInt(el.style.top) || el.offsetTop, 
      el.clientWidth,
      el.clientHeight
    );
  },
  
  // ----------
  // Function: data
  data: function(key, value) {
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    var data = this[0].iQData;
    if(value === undefined)
      return (data ? data[key] : null);
    
    if(!data)
      data = this[0].iQData = {};
      
    data[key] = value;
    return this;    
  },
  
  // ----------
  // Function: html
  // TODO: security
  html: function(value) {
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    if(value === undefined)
      return this[0].innerHTML;
      
    this[0].innerHTML = value;
    return this;
  },  
  
  // ----------
  // Function: text
  text: function(value) {
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    if(value === undefined) {
      return this[0].textContent;
    }
      
		return this.empty().append( (this[0] && this[0].ownerDocument || document).createTextNode(value));
  },  
  
  // ----------
  // Function: appendTo
  appendTo: function(selector) {
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    iQ(selector).append(this);
    return this;
  },
  
  // ----------
  // Function: append
  append: function(selector) {
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    var object = iQ(selector);
    Utils.assert('does not yet support multi-objects (or null objects)', object.length == 1);
    this[0].appendChild(object[0]);
    return this;
  },
  
  // ----------
  // Function: css
  css: function(a, b) {
    var properties = null;
    if(typeof a === 'string') {
      if(b === undefined) {
        Utils.assert('retrieval does not support multi-objects (or null objects)', this.length == 1);      
        return window.getComputedStyle(this[0], null).getPropertyValue(a);  
      } else {
        properties = {};
        properties[a] = b;
      }
    } else
      properties = a;

		for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
      iQ.each(properties, function(key, value) {
        if(key == 'left' || key == 'top' || key == 'width' || key == 'height') {
          if(typeof(value) != 'string') 
            value += 'px';
        }
          
        elem.style[key] = value;
      });
    } 
  },
  
  // ----------
  // Function: bind
  bind: function(type, func) {
    Utils.assert('does not support eventData argument', iQ.isFunction(func));
  	for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
      elem.addEventListener(type, func, false);
    }
  },
  
  // ----------
  // Function: unbind
  unbind: function(type, func) {
  	for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
      elem.removeEventListener(type, func, false);
    }
  }
};

// ----------
// Give the init function the iQ prototype for later instantiation
iQ.fn.init.prototype = iQ.fn;

// ----------
// Function: extend
iQ.extend = iQ.fn.extend = function() {
	// copy reference to target object
	var target = arguments[0] || {}, i = 1, length = arguments.length, deep = false, options, name, src, copy;

	// Handle a deep copy situation
	if ( typeof target === "boolean" ) {
		deep = target;
		target = arguments[1] || {};
		// skip the boolean and the target
		i = 2;
	}

	// Handle case when target is a string or something (possible in deep copy)
	if ( typeof target !== "object" && !iQ.isFunction(target) ) {
		target = {};
	}

	// extend iQ itself if only one argument is passed
	if ( length === i ) {
		target = this;
		--i;
	}

	for ( ; i < length; i++ ) {
		// Only deal with non-null/undefined values
		if ( (options = arguments[ i ]) != null ) {
			// Extend the base object
			for ( name in options ) {
				src = target[ name ];
				copy = options[ name ];

				// Prevent never-ending loop
				if ( target === copy ) {
					continue;
				}

				// Recurse if we're merging object literal values or arrays
				if ( deep && copy && ( iQ.isPlainObject(copy) || iQ.isArray(copy) ) ) {
					var clone = src && ( iQ.isPlainObject(src) || iQ.isArray(src) ) ? src
						: iQ.isArray(copy) ? [] : {};

					// Never move original objects, clone them
					target[ name ] = iQ.extend( deep, clone, copy );

				// Don't bring in undefined values
				} else if ( copy !== undefined ) {
					target[ name ] = copy;
				}
			}
		}
	}

	// Return the modified object
	return target;
};

// ##########
// Class: iQ
// Singleton
iQ.extend({
	// -----------
	// Function: isFunction
	// See test/unit/core.js for details concerning isFunction.
	// Since version 1.3, DOM methods and functions like alert
	// aren't supported. They return false on IE (#2968).
	isFunction: function( obj ) {
		return toString.call(obj) === "[object Function]";
	},

  // ----------
  // Function: isArray
	isArray: function( obj ) {
		return toString.call(obj) === "[object Array]";
	},

  // ----------
  // Function: isPlainObject
	isPlainObject: function( obj ) {
		// Must be an Object.
		// Because of IE, we also have to check the presence of the constructor property.
		// Make sure that DOM nodes and window objects don't pass through, as well
		if ( !obj || toString.call(obj) !== "[object Object]" || obj.nodeType || obj.setInterval ) {
			return false;
		}
		
		// Not own constructor property must be Object
		if ( obj.constructor
			&& !hasOwnProperty.call(obj, "constructor")
			&& !hasOwnProperty.call(obj.constructor.prototype, "isPrototypeOf") ) {
			return false;
		}
		
		// Own properties are enumerated firstly, so to speed up,
		// if last one is own, then all properties are own.
	
		var key;
		for ( key in obj ) {}
		
		return key === undefined || hasOwnProperty.call( obj, key );
	},

  // ----------
  // Function: isEmptyObject
	isEmptyObject: function( obj ) {
		for ( var name in obj ) {
			return false;
		}
		return true;
	},

	// ----------
  // Function: each
	// args is for internal usage only
	each: function( object, callback, args ) {
		var name, i = 0,
			length = object.length,
			isObj = length === undefined || iQ.isFunction(object);

		if ( args ) {
			if ( isObj ) {
				for ( name in object ) {
					if ( callback.apply( object[ name ], args ) === false ) {
						break;
					}
				}
			} else {
				for ( ; i < length; ) {
					if ( callback.apply( object[ i++ ], args ) === false ) {
						break;
					}
				}
			}

		// A special, fast, case for the most common use of each
		} else {
			if ( isObj ) {
				for ( name in object ) {
					if ( callback.call( object[ name ], name, object[ name ] ) === false ) {
						break;
					}
				}
			} else {
				for ( var value = object[0];
					i < length && callback.call( value, i, value ) !== false; value = object[++i] ) {}
			}
		}

		return object;
	},
	
  // ----------
  // Function: trim
	trim: function( text ) {
		return (text || "").replace( rtrim, "" );
	},

  // ----------
  // Function: makeArray
	// results is for internal usage only
	makeArray: function( array, results ) {
		var ret = results || [];

		if ( array != null ) {
			// The window, strings (and functions) also have 'length'
			// The extra typeof function check is to prevent crashes
			// in Safari 2 (See: #3039)
			if ( array.length == null || typeof array === "string" || iQ.isFunction(array) || (typeof array !== "function" && array.setInterval) ) {
				push.call( ret, array );
			} else {
				iQ.merge( ret, array );
			}
		}

		return ret;
	},

  // ----------
  // Function: merge
	merge: function( first, second ) {
		var i = first.length, j = 0;

		if ( typeof second.length === "number" ) {
			for ( var l = second.length; j < l; j++ ) {
				first[ i++ ] = second[ j ];
			}
		
		} else {
			while ( second[j] !== undefined ) {
				first[ i++ ] = second[ j++ ];
			}
		}

		first.length = i;

		return first;
	},

  // ----------
  // Function: grep
	grep: function( elems, callback, inv ) {
		var ret = [];

		// Go through the array, only saving the items
		// that pass the validator function
		for ( var i = 0, length = elems.length; i < length; i++ ) {
			if ( !inv !== !callback( elems[ i ], i ) ) {
				ret.push( elems[ i ] );
			}
		}

		return ret;
	}	
});

// ----------
// All iQ objects should point back to these
rootiQ = iQ(document);

// ----------
// Expose iQ to the global object
window.iQ = iQ;

})(window);
