/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// **********
// Title: utils.js

this.EXPORTED_SYMBOLS = ["Point", "Rect", "Range", "Subscribable", "Utils", "MRUList"];

// #########
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

// ##########
// Class: Point
// A simple point.
//
// Constructor: Point
// If a is a Point, creates a copy of it. Otherwise, expects a to be x,
// and creates a Point with it along with y. If either a or y are omitted,
// 0 is used in their place.
this.Point = function Point(a, y) {
  if (Utils.isPoint(a)) {
    this.x = a.x;
    this.y = a.y;
  } else {
    this.x = (Utils.isNumber(a) ? a : 0);
    this.y = (Utils.isNumber(y) ? y : 0);
  }
};

Point.prototype = {
  // ----------
  // Function: toString
  // Prints [Point (x,y)] for debug use
  toString: function Point_toString() {
    return "[Point (" + this.x + "," + this.y + ")]";
  },

  // ----------
  // Function: distance
  // Returns the distance from this point to the given <Point>.
  distance: function Point_distance(point) {
    var ax = this.x - point.x;
    var ay = this.y - point.y;
    return Math.sqrt((ax * ax) + (ay * ay));
  }
};

// ##########
// Class: Rect
// A simple rectangle. Note that in addition to the left and width, it also has
// a right property; changing one affects the others appropriately. Same for the
// vertical properties.
//
// Constructor: Rect
// If a is a Rect, creates a copy of it. Otherwise, expects a to be left,
// and creates a Rect with it along with top, width, and height.
this.Rect = function Rect(a, top, width, height) {
  // Note: perhaps 'a' should really be called 'rectOrLeft'
  if (Utils.isRect(a)) {
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

Rect.prototype = {
  // ----------
  // Function: toString
  // Prints [Rect (left,top,width,height)] for debug use
  toString: function Rect_toString() {
    return "[Rect (" + this.left + "," + this.top + "," +
            this.width + "," + this.height + ")]";
  },

  get right() {
    return this.left + this.width;
  },
  set right(value) {
    this.width = value - this.left;
  },

  get bottom() {
    return this.top + this.height;
  },
  set bottom(value) {
    this.height = value - this.top;
  },

  // ----------
  // Variable: xRange
  // Gives you a new <Range> for the horizontal dimension.
  get xRange() {
    return new Range(this.left, this.right);
  },

  // ----------
  // Variable: yRange
  // Gives you a new <Range> for the vertical dimension.
  get yRange() {
    return new Range(this.top, this.bottom);
  },

  // ----------
  // Function: intersects
  // Returns true if this rectangle intersects the given <Rect>.
  intersects: function Rect_intersects(rect) {
    return (rect.right > this.left &&
            rect.left < this.right &&
            rect.bottom > this.top &&
            rect.top < this.bottom);
  },

  // ----------
  // Function: intersection
  // Returns a new <Rect> with the intersection of this rectangle and the give <Rect>,
  // or null if they don't intersect.
  intersection: function Rect_intersection(rect) {
    var box = new Rect(Math.max(rect.left, this.left), Math.max(rect.top, this.top), 0, 0);
    box.right = Math.min(rect.right, this.right);
    box.bottom = Math.min(rect.bottom, this.bottom);
    if (box.width > 0 && box.height > 0)
      return box;

    return null;
  },

  // ----------
  // Function: contains
  // Returns a boolean denoting if the <Rect> or <Point> is contained inside
  // this rectangle.
  //
  // Parameters
  //  - A <Rect> or a <Point>
  contains: function Rect_contains(a) {
    if (Utils.isPoint(a))
      return (a.x > this.left &&
              a.x < this.right &&
              a.y > this.top &&
              a.y < this.bottom);

    return (a.left >= this.left &&
            a.right <= this.right &&
            a.top >= this.top &&
            a.bottom <= this.bottom);
  },

  // ----------
  // Function: center
  // Returns a new <Point> with the center location of this rectangle.
  center: function Rect_center() {
    return new Point(this.left + (this.width / 2), this.top + (this.height / 2));
  },

  // ----------
  // Function: size
  // Returns a new <Point> with the dimensions of this rectangle.
  size: function Rect_size() {
    return new Point(this.width, this.height);
  },

  // ----------
  // Function: position
  // Returns a new <Point> with the top left of this rectangle.
  position: function Rect_position() {
    return new Point(this.left, this.top);
  },

  // ----------
  // Function: area
  // Returns the area of this rectangle.
  area: function Rect_area() {
    return this.width * this.height;
  },

  // ----------
  // Function: inset
  // Makes the rect smaller (if the arguments are positive) as if a margin is added all around
  // the initial rect, with the margin widths (symmetric) being specified by the arguments.
  //
  // Paramaters
  //  - A <Point> or two arguments: x and y
  inset: function Rect_inset(a, b) {
    if (Utils.isPoint(a)) {
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
  offset: function Rect_offset(a, b) {
    if (Utils.isPoint(a)) {
      this.left += a.x;
      this.top += a.y;
    } else {
      this.left += a;
      this.top += b;
    }
  },

  // ----------
  // Function: equals
  // Returns true if this rectangle is identical to the given <Rect>.
  equals: function Rect_equals(rect) {
    return (rect.left == this.left &&
            rect.top == this.top &&
            rect.width == this.width &&
            rect.height == this.height);
  },

  // ----------
  // Function: union
  // Returns a new <Rect> with the union of this rectangle and the given <Rect>.
  union: function Rect_union(a) {
    var newLeft = Math.min(a.left, this.left);
    var newTop = Math.min(a.top, this.top);
    var newWidth = Math.max(a.right, this.right) - newLeft;
    var newHeight = Math.max(a.bottom, this.bottom) - newTop;
    var newRect = new Rect(newLeft, newTop, newWidth, newHeight);

    return newRect;
  },

  // ----------
  // Function: copy
  // Copies the values of the given <Rect> into this rectangle.
  copy: function Rect_copy(a) {
    this.left = a.left;
    this.top = a.top;
    this.width = a.width;
    this.height = a.height;
  }
};

// ##########
// Class: Range
// A physical interval, with a min and max.
//
// Constructor: Range
// Creates a Range with the given min and max
this.Range = function Range(min, max) {
  if (Utils.isRange(min) && !max) { // if the one variable given is a range, copy it.
    this.min = min.min;
    this.max = min.max;
  } else {
    this.min = min || 0;
    this.max = max || 0;
  }
};

Range.prototype = {
  // ----------
  // Function: toString
  // Prints [Range (min,max)] for debug use
  toString: function Range_toString() {
    return "[Range (" + this.min + "," + this.max + ")]";
  },

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
  // Whether the <Range> contains the given <Range> or value or not.
  //
  // Parameters
  //  - a number or <Range>
  contains: function Range_contains(value) {
    if (Utils.isNumber(value))
      return value >= this.min && value <= this.max;
    if (Utils.isRange(value))
      return value.min >= this.min && value.max <= this.max;
    return false;
  },

  // ----------
  // Function: overlaps
  // Whether the <Range> overlaps with the given <Range> value or not.
  //
  // Parameters
  //  - a number or <Range>
  overlaps: function Range_overlaps(value) {
    if (Utils.isNumber(value))
      return this.contains(value);
    if (Utils.isRange(value))
      return !(value.max < this.min || this.max < value.min);
    return false;
  },

  // ----------
  // Function: proportion
  // Maps the given value to the range [0,1], so that it returns 0 if the value is <= the min,
  // returns 1 if the value >= the max, and returns an interpolated "proportion" in (min, max).
  //
  // Parameters
  //  - a number
  //  - (bool) smooth? If true, a smooth tanh-based function will be used instead of the linear.
  proportion: function Range_proportion(value, smooth) {
    if (value <= this.min)
      return 0;
    if (this.max <= value)
      return 1;

    var proportion = (value - this.min) / this.extent;

    if (smooth) {
      // The ease function ".5+.5*Math.tanh(4*x-2)" is a pretty
      // little graph. It goes from near 0 at x=0 to near 1 at x=1
      // smoothly and beautifully.
      // http://www.wolframalpha.com/input/?i=.5+%2B+.5+*+tanh%28%284+*+x%29+-+2%29
      let tanh = function tanh(x) {
        var e = Math.exp(x);
        return (e - 1/e) / (e + 1/e);
      };

      return .5 - .5 * tanh(2 - 4 * proportion);
    }

    return proportion;
  },

  // ----------
  // Function: scale
  // Takes the given value in [0,1] and maps it to the associated value on the Range.
  //
  // Parameters
  //  - a number in [0,1]
  scale: function Range_scale(value) {
    if (value > 1)
      value = 1;
    if (value < 0)
      value = 0;
    return this.min + this.extent * value;
  }
};

// ##########
// Class: Subscribable
// A mix-in for allowing objects to collect subscribers for custom events.
this.Subscribable = function Subscribable() {
  this.subscribers = null;
};

Subscribable.prototype = {
  // ----------
  // Function: addSubscriber
  // The given callback will be called when the Subscribable fires the given event.
  addSubscriber: function Subscribable_addSubscriber(eventName, callback) {
    try {
      Utils.assertThrow(typeof callback == "function", "callback must be a function");
      Utils.assertThrow(eventName && typeof eventName == "string",
          "eventName must be a non-empty string");
    } catch(e) {
      Utils.log(e);
      return;
    }

    if (!this.subscribers)
      this.subscribers = {};

    if (!this.subscribers[eventName])
      this.subscribers[eventName] = [];

    let subscribers = this.subscribers[eventName];
    if (subscribers.indexOf(callback) == -1)
      subscribers.push(callback);
  },

  // ----------
  // Function: removeSubscriber
  // Removes the subscriber associated with the event for the given callback.
  removeSubscriber: function Subscribable_removeSubscriber(eventName, callback) {
    try {
      Utils.assertThrow(typeof callback == "function", "callback must be a function");
      Utils.assertThrow(eventName && typeof eventName == "string",
          "eventName must be a non-empty string");
    } catch(e) {
      Utils.log(e);
      return;
    }

    if (!this.subscribers || !this.subscribers[eventName])
      return;

    let subscribers = this.subscribers[eventName];
    let index = subscribers.indexOf(callback);

    if (index > -1)
      subscribers.splice(index, 1);
  },

  // ----------
  // Function: _sendToSubscribers
  // Internal routine. Used by the Subscribable to fire events.
  _sendToSubscribers: function Subscribable__sendToSubscribers(eventName, eventInfo) {
    try {
      Utils.assertThrow(eventName && typeof eventName == "string",
          "eventName must be a non-empty string");
    } catch(e) {
      Utils.log(e);
      return;
    }

    if (!this.subscribers || !this.subscribers[eventName])
      return;

    let subsCopy = this.subscribers[eventName].concat();
    subsCopy.forEach(function (callback) {
      try {
        callback(this, eventInfo);
      } catch(e) {
        Utils.log(e);
      }
    }, this);
  }
};

// ##########
// Class: Utils
// Singelton with common utility functions.
this.Utils = {
  // ----------
  // Function: toString
  // Prints [Utils] for debug use
  toString: function Utils_toString() {
    return "[Utils]";
  },

  // ___ Logging
  useConsole: true, // as opposed to dump
  showTime: false,

  // ----------
  // Function: log
  // Prints the given arguments to the JavaScript error console as a message.
  // Pass as many arguments as you want, it'll print them all.
  log: function Utils_log() {
    var text = this.expandArgumentsForLog(arguments);
    var prefix = this.showTime ? Date.now() + ': ' : '';
    if (this.useConsole)    
      Services.console.logStringMessage(prefix + text);
    else
      dump(prefix + text + '\n');
  },

  // ----------
  // Function: error
  // Prints the given arguments to the JavaScript error console as an error.
  // Pass as many arguments as you want, it'll print them all.
  error: function Utils_error() {
    var text = this.expandArgumentsForLog(arguments);
    var prefix = this.showTime ? Date.now() + ': ' : '';
    if (this.useConsole)    
      Cu.reportError(prefix + "tabview error: " + text);
    else
      dump(prefix + "TABVIEW ERROR: " + text + '\n');
  },

  // ----------
  // Function: trace
  // Prints the given arguments to the JavaScript error console as a message,
  // along with a full stack trace.
  // Pass as many arguments as you want, it'll print them all.
  trace: function Utils_trace() {
    var text = this.expandArgumentsForLog(arguments);

    // cut off the first line of the stack trace, because that's just this function.
    let stack = Error().stack.split("\n").slice(1);

    // if the caller was assert, cut out the line for the assert function as well.
    if (stack[0].indexOf("Utils_assert(") == 0)
      stack.splice(0, 1);

    this.log('trace: ' + text + '\n' + stack.join("\n"));
  },

  // ----------
  // Function: assert
  // Prints a stack trace along with label (as a console message) if condition is false.
  assert: function Utils_assert(condition, label) {
    if (!condition) {
      let text;
      if (typeof label != 'string')
        text = 'badly formed assert';
      else
        text = "tabview assert: " + label;

      this.trace(text);
    }
  },

  // ----------
  // Function: assertThrow
  // Throws label as an exception if condition is false.
  assertThrow: function Utils_assertThrow(condition, label) {
    if (!condition) {
      let text;
      if (typeof label != 'string')
        text = 'badly formed assert';
      else
        text = "tabview assert: " + label;

      // cut off the first line of the stack trace, because that's just this function.
      let stack = Error().stack.split("\n").slice(1);

      throw text + "\n" + stack.join("\n");
    }
  },

  // ----------
  // Function: expandObject
  // Prints the given object to a string, including all of its properties.
  expandObject: function Utils_expandObject(obj) {
    var s = obj + ' = {';
    for (let prop in obj) {
      let value;
      try {
        value = obj[prop];
      } catch(e) {
        value = '[!!error retrieving property]';
      }

      s += prop + ': ';
      if (typeof value == 'string')
        s += '\'' + value + '\'';
      else if (typeof value == 'function')
        s += 'function';
      else
        s += value;

      s += ', ';
    }
    return s + '}';
  },

  // ----------
  // Function: expandArgumentsForLog
  // Expands all of the given args (an array) into a single string.
  expandArgumentsForLog: function Utils_expandArgumentsForLog(args) {
    var that = this;
    return Array.map(args, function(arg) {
      return typeof arg == 'object' ? that.expandObject(arg) : arg;
    }).join('; ');
  },

  // ___ Misc

  // ----------
  // Function: isLeftClick
  // Given a DOM mouse event, returns true if it was for the left mouse button.
  isLeftClick: function Utils_isLeftClick(event) {
    return event.button == 0;
  },

  // ----------
  // Function: isMiddleClick
  // Given a DOM mouse event, returns true if it was for the middle mouse button.
  isMiddleClick: function Utils_isMiddleClick(event) {
    return event.button == 1;
  },

  // ----------
  // Function: isRightClick
  // Given a DOM mouse event, returns true if it was for the right mouse button.
  isRightClick: function Utils_isRightClick(event) {
    return event.button == 2;
  },

  // ----------
  // Function: isDOMElement
  // Returns true if the given object is a DOM element.
  isDOMElement: function Utils_isDOMElement(object) {
    return object instanceof Ci.nsIDOMElement;
  },

  // ----------
  // Function: isValidXULTab
  // A xulTab is valid if it has not been closed,
  // and it has not been removed from the DOM
  // Returns true if the tab is valid.
  isValidXULTab: function Utils_isValidXULTab(xulTab) {
    return !xulTab.closing && xulTab.parentNode;
  },

  // ----------
  // Function: isNumber
  // Returns true if the argument is a valid number.
  isNumber: function Utils_isNumber(n) {
    return typeof n == 'number' && !isNaN(n);
  },

  // ----------
  // Function: isRect
  // Returns true if the given object (r) looks like a <Rect>.
  isRect: function Utils_isRect(r) {
    return (r &&
            this.isNumber(r.left) &&
            this.isNumber(r.top) &&
            this.isNumber(r.width) &&
            this.isNumber(r.height));
  },

  // ----------
  // Function: isRange
  // Returns true if the given object (r) looks like a <Range>.
  isRange: function Utils_isRange(r) {
    return (r &&
            this.isNumber(r.min) &&
            this.isNumber(r.max));
  },

  // ----------
  // Function: isPoint
  // Returns true if the given object (p) looks like a <Point>.
  isPoint: function Utils_isPoint(p) {
    return (p && this.isNumber(p.x) && this.isNumber(p.y));
  },

  // ----------
  // Function: isPlainObject
  // Check to see if an object is a plain object (created using "{}" or "new Object").
  isPlainObject: function Utils_isPlainObject(obj) {
    // Must be an Object.
    // Make sure that DOM nodes and window objects don't pass through, as well
    if (!obj || Object.prototype.toString.call(obj) !== "[object Object]" ||
       obj.nodeType || obj.setInterval) {
      return false;
    }

    // Not own constructor property must be Object
    const hasOwnProperty = Object.prototype.hasOwnProperty;

    if (obj.constructor &&
       !hasOwnProperty.call(obj, "constructor") &&
       !hasOwnProperty.call(obj.constructor.prototype, "isPrototypeOf")) {
      return false;
    }

    // Own properties are enumerated firstly, so to speed up,
    // if last one is own, then all properties are own.

    var key;
    for (key in obj) {}

    return key === undefined || hasOwnProperty.call(obj, key);
  },

  // ----------
  // Function: isEmptyObject
  // Returns true if the given object has no members.
  isEmptyObject: function Utils_isEmptyObject(obj) {
    for (let name in obj)
      return false;
    return true;
  },

  // ----------
  // Function: copy
  // Returns a copy of the argument. Note that this is a shallow copy; if the argument
  // has properties that are themselves objects, those properties will be copied by reference.
  copy: function Utils_copy(value) {
    if (value && typeof value == 'object') {
      if (Array.isArray(value))
        return this.extend([], value);
      return this.extend({}, value);
    }
    return value;
  },

  // ----------
  // Function: merge
  // Merge two array-like objects into the first and return it.
  merge: function Utils_merge(first, second) {
    Array.forEach(second, el => Array.push(first, el));
    return first;
  },

  // ----------
  // Function: extend
  // Pass several objects in and it will combine them all into the first object and return it.
  extend: function Utils_extend() {

    // copy reference to target object
    let target = arguments[0] || {};
    // Deep copy is not supported
    if (typeof target === "boolean") {
      this.assert(false, "The first argument of extend cannot be a boolean." +
          "Deep copy is not supported.");
      return target;
    }

    // Back when this was in iQ + iQ.fn, so you could extend iQ objects with it.
    // This is no longer supported.
    let length = arguments.length;
    if (length === 1) {
      this.assert(false, "Extending the iQ prototype using extend is not supported.");
      return target;
    }

    // Handle case when target is a string or something
    if (typeof target != "object" && typeof target != "function") {
      target = {};
    }

    for (let i = 1; i < length; i++) {
      // Only deal with non-null/undefined values
      let options = arguments[i];
      if (options != null) {
        // Extend the base object
        for (let name in options) {
          let copy = options[name];

          // Prevent never-ending loop
          if (target === copy)
            continue;

          if (copy !== undefined)
            target[name] = copy;
        }
      }
    }

    // Return the modified object
    return target;
  },

  // ----------
  // Function: attempt
  // Tries to execute a number of functions. Returns immediately the return
  // value of the first non-failed function without executing successive
  // functions, or null.
  attempt: function Utils_attempt() {
    let args = arguments;

    for (let i = 0; i < args.length; i++) {
      try {
        return args[i]();
      } catch (e) {}
    }

    return null;
  }
};

// ##########
// Class: MRUList
// A most recently used list.
//
// Constructor: MRUList
// If a is an array of entries, creates a copy of it.
this.MRUList = function MRUList(a) {
  if (Array.isArray(a))
    this._list = a.concat();
  else
    this._list = [];
};

MRUList.prototype = {
  // ----------
  // Function: toString
  // Prints [List (entry1, entry2, ...)] for debug use
  toString: function MRUList_toString() {
    return "[List (" + this._list.join(", ") + ")]";
  },

  // ----------
  // Function: update
  // Updates/inserts the given entry as the most recently used one in the list.
  update: function MRUList_update(entry) {
    this.remove(entry);
    this._list.unshift(entry);
  },

  // ----------
  // Function: remove
  // Removes the given entry from the list.
  remove: function MRUList_remove(entry) {
    let index = this._list.indexOf(entry);
    if (index > -1)
      this._list.splice(index, 1);
  },

  // ----------
  // Function: peek
  // Returns the most recently used entry.  If a filter exists, gets the most 
  // recently used entry which matches the filter.
  peek: function MRUList_peek(filter) {
    let match = null;
    if (filter && typeof filter == "function")
      this._list.some(function MRUList_peek_getEntry(entry) {
        if (filter(entry)) {
          match = entry
          return true;
        }
        return false;
      });
    else 
      match = this._list.length > 0 ? this._list[0] : null;

    return match;
  },
};

