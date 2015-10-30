/**
 * @license OpenTok.js v2.6.8 fae7901 HEAD
 *
 * Copyright (c) 2010-2015 TokBox, Inc.
 * Subject to the applicable Software Development Kit (SDK) License Agreement:
 * https://tokbox.com/support/sdk_license
 *
 * Date: October 28 03:45:23 2015
 */
!(function(window) {

!(function(window, OTHelpers, undefined) {

/**
 * @license  Common JS Helpers on OpenTok 0.4.1 259ca46 v0.4.1-branch
 * http://www.tokbox.com/
 *
 * Copyright (c) 2015 TokBox, Inc.
 *
 * Date: October 28 03:45:12 2015
 *
 */


// OT Helper Methods
//
// helpers.js                           <- the root file
// helpers/lib/{helper topic}.js        <- specialised helpers for specific tasks/topics
//                                          (i.e. video, dom, etc)
//
// @example Getting a DOM element by it's id
//  var element = OTHelpers('#domId');
//
//

/*jshint browser:true, smarttabs:true*/

// Short cuts to commonly accessed prototypes
var prototypeSlice = Array.prototype.slice;


// RegEx to detect CSS Id selectors
var detectIdSelectors = /^#([\w-]*)$/;

// The top-level namespace, also performs basic DOMElement selecting.
//
// @example Get the DOM element with the id of 'domId'
//   OTHelpers('#domId')
//
// @example Get all video elements
//   OTHelpers('video')
//
// @example Get all elements with the class name of 'foo'
//   OTHelpers('.foo')
//
// @example Get all elements with the class name of 'foo',
// and do something with the first.
//   var collection = OTHelpers('.foo');
//   console.log(collection.first);
//
//
// The second argument is the context, that is document or parent Element, to
// select from.
//
// @example Get a video element within the element with the id of 'domId'
//   OTHelpers('video', OTHelpers('#domId'))
//
//
//
// OTHelpers will accept any of the following and return a collection:
//   OTHelpers()
//   OTHelpers('css selector', optionalParentNode)
//   OTHelpers(DomNode)
//   OTHelpers([array of DomNode])
//
// The collection is a ElementCollection object, see the ElementCollection docs for usage info.
//
var OTHelpers = function(selector, context) {
  var results = [];

  if (typeof(selector) === 'string') {
    var idSelector = detectIdSelectors.exec(selector);
    context = context || document;

    if (idSelector && idSelector[1]) {
      var element = context.getElementById(idSelector[1]);
      if (element) results.push(element);
    }
    else {
      results = context.querySelectorAll(selector);
    }
  }
  else if (selector &&
            (selector.nodeType || window.XMLHttpRequest && selector instanceof XMLHttpRequest)) {

    // allow OTHelpers(DOMNode) and OTHelpers(xmlHttpRequest)
    results = [selector];
    context = selector;
  }
  else if (OTHelpers.isArray(selector)) {
    results = selector.slice();
    context  = null;
  }

  return new ElementCollection(results, context);
};

// alias $ to OTHelpers for internal use. This is not exposed outside of OTHelpers.
var $ = OTHelpers;

// A helper for converting a NodeList to a JS Array
var nodeListToArray = function nodeListToArray (nodes) {
  if ($.env.name !== 'IE' || $.env.version > 9) {
    return prototypeSlice.call(nodes);
  }

  // IE 9 and below call use Array.prototype.slice.call against
  // a NodeList, hence the following
  var array = [];

  for (var i=0, num=nodes.length; i<num; ++i) {
    array.push(nodes[i]);
  }

  return array;
};

// ElementCollection contains the result of calling OTHelpers.
//
// It has the following properties:
//   length
//   first
//   last
//
// It also has a get method that can be used to access elements in the collection
//
//   var videos = OTHelpers('video');
//   var firstElement = videos.get(0);               // identical to videos.first
//   var lastElement = videos.get(videos.length-1);  // identical to videos.last
//   var allVideos = videos.get();
//
//
// The collection also implements the following helper methods:
//   some, forEach, map, filter, find,
//   appendTo, after, before, remove, empty,
//   attr, center, width, height,
//   addClass, removeClass, hasClass, toggleClass,
//   on, off, once,
//   observeStyleChanges, observeNodeOrChildNodeRemoval
//
// Mostly the usage should be obvious. When in doubt, assume it functions like
// the jQuery equivalent.
//
var ElementCollection = function ElementCollection (elems, context) {
  var elements = nodeListToArray(elems);
  this.context = context;
  this.toArray = function() { return elements; };

  this.length = elements.length;
  this.first = elements[0];
  this.last = elements[elements.length-1];

  this.get = function(index) {
    if (index === void 0) return elements;
    return elements[index];
  };
};


ElementCollection.prototype.some = function (iter, context) {
  return $.some(this.get(), iter, context);
};

ElementCollection.prototype.forEach = function(fn, context) {
  $.forEach(this.get(), fn, context);
  return this;
};

ElementCollection.prototype.map = function(fn, context) {
  return new ElementCollection($.map(this.get(), fn, context), this.context);
};

ElementCollection.prototype.filter = function(fn, context) {
  return new ElementCollection($.filter(this.get(), fn, context), this.context);
};

ElementCollection.prototype.find = function(selector) {
  return $(selector, this.first);
};


var previousOTHelpers = window.OTHelpers;

window.OTHelpers = OTHelpers;

// A guard to detect when IE has performed cleans on unload
window.___othelpers = true;

OTHelpers.keys = Object.keys || function(object) {
  var keys = [], hasOwnProperty = Object.prototype.hasOwnProperty;
  for(var key in object) {
    if(hasOwnProperty.call(object, key)) {
      keys.push(key);
    }
  }
  return keys;
};

var _each = Array.prototype.forEach || function(iter, ctx) {
  for(var idx = 0, count = this.length || 0; idx < count; ++idx) {
    if(idx in this) {
      iter.call(ctx, this[idx], idx);
    }
  }
};

OTHelpers.forEach = function(array, iter, ctx) {
  return _each.call(array, iter, ctx);
};

var _map = Array.prototype.map || function(iter, ctx) {
  var collect = [];
  _each.call(this, function(item, idx) {
    collect.push(iter.call(ctx, item, idx));
  });
  return collect;
};

OTHelpers.map = function(array, iter) {
  return _map.call(array, iter);
};

var _filter = Array.prototype.filter || function(iter, ctx) {
  var collect = [];
  _each.call(this, function(item, idx) {
    if(iter.call(ctx, item, idx)) {
      collect.push(item);
    }
  });
  return collect;
};

OTHelpers.filter = function(array, iter, ctx) {
  return _filter.call(array, iter, ctx);
};

var _some = Array.prototype.some || function(iter, ctx) {
  var any = false;
  for(var idx = 0, count = this.length || 0; idx < count; ++idx) {
    if(idx in this) {
      if(iter.call(ctx, this[idx], idx)) {
        any = true;
        break;
      }
    }
  }
  return any;
};

OTHelpers.find = function(array, iter, ctx) {
  if (!$.isFunction(iter)) {
    throw new TypeError('iter must be a function');
  }

  var any = void 0;
  for(var idx = 0, count = array.length || 0; idx < count; ++idx) {
    if(idx in array) {
      if(iter.call(ctx, array[idx], idx)) {
        any = array[idx];
        break;
      }
    }
  }
  return any;
};

OTHelpers.findIndex = function(array, iter, ctx) {
  if (!$.isFunction(iter)) {
    throw new TypeError('iter must be a function');
  }

  for (var i = 0, count = array.length || 0; i < count; ++i) {
    if (i in array && iter.call(ctx, array[i], i, array)) {
      return i;
    }
  }

  return -1;
};


OTHelpers.some = function(array, iter, ctx) {
  return _some.call(array, iter, ctx);
};

var _indexOf = Array.prototype.indexOf || function(searchElement, fromIndex) {
  var i,
      pivot = (fromIndex) ? fromIndex : 0,
      length;

  if (!this) {
    throw new TypeError();
  }

  length = this.length;

  if (length === 0 || pivot >= length) {
    return -1;
  }

  if (pivot < 0) {
    pivot = length - Math.abs(pivot);
  }

  for (i = pivot; i < length; i++) {
    if (this[i] === searchElement) {
      return i;
    }
  }
  return -1;
};

OTHelpers.arrayIndexOf = function(array, searchElement, fromIndex) {
  return _indexOf.call(array, searchElement, fromIndex);
};

var _bind = Function.prototype.bind || function() {
  var args = prototypeSlice.call(arguments),
      ctx = args.shift(),
      fn = this;
  return function() {
    return fn.apply(ctx, args.concat(prototypeSlice.call(arguments)));
  };
};

OTHelpers.bind = function() {
  var args = prototypeSlice.call(arguments),
      fn = args.shift();
  return _bind.apply(fn, args);
};

var _trim = String.prototype.trim || function() {
  return this.replace(/^\s+|\s+$/g, '');
};

OTHelpers.trim = function(str) {
  return _trim.call(str);
};

OTHelpers.noConflict = function() {
  OTHelpers.noConflict = function() {
    return OTHelpers;
  };
  window.OTHelpers = previousOTHelpers;
  return OTHelpers;
};

OTHelpers.isNone = function(obj) {
  return obj === void 0 || obj === null;
};

OTHelpers.isObject = function(obj) {
  return obj === Object(obj);
};

OTHelpers.isFunction = function(obj) {
  return !!obj && (obj.toString().indexOf('()') !== -1 ||
    Object.prototype.toString.call(obj) === '[object Function]');
};

OTHelpers.isArray = OTHelpers.isFunction(Array.isArray) && Array.isArray ||
  function (vArg) {
    return Object.prototype.toString.call(vArg) === '[object Array]';
  };

OTHelpers.isEmpty = function(obj) {
  if (obj === null || obj === void 0) return true;
  if (OTHelpers.isArray(obj) || typeof(obj) === 'string') return obj.length === 0;

  // Objects without enumerable owned properties are empty.
  for (var key in obj) {
    if (obj.hasOwnProperty(key)) return false;
  }

  return true;
};

// Extend a target object with the properties from one or
// more source objects
//
// @example:
//    dest = OTHelpers.extend(dest, source1, source2, source3);
//
OTHelpers.extend = function(/* dest, source1[, source2, ..., , sourceN]*/) {
  var sources = prototypeSlice.call(arguments),
      dest = sources.shift();

  OTHelpers.forEach(sources, function(source) {
    for (var key in source) {
      dest[key] = source[key];
    }
  });

  return dest;
};

// Ensures that the target object contains certain defaults.
//
// @example
//   var options = OTHelpers.defaults(options, {
//     loading: true     // loading by default
//   });
//
OTHelpers.defaults = function(/* dest, defaults1[, defaults2, ..., , defaultsN]*/) {
  var sources = prototypeSlice.call(arguments),
      dest = sources.shift();

  OTHelpers.forEach(sources, function(source) {
    for (var key in source) {
      if (dest[key] === void 0) dest[key] = source[key];
    }
  });

  return dest;
};

OTHelpers.clone = function(obj) {
  if (!OTHelpers.isObject(obj)) return obj;
  return OTHelpers.isArray(obj) ? obj.slice() : OTHelpers.extend({}, obj);
};



// Handy do nothing function
OTHelpers.noop = function() {};


// Returns the number of millisceonds since the the UNIX epoch, this is functionally
// equivalent to executing new Date().getTime().
//
// Where available, we use 'performance.now' which is more accurate and reliable,
// otherwise we default to new Date().getTime().
OTHelpers.now = (function() {
  var performance = window.performance || {},
      navigationStart,
      now =  performance.now       ||
             performance.mozNow    ||
             performance.msNow     ||
             performance.oNow      ||
             performance.webkitNow;

  if (now) {
    now = OTHelpers.bind(now, performance);
    navigationStart = performance.timing.navigationStart;

    return  function() { return navigationStart + now(); };
  } else {
    return function() { return new Date().getTime(); };
  }
})();

OTHelpers.canDefineProperty = true;

try {
  Object.defineProperty({}, 'x', {});
} catch (err) {
  OTHelpers.canDefineProperty = false;
}

// A helper for defining a number of getters at once.
//
// @example: from inside an object
//   OTHelpers.defineGetters(this, {
//     apiKey: function() { return _apiKey; },
//     token: function() { return _token; },
//     connected: function() { return this.is('connected'); },
//     capabilities: function() { return _socket.capabilities; },
//     sessionId: function() { return _sessionId; },
//     id: function() { return _sessionId; }
//   });
//
OTHelpers.defineGetters = function(self, getters, enumerable) {
  var propsDefinition = {};

  if (enumerable === void 0) enumerable = false;

  for (var key in getters) {
    propsDefinition[key] = {
      get: getters[key],
      enumerable: enumerable
    };
  }

  OTHelpers.defineProperties(self, propsDefinition);
};

var generatePropertyFunction = function(object, getter, setter) {
  if(getter && !setter) {
    return function() {
      return getter.call(object);
    };
  } else if(getter && setter) {
    return function(value) {
      if(value !== void 0) {
        setter.call(object, value);
      }
      return getter.call(object);
    };
  } else {
    return function(value) {
      if(value !== void 0) {
        setter.call(object, value);
      }
    };
  }
};

OTHelpers.defineProperties = function(object, getterSetters) {
  for (var key in getterSetters) {
    object[key] = generatePropertyFunction(object, getterSetters[key].get,
      getterSetters[key].set);
  }
};


// Polyfill Object.create for IE8
//
// See https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/create
//
if (!Object.create) {
  Object.create = function (o) {
    if (arguments.length > 1) {
      throw new Error('Object.create implementation only accepts the first parameter.');
    }
    function F() {}
    F.prototype = o;
    return new F();
  };
}

// These next bits are included from Underscore.js. The original copyright
// notice is below.
//
// http://underscorejs.org
// (c) 2009-2011 Jeremy Ashkenas, DocumentCloud Inc.
// (c) 2011-2013 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
// Underscore may be freely distributed under the MIT license.

// Invert the keys and values of an object. The values must be serializable.
OTHelpers.invert = function(obj) {
  var result = {};
  for (var key in obj) if (obj.hasOwnProperty(key)) result[obj[key]] = key;
  return result;
};


// A helper for the common case of making a simple promise that is either
// resolved or rejected straight away.
//
// If the +err+ param is provide then the promise will be rejected, otherwise
// it will resolve.
//
OTHelpers.makeSimplePromise = function(err) {
  return new OTHelpers.RSVP.Promise(function(resolve, reject) {
    if (err === void 0) {
      resolve();
    } else {
      reject(err);
    }
  });
};
// tb_require('../../../helpers.js')

/* exported EventableEvent */

OTHelpers.Event = function() {
  return function (type, cancelable) {
    this.type = type;
    this.cancelable = cancelable !== undefined ? cancelable : true;

    var _defaultPrevented = false;

    this.preventDefault = function() {
      if (this.cancelable) {
        _defaultPrevented = true;
      } else {
        OTHelpers.warn('Event.preventDefault :: Trying to preventDefault ' +
          'on an Event that isn\'t cancelable');
      }
    };

    this.isDefaultPrevented = function() {
      return _defaultPrevented;
    };
  };
};
/*jshint browser:true, smarttabs:true*/

// tb_require('../../helpers.js')

OTHelpers.statable = function(self, possibleStates, initialState, stateChanged,
  stateChangedFailed) {
  var previousState,
      currentState = self.currentState = initialState;

  var setState = function(state) {
    if (currentState !== state) {
      if (OTHelpers.arrayIndexOf(possibleStates, state) === -1) {
        if (stateChangedFailed && OTHelpers.isFunction(stateChangedFailed)) {
          stateChangedFailed('invalidState', state);
        }
        return;
      }

      self.previousState = previousState = currentState;
      self.currentState = currentState = state;
      if (stateChanged && OTHelpers.isFunction(stateChanged)) stateChanged(state, previousState);
    }
  };


  // Returns a number of states and returns true if the current state
  // is any of them.
  //
  // @example
  // if (this.is('connecting', 'connected')) {
  //   // do some stuff
  // }
  //
  self.is = function (/* state0:String, state1:String, ..., stateN:String */) {
    return OTHelpers.arrayIndexOf(arguments, currentState) !== -1;
  };


  // Returns a number of states and returns true if the current state
  // is none of them.
  //
  // @example
  // if (this.isNot('connecting', 'connected')) {
  //   // do some stuff
  // }
  //
  self.isNot = function (/* state0:String, state1:String, ..., stateN:String */) {
    return OTHelpers.arrayIndexOf(arguments, currentState) === -1;
  };

  return setState;
};

/*jshint browser:true, smarttabs:true */

// tb_require('../helpers.js')


var getErrorLocation;

// Properties that we'll acknowledge from the JS Error object
var safeErrorProps = [
  'description',
  'fileName',
  'lineNumber',
  'message',
  'name',
  'number',
  'stack'
];


// OTHelpers.Error
//
// A construct to contain error information that also helps with extracting error
// context, such as stack trace.
//
// @constructor
// @memberof OTHelpers
// @method Error
//
// @param {String} message
//      Optional. The error message
//
// @param {Object} props
//      Optional. A dictionary of properties containing extra Error info.
//
//
// @example Create a simple error with juts a custom message
//   var error = new OTHelpers.Error('Something Broke!');
//   error.message === 'Something Broke!';
//
// @example Create an Error with a message and a name
//   var error = new OTHelpers.Error('Something Broke!', 'FooError');
//   error.message === 'Something Broke!';
//   error.name === 'FooError';
//
// @example Create an Error with a message, name, and custom properties
//   var error = new OTHelpers.Error('Something Broke!', 'FooError', {
//     foo: 'bar',
//     listOfImportantThings: [1,2,3,4]
//   });
//   error.message === 'Something Broke!';
//   error.name === 'FooError';
//   error.foo === 'bar';
//   error.listOfImportantThings == [1,2,3,4];
//
// @example Create an Error from a Javascript Error
//   var error = new OTHelpers.Error(domSyntaxError);
//   error.message === domSyntaxError.message;
//   error.name === domSyntaxError.name === 'SyntaxError';
//   // ...continues for each properties of domSyntaxError
//
// @references
// * https://code.google.com/p/v8/wiki/JavaScriptStackTraceApi
// * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Error/Stack
// * http://www.w3.org/TR/dom/#interface-domerror
//
//
// @todo
// * update usage in OTMedia
// * replace error handling in OT.js
// * normalise stack behaviour under Chrome/Node/Safari with other browsers
// * unit test for stack parsing
//
//
OTHelpers.Error = function (message, name, props) {
  switch (arguments.length) {
  case 1:
    if ($.isObject(message)) {
      props = message;
      name = void 0;
      message = void 0;
    }
    // Otherwise it's the message
    break;

  case 2:
    if ($.isObject(name)) {
      props = name;
      name = void 0;
    }
    // Otherwise name is actually the name

    break;
  }

  if ( props instanceof Error) {
    // Special handling of this due to Chrome weirdness. It seems that
    // properties of the Error object, and it's children, are not
    // enumerable in Chrome?
    for (var i = 0, num = safeErrorProps.length; i < num; ++i) {
      this[safeErrorProps[i]] = props[safeErrorProps[i]];
    }
  }
  else if ( $.isObject(props)) {
    // Use an custom properties that are provided
    for (var key in props) {
      if (props.hasOwnProperty(key)) {
        this[key] = props[key];
      }
    }
  }

  // If any of the fundamental properties are missing then try and
  // extract them.
  if ( !(this.fileName && this.lineNumber && this.columnNumber && this.stack) ) {
    var err = getErrorLocation();

    if (!this.fileName && err.fileName) {
      this.fileName = err.fileName;
    }

    if (!this.lineNumber && err.lineNumber) {
      this.lineNumber = err.lineNumber;
    }

    if (!this.columnNumber && err.columnNumber) {
      this.columnNumber = err.columnNumber;
    }

    if (!this.stack && err.stack) {
      this.stack = err.stack;
    }
  }

  if (!this.message && message) this.message = message;
  if (!this.name && name) this.name = name;
};

OTHelpers.Error.prototype.toString =
OTHelpers.Error.prototype.valueOf = function() {
  var locationDetails = '';
  if (this.fileName) locationDetails += ' ' + this.fileName;
  if (this.lineNumber) {
    locationDetails += ' ' + this.lineNumber;
    if (this.columnNumber) locationDetails += ':' + this.columnNumber;
  }

  return '<' + (this.name ? this.name + ' ' : '') + this.message + locationDetails + '>';
};


// Normalise err.stack so that it is the same format as the other browsers
// We skip the first two frames so that we don't capture getErrorLocation() and
// the callee.
//
// Used by Environments that support the StackTrace API. (Chrome, Node, Opera)
//
var prepareStackTrace = function prepareStackTrace (_, stack){
  return $.map(stack.slice(2), function(frame) {
    var _f = {
      fileName: frame.getFileName(),
      linenumber: frame.getLineNumber(),
      columnNumber: frame.getColumnNumber()
    };

    if (frame.getFunctionName()) _f.functionName = frame.getFunctionName();
    if (frame.getMethodName()) _f.methodName = frame.getMethodName();
    if (frame.getThis()) _f.self = frame.getThis();

    return _f;
  });
};


// Black magic to retrieve error location info for various environments
getErrorLocation = function getErrorLocation () {
  var info = {},
      callstack,
      errLocation,
      err;

  switch ($.env.name) {
  case 'Firefox':
  case 'Safari':
  case 'IE':

    if ($.env.name !== 'IE') {
      err = new Error();
    }
    else {
      try {
        window.call.js.is.explody;
      }
      catch(e) { err = e; }
    }

    callstack = (err.stack || '').split('\n');

    //Remove call to getErrorLocation() and the callee
    callstack.shift();
    callstack.shift();

    info.stack = callstack;

    if ($.env.name === 'IE') {
      // IE also includes the error message in it's stack trace
      info.stack.shift();

      // each line begins with some amounts of spaces and 'at', we remove
      // these to normalise with the other browsers.
      info.stack = $.map(callstack, function(call) {
        return call.replace(/^\s+at\s+/g, '');
      });
    }

    errLocation = /@(.+?):([0-9]+)(:([0-9]+))?$/.exec(callstack[0]);
    if (errLocation) {
      info.fileName = errLocation[1];
      info.lineNumber = parseInt(errLocation[2], 10);
      if (errLocation.length > 3) info.columnNumber = parseInt(errLocation[4], 10);
    }
    break;

  case 'Chrome':
  case 'Node':
  case 'Opera':
    var currentPST = Error.prepareStackTrace;
    Error.prepareStackTrace = prepareStackTrace;
    err = new Error();
    info.stack = err.stack;
    Error.prepareStackTrace = currentPST;

    var topFrame = info.stack[0];
    info.lineNumber = topFrame.lineNumber;
    info.columnNumber = topFrame.columnNumber;
    info.fileName = topFrame.fileName;
    if (topFrame.functionName) info.functionName = topFrame.functionName;
    if (topFrame.methodName) info.methodName = topFrame.methodName;
    if (topFrame.self) info.self = topFrame.self;
    break;

  default:
    err = new Error();
    if (err.stack) info.stack = err.stack.split('\n');
    break;
  }

  if (err.message) info.message = err.message;
  return info;
};


/*jshint browser:true, smarttabs:true*/
/* global process */

// tb_require('../helpers.js')


// OTHelpers.env
//
// Contains information about the current environment.
// * **OTHelpers.env.name** The name of the Environment (Chrome, FF, Node, etc)
// * **OTHelpers.env.version** Usually a Float, except in Node which uses a String
// * **OTHelpers.env.userAgent** The raw user agent
// * **OTHelpers.env.versionGreaterThan** A helper method that returns true if the
// current version is greater than the argument
//
// Example
//     if (OTHelpers.env.versionGreaterThan('0.10.30')) {
//       // do something
//     }
//
(function() {
  // @todo make exposing userAgent unnecessary
  var version = -1;

  // Returns true if otherVersion is greater than the current environment
  // version.
  var versionGEThan = function versionGEThan (otherVersion) {
    if (otherVersion === version) return true;

    if (typeof(otherVersion) === 'number' && typeof(version) === 'number') {
      return otherVersion > version;
    }

    // The versions have multiple components (i.e. 0.10.30) and
    // must be compared piecewise.
    // Note: I'm ignoring the case where one version has multiple
    // components and the other doesn't.
    var v1 = otherVersion.split('.'),
        v2 = version.split('.'),
        versionLength = (v1.length > v2.length ? v2 : v1).length;

    for (var i = 0; i < versionLength; ++i) {
      if (parseInt(v1[i], 10) > parseInt(v2[i], 10)) {
        return true;
      }
    }

    // Special case, v1 has extra components but the initial components
    // were identical, we assume this means newer but it might also mean
    // that someone changed versioning systems.
    if (i < v1.length) {
      return true;
    }

    return false;
  };

  var env = function() {
    if (typeof(process) !== 'undefined' &&
        typeof(process.versions) !== 'undefined' &&
        typeof(process.versions.node) === 'string') {

      version = process.versions.node;
      if (version.substr(1) === 'v') version = version.substr(1);

      // Special casing node to avoid gating window.navigator.
      // Version will be a string rather than a float.
      return {
        name: 'Node',
        version: version,
        userAgent: 'Node ' + version,
        iframeNeedsLoad: false,
        versionGreaterThan: versionGEThan
      };
    }

    var userAgent = window.navigator.userAgent.toLowerCase(),
        appName = window.navigator.appName,
        navigatorVendor,
        name = 'unknown';

    if (userAgent.indexOf('opera') > -1 || userAgent.indexOf('opr') > -1) {
      name = 'Opera';

      if (/opr\/([0-9]{1,}[\.0-9]{0,})/.exec(userAgent) !== null) {
        version = parseFloat( RegExp.$1 );
      }

    } else if (userAgent.indexOf('firefox') > -1)   {
      name = 'Firefox';

      if (/firefox\/([0-9]{1,}[\.0-9]{0,})/.exec(userAgent) !== null) {
        version = parseFloat( RegExp.$1 );
      }

    } else if (appName === 'Microsoft Internet Explorer') {
      // IE 10 and below
      name = 'IE';

      if (/msie ([0-9]{1,}[\.0-9]{0,})/.exec(userAgent) !== null) {
        version = parseFloat( RegExp.$1 );
      }

    } else if (appName === 'Netscape' && userAgent.indexOf('trident') > -1) {
      // IE 11+

      name = 'IE';

      if (/trident\/.*rv:([0-9]{1,}[\.0-9]{0,})/.exec(userAgent) !== null) {
        version = parseFloat( RegExp.$1 );
      }

    } else if (userAgent.indexOf('chrome') > -1) {
      name = 'Chrome';

      if (/chrome\/([0-9]{1,}[\.0-9]{0,})/.exec(userAgent) !== null) {
        version = parseFloat( RegExp.$1 );
      }

    } else if ((navigatorVendor = window.navigator.vendor) &&
      navigatorVendor.toLowerCase().indexOf('apple') > -1) {
      name = 'Safari';

      if (/version\/([0-9]{1,}[\.0-9]{0,})/.exec(userAgent) !== null) {
        version = parseFloat( RegExp.$1 );
      }
    }

    return {
      name: name,
      version: version,
      userAgent: window.navigator.userAgent,
      iframeNeedsLoad: userAgent.indexOf('webkit') < 0,
      versionGreaterThan: versionGEThan
    };
  }();


  OTHelpers.env = env;

  OTHelpers.browser = function() {
    return OTHelpers.env.name;
  };

  OTHelpers.browserVersion = function() {
    return OTHelpers.env;
  };

})();
// tb_require('../../environment.js')
// tb_require('./event.js')

var nodeEventing;

if($.env.name === 'Node') {
  (function() {
    var EventEmitter = require('events').EventEmitter,
        util = require('util');

    // container for the EventEmitter behaviour. This prevents tight coupling
    // caused by accidentally bleeding implementation details and API into whatever
    // objects nodeEventing is applied to.
    var NodeEventable = function NodeEventable () {
      EventEmitter.call(this);

      this.events = {};
    };
    util.inherits(NodeEventable, EventEmitter);


    nodeEventing = function nodeEventing (/* self */) {
      var api = new NodeEventable(),
          _on = api.on,
          _off = api.removeListener;


      api.addListeners = function (eventNames, handler, context, closure) {
        var listener = {handler: handler};
        if (context) listener.context = context;
        if (closure) listener.closure = closure;

        $.forEach(eventNames, function(name) {
          if (!api.events[name]) api.events[name] = [];
          api.events[name].push(listener);

          _on(name, handler);

          var addedListener = name + ':added';
          if (api.events[addedListener]) {
            api.emit(addedListener, api.events[name].length);
          }
        });
      };

      api.removeAllListenersNamed = function (eventNames) {
        var _eventNames = eventNames.split(' ');
        api.removeAllListeners(_eventNames);

        $.forEach(_eventNames, function(name) {
          if (api.events[name]) delete api.events[name];
        });
      };

      api.removeListeners = function (eventNames, handler, closure) {
        function filterHandlers(listener) {
          return !(listener.handler === handler && listener.closure === closure);
        }

        $.forEach(eventNames.split(' '), function(name) {
          if (api.events[name]) {
            _off(name, handler);
            api.events[name] = $.filter(api.events[name], filterHandlers);
            if (api.events[name].length === 0) delete api.events[name];

            var removedListener = name + ':removed';
            if (api.events[removedListener]) {
              api.emit(removedListener, api.events[name] ? api.events[name].length : 0);
            }
          }
        });
      };

      api.removeAllListeners = function () {
        api.events = {};
        api.removeAllListeners();
      };

      api.dispatchEvent = function(event, defaultAction) {
        this.emit(event.type, event);

        if (defaultAction) {
          defaultAction.call(null, event);
        }
      };

      api.trigger = $.bind(api.emit, api);


      return api;
    };
  })();
}

// tb_require('../../environment.js')
// tb_require('./event.js')

var browserEventing;

if($.env.name !== 'Node') {

  browserEventing = function browserEventing (self, syncronous) {
    var api = {
      events: {}
    };


    // Call the defaultAction, passing args
    function executeDefaultAction(defaultAction, args) {
      if (!defaultAction) return;

      defaultAction.apply(null, args.slice());
    }

    // Execute each handler in +listeners+ with +args+.
    //
    // Each handler will be executed async. On completion the defaultAction
    // handler will be executed with the args.
    //
    // @param [Array] listeners
    //    An array of functions to execute. Each will be passed args.
    //
    // @param [Array] args
    //    An array of arguments to execute each function in  +listeners+ with.
    //
    // @param [String] name
    //    The name of this event.
    //
    // @param [Function, Null, Undefined] defaultAction
    //    An optional function to execute after every other handler. This will execute even
    //    if +listeners+ is empty. +defaultAction+ will be passed args as a normal
    //    handler would.
    //
    // @return Undefined
    //
    function executeListenersAsyncronously(name, args, defaultAction) {
      var listeners = api.events[name];
      if (!listeners || listeners.length === 0) return;

      var listenerAcks = listeners.length;

      $.forEach(listeners, function(listener) { // , index
        function filterHandlers(_listener) {
          return _listener.handler === listener.handler;
        }

        // We run this asynchronously so that it doesn't interfere with execution if an
        // error happens
        $.callAsync(function() {
          try {
            // have to check if the listener has not been removed
            if (api.events[name] && $.some(api.events[name], filterHandlers)) {
              (listener.closure || listener.handler).apply(listener.context || null, args);
            }
          }
          finally {
            listenerAcks--;

            if (listenerAcks === 0) {
              executeDefaultAction(defaultAction, args);
            }
          }
        });
      });
    }


    // This is identical to executeListenersAsyncronously except that handlers will
    // be executed syncronously.
    //
    // On completion the defaultAction handler will be executed with the args.
    //
    // @param [Array] listeners
    //    An array of functions to execute. Each will be passed args.
    //
    // @param [Array] args
    //    An array of arguments to execute each function in  +listeners+ with.
    //
    // @param [String] name
    //    The name of this event.
    //
    // @param [Function, Null, Undefined] defaultAction
    //    An optional function to execute after every other handler. This will execute even
    //    if +listeners+ is empty. +defaultAction+ will be passed args as a normal
    //    handler would.
    //
    // @return Undefined
    //
    function executeListenersSyncronously(name, args) { // defaultAction is not used
      var listeners = api.events[name];
      if (!listeners || listeners.length === 0) return;

      $.forEach(listeners, function(listener) { // index
        (listener.closure || listener.handler).apply(listener.context || null, args);
      });
    }

    var executeListeners = syncronous === true ?
      executeListenersSyncronously : executeListenersAsyncronously;


    api.addListeners = function (eventNames, handler, context, closure) {
      var listener = {handler: handler};
      if (context) listener.context = context;
      if (closure) listener.closure = closure;

      $.forEach(eventNames, function(name) {
        if (!api.events[name]) api.events[name] = [];
        api.events[name].push(listener);

        var addedListener = name + ':added';
        if (api.events[addedListener]) {
          executeListeners(addedListener, [api.events[name].length]);
        }
      });
    };

    api.removeListeners = function(eventNames, handler, context) {
      function filterListeners(listener) {
        var isCorrectHandler = (
          listener.handler.originalHandler === handler ||
          listener.handler === handler
        );

        return !(isCorrectHandler && listener.context === context);
      }

      $.forEach(eventNames, function(name) {
        if (api.events[name]) {
          api.events[name] = $.filter(api.events[name], filterListeners);
          if (api.events[name].length === 0) delete api.events[name];

          var removedListener = name + ':removed';
          if (api.events[ removedListener]) {
            executeListeners(removedListener, [api.events[name] ? api.events[name].length : 0]);
          }
        }
      });
    };

    api.removeAllListenersNamed = function (eventNames) {
      $.forEach(eventNames, function(name) {
        if (api.events[name]) {
          delete api.events[name];
        }
      });
    };

    api.removeAllListeners = function () {
      api.events = {};
    };

    api.dispatchEvent = function(event, defaultAction) {
      if (!api.events[event.type] || api.events[event.type].length === 0) {
        executeDefaultAction(defaultAction, [event]);
        return;
      }

      executeListeners(event.type, [event], defaultAction);
    };

    api.trigger = function(eventName, args) {
      if (!api.events[eventName] || api.events[eventName].length === 0) {
        return;
      }

      executeListeners(eventName, args);
    };


    return api;
  };
}

/*jshint browser:false, smarttabs:true*/
/* global window, require */

// tb_require('../../helpers.js')
// tb_require('../environment.js')

if (window.OTHelpers.env.name === 'Node') {
  var request = require('request');

  OTHelpers.request = function(url, options, callback) {
    var completion = function(error, response, body) {
      var event = {response: response, body: body};

      // We need to detect things that Request considers a success,
      // but we consider to be failures.
      if (!error && response.statusCode >= 200 &&
                  (response.statusCode < 300 || response.statusCode === 304) ) {
        callback(null, event);
      } else {
        callback(error, event);
      }
    };

    if (options.method.toLowerCase() === 'get') {
      request.get(url, completion);
    }
    else {
      request.post(url, options.body, completion);
    }
  };

  OTHelpers.getJSON = function(url, options, callback) {
    var extendedHeaders = require('underscore').extend(
      {
        'Accept': 'application/json'
      },
      options.headers || {}
    );

    request.get({
      url: url,
      headers: extendedHeaders,
      json: true
    }, function(err, response) {
      callback(err, response && response.body);
    });
  };
}
/*jshint browser:true, smarttabs:true*/

// tb_require('../../helpers.js')
// tb_require('../environment.js')

function formatPostData(data) { //, contentType
  // If it's a string, we assume it's properly encoded
  if (typeof(data) === 'string') return data;

  var queryString = [];

  for (var key in data) {
    queryString.push(
      encodeURIComponent(key) + '=' + encodeURIComponent(data[key])
    );
  }

  return queryString.join('&').replace(/\+/g, '%20');
}

if (window.OTHelpers.env.name !== 'Node') {

  OTHelpers.xdomainRequest = function(url, options, callback) {
    /*global XDomainRequest*/
    var xdr = new XDomainRequest(),
        _options = options || {},
        _method = _options.method.toLowerCase();

    if(!_method) {
      callback(new Error('No HTTP method specified in options'));
      return;
    }

    _method = _method.toUpperCase();

    if(!(_method === 'GET' || _method === 'POST')) {
      callback(new Error('HTTP method can only be '));
      return;
    }

    function done(err, event) {
      xdr.onload = xdr.onerror = xdr.ontimeout = function() {};
      xdr = void 0;
      callback(err, event);
    }


    xdr.onload = function() {
      done(null, {
        target: {
          responseText: xdr.responseText,
          headers: {
            'content-type': xdr.contentType
          }
        }
      });
    };

    xdr.onerror = function() {
      done(new Error('XDomainRequest of ' + url + ' failed'));
    };

    xdr.ontimeout = function() {
      done(new Error('XDomainRequest of ' + url + ' timed out'));
    };

    xdr.open(_method, url);
    xdr.send(options.body && formatPostData(options.body));

  };

  OTHelpers.request = function(url, options, callback) {
    var request = new XMLHttpRequest(),
        _options = options || {},
        _method = _options.method;

    if(!_method) {
      callback(new Error('No HTTP method specified in options'));
      return;
    }

    if (options.overrideMimeType) {
      if (request.overrideMimeType) {
        request.overrideMimeType(options.overrideMimeType);
      }
      delete options.overrideMimeType;
    }

    // Setup callbacks to correctly respond to success and error callbacks. This includes
    // interpreting the responses HTTP status, which XmlHttpRequest seems to ignore
    // by default.
    if (callback) {
      OTHelpers.on(request, 'load', function(event) {
        var status = event.target.status;

        // We need to detect things that XMLHttpRequest considers a success,
        // but we consider to be failures.
        if ( status >= 200 && (status < 300 || status === 304) ) {
          callback(null, event);
        } else {
          callback(event);
        }
      });

      OTHelpers.on(request, 'error', callback);
    }

    request.open(options.method, url, true);

    if (!_options.headers) _options.headers = {};

    for (var name in _options.headers) {
      if (!Object.prototype.hasOwnProperty.call(_options.headers, name)) {
        continue;
      }
      request.setRequestHeader(name, _options.headers[name]);
    }

    request.send(options.body && formatPostData(options.body));
  };


  OTHelpers.getJSON = function(url, options, callback) {
    options = options || {};

    var done = function(error, event) {
      if(error) {
        callback(error, event && event.target && event.target.responseText);
      } else {
        var response;

        try {
          response = JSON.parse(event.target.responseText);
        } catch(e) {
          // Badly formed JSON
          callback(e, event && event.target && event.target.responseText);
          return;
        }

        callback(null, response, event);
      }
    };

    if(options.xdomainrequest) {
      OTHelpers.xdomainRequest(url, { method: 'GET' }, done);
    } else {
      var extendedHeaders = OTHelpers.extend({
        'Accept': 'application/json'
      }, options.headers || {});

      OTHelpers.get(url, OTHelpers.extend(options || {}, {
        headers: extendedHeaders
      }), done);
    }

  };

}
/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')
// tb_require('./environment.js')


// Log levels for OTLog.setLogLevel
var LOG_LEVEL_DEBUG = 5,
    LOG_LEVEL_LOG   = 4,
    LOG_LEVEL_INFO  = 3,
    LOG_LEVEL_WARN  = 2,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_NONE  = 0;


// There is a single global log level for every component that uses
// the logs.
var _logLevel = LOG_LEVEL_NONE;

var setLogLevel = function setLogLevel (level) {
  _logLevel = typeof(level) === 'number' ? level : 0;
  return _logLevel;
};


OTHelpers.useLogHelpers = function(on){

  // Log levels for OTLog.setLogLevel
  on.DEBUG    = LOG_LEVEL_DEBUG;
  on.LOG      = LOG_LEVEL_LOG;
  on.INFO     = LOG_LEVEL_INFO;
  on.WARN     = LOG_LEVEL_WARN;
  on.ERROR    = LOG_LEVEL_ERROR;
  on.NONE     = LOG_LEVEL_NONE;

  var _logs = [],
      _canApplyConsole = true;

  try {
    Function.prototype.bind.call(window.console.log, window.console);
  } catch (err) {
    _canApplyConsole = false;
  }

  // Some objects can't be logged in the console, mostly these are certain
  // types of native objects that are exposed to JS. This is only really a
  // problem with IE, hence only the IE version does anything.
  var makeLogArgumentsSafe = function(args) { return args; };

  if (OTHelpers.env.name === 'IE') {
    makeLogArgumentsSafe = function(args) {
      return [toDebugString(prototypeSlice.apply(args))];
    };
  }

  // Generates a logging method for a particular method and log level.
  //
  // Attempts to handle the following cases:
  // * the desired log method doesn't exist, call fallback (if available) instead
  // * the console functionality isn't available because the developer tools (in IE)
  // aren't open, call fallback (if available)
  // * attempt to deal with weird IE hosted logging methods as best we can.
  //
  function generateLoggingMethod(method, level, fallback) {
    return function() {
      if (on.shouldLog(level)) {
        var cons = window.console,
            args = makeLogArgumentsSafe(arguments);

        // In IE, window.console may not exist if the developer tools aren't open
        // This also means that cons and cons[method] can appear at any moment
        // hence why we retest this every time.
        if (cons && cons[method]) {
          // the desired console method isn't a real object, which means
          // that we can't use apply on it. We force it to be a real object
          // using Function.bind, assuming that's available.
          if (cons[method].apply || _canApplyConsole) {
            if (!cons[method].apply) {
              cons[method] = Function.prototype.bind.call(cons[method], cons);
            }

            cons[method].apply(cons, args);
          }
          else {
            // This isn't the same result as the above, but it's better
            // than nothing.
            cons[method](args);
          }
        }
        else if (fallback) {
          fallback.apply(on, args);

          // Skip appendToLogs, we delegate entirely to the fallback
          return;
        }

        appendToLogs(method, makeLogArgumentsSafe(arguments));
      }
    };
  }

  on.log = generateLoggingMethod('log', on.LOG);

  // Generate debug, info, warn, and error logging methods, these all fallback to on.log
  on.debug = generateLoggingMethod('debug', on.DEBUG, on.log);
  on.info = generateLoggingMethod('info', on.INFO, on.log);
  on.warn = generateLoggingMethod('warn', on.WARN, on.log);
  on.error = generateLoggingMethod('error', on.ERROR, on.log);


  on.setLogLevel = function(level) {
    on.debug('TB.setLogLevel(' + _logLevel + ')');
    return setLogLevel(level);
  };

  on.getLogs = function() {
    return _logs;
  };

  // Determine if the level is visible given the current logLevel.
  on.shouldLog = function(level) {
    return _logLevel >= level;
  };

  // Format the current time nicely for logging. Returns the current
  // local time.
  function formatDateStamp() {
    var now = new Date();
    return now.toLocaleTimeString() + now.getMilliseconds();
  }

  function toJson(object) {
    try {
      return JSON.stringify(object);
    } catch(e) {
      return object.toString();
    }
  }

  function toDebugString(object) {
    var components = [];

    if (typeof(object) === 'undefined') {
      // noop
    }
    else if (object === null) {
      components.push('NULL');
    }
    else if (OTHelpers.isArray(object)) {
      for (var i=0; i<object.length; ++i) {
        components.push(toJson(object[i]));
      }
    }
    else if (OTHelpers.isObject(object)) {
      for (var key in object) {
        var stringValue;

        if (!OTHelpers.isFunction(object[key])) {
          stringValue = toJson(object[key]);
        }
        else if (object.hasOwnProperty(key)) {
          stringValue = 'function ' + key + '()';
        }

        components.push(key + ': ' + stringValue);
      }
    }
    else if (OTHelpers.isFunction(object)) {
      try {
        components.push(object.toString());
      } catch(e) {
        components.push('function()');
      }
    }
    else  {
      components.push(object.toString());
    }

    return components.join(', ');
  }

  // Append +args+ to logs, along with the current log level and the a date stamp.
  function appendToLogs(level, args) {
    if (!args) return;

    var message = toDebugString(args);
    if (message.length <= 2) return;

    _logs.push(
      [level, formatDateStamp(), message]
    );
  }
};

OTHelpers.useLogHelpers(OTHelpers);
OTHelpers.setLogLevel(OTHelpers.ERROR);

/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')

// DOM helpers

// Helper function for adding event listeners to dom elements.
// WARNING: This doesn't preserve event types, your handler could
// be getting all kinds of different parameters depending on the browser.
// You also may have different scopes depending on the browser and bubbling
// and cancelable are not supported.
ElementCollection.prototype.on = function (eventName, handler) {
  return this.forEach(function(element) {
    if (element.addEventListener) {
      element.addEventListener(eventName, handler, false);
    } else if (element.attachEvent) {
      element.attachEvent('on' + eventName, handler);
    } else {
      var oldHandler = element['on'+eventName];
      element['on'+eventName] = function() {
        handler.apply(this, arguments);
        if (oldHandler) oldHandler.apply(this, arguments);
      };
    }
  });
};

// Helper function for removing event listeners from dom elements.
ElementCollection.prototype.off = function (eventName, handler) {
  return this.forEach(function(element) {
    if (element.removeEventListener) {
      element.removeEventListener (eventName, handler,false);
    }
    else if (element.detachEvent) {
      element.detachEvent('on' + eventName, handler);
    }
  });
};

ElementCollection.prototype.once = function (eventName, handler) {
  var removeAfterTrigger = $.bind(function() {
    this.off(eventName, removeAfterTrigger);
    handler.apply(null, arguments);
  }, this);

  return this.on(eventName, removeAfterTrigger);
};

// @remove
OTHelpers.on = function(element, eventName,  handler) {
  return $(element).on(eventName, handler);
};

// @remove
OTHelpers.off = function(element, eventName, handler) {
  return $(element).off(eventName, handler);
};

// @remove
OTHelpers.once = function (element, eventName, handler) {
  return $(element).once(eventName, handler);
};

/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')
// tb_require('./dom_events.js')

(function() {

  var _domReady = typeof(document) === 'undefined' ||
                    document.readyState === 'complete' ||
                   (document.readyState === 'interactive' && document.body),

      _loadCallbacks = [],
      _unloadCallbacks = [],
      _domUnloaded = false,

      onDomReady = function() {
        _domReady = true;

        if (typeof(document) !== 'undefined') {
          if ( document.addEventListener ) {
            document.removeEventListener('DOMContentLoaded', onDomReady, false);
            window.removeEventListener('load', onDomReady, false);
          } else {
            document.detachEvent('onreadystatechange', onDomReady);
            window.detachEvent('onload', onDomReady);
          }
        }

        // This is making an assumption about there being only one 'window'
        // that we care about.
        OTHelpers.on(window, 'unload', onDomUnload);

        OTHelpers.forEach(_loadCallbacks, function(listener) {
          listener[0].call(listener[1]);
        });

        _loadCallbacks = [];
      },

      onDomUnload = function() {
        _domUnloaded = true;

        OTHelpers.forEach(_unloadCallbacks, function(listener) {
          listener[0].call(listener[1]);
        });

        _unloadCallbacks = [];
      };


  OTHelpers.onDOMLoad = function(cb, context) {
    if (OTHelpers.isReady()) {
      cb.call(context);
      return;
    }

    _loadCallbacks.push([cb, context]);
  };

  OTHelpers.onDOMUnload = function(cb, context) {
    if (this.isDOMUnloaded()) {
      cb.call(context);
      return;
    }

    _unloadCallbacks.push([cb, context]);
  };

  OTHelpers.isReady = function() {
    return !_domUnloaded && _domReady;
  };

  OTHelpers.isDOMUnloaded = function() {
    return _domUnloaded;
  };

  if (_domReady) {
    onDomReady();
  } else if(typeof(document) !== 'undefined') {
    if (document.addEventListener) {
      document.addEventListener('DOMContentLoaded', onDomReady, false);

      // fallback
      window.addEventListener( 'load', onDomReady, false );

    } else if (document.attachEvent) {
      document.attachEvent('onreadystatechange', function() {
        if (document.readyState === 'complete') onDomReady();
      });

      // fallback
      window.attachEvent( 'onload', onDomReady );
    }
  }

})();

/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')

OTHelpers.setCookie = function(key, value) {
  try {
    localStorage.setItem(key, value);
  } catch (err) {
    // Store in browser cookie
    var date = new Date();
    date.setTime(date.getTime()+(365*24*60*60*1000));
    var expires = '; expires=' + date.toGMTString();
    document.cookie = key + '=' + value + expires + '; path=/';
  }
};

OTHelpers.getCookie = function(key) {
  var value;

  try {
    value = localStorage.getItem(key);
    return value;
  } catch (err) {
    // Check browser cookies
    var nameEQ = key + '=';
    var ca = document.cookie.split(';');
    for(var i=0;i < ca.length;i++) {
      var c = ca[i];
      while (c.charAt(0) === ' ') {
        c = c.substring(1,c.length);
      }
      if (c.indexOf(nameEQ) === 0) {
        value = c.substring(nameEQ.length,c.length);
      }
    }

    if (value) {
      return value;
    }
  }

  return null;
};

// tb_require('../helpers.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */


OTHelpers.Collection = function(idField) {
  var _models = [],
      _byId = {},
      _idField = idField || 'id';

  OTHelpers.eventing(this, true);

  var modelProperty = function(model, property) {
    if(OTHelpers.isFunction(model[property])) {
      return model[property]();
    } else {
      return model[property];
    }
  };

  var onModelUpdate = OTHelpers.bind(function onModelUpdate (event) {
        this.trigger('update', event);
        this.trigger('update:'+event.target.id, event);
      }, this),

      onModelDestroy = OTHelpers.bind(function onModelDestroyed (event) {
        this.remove(event.target, event.reason);
      }, this);


  this.reset = function() {
    // Stop listening on the models, they are no longer our problem
    OTHelpers.forEach(_models, function(model) {
      model.off('updated', onModelUpdate, this);
      model.off('destroyed', onModelDestroy, this);
    }, this);

    _models = [];
    _byId = {};
  };

  this.destroy = function(reason) {
    OTHelpers.forEach(_models, function(model) {
      if(model && typeof model.destroy === 'function') {
        model.destroy(reason, true);
      }
    });

    this.reset();
    this.off();
  };

  this.get = function(id) { return id && _byId[id] !== void 0 ? _models[_byId[id]] : void 0; };
  this.has = function(id) { return id && _byId[id] !== void 0; };

  this.toString = function() { return _models.toString(); };

  // Return only models filtered by either a dict of properties
  // or a filter function.
  //
  // @example Return all publishers with a streamId of 1
  //   OT.publishers.where({streamId: 1})
  //
  // @example The same thing but filtering using a filter function
  //   OT.publishers.where(function(publisher) {
  //     return publisher.stream.id === 4;
  //   });
  //
  // @example The same thing but filtering using a filter function
  //          executed with a specific this
  //   OT.publishers.where(function(publisher) {
  //     return publisher.stream.id === 4;
  //   }, self);
  //
  this.where = function(attrsOrFilterFn, context) {
    if (OTHelpers.isFunction(attrsOrFilterFn)) {
      return OTHelpers.filter(_models, attrsOrFilterFn, context);
    }

    return OTHelpers.filter(_models, function(model) {
      for (var key in attrsOrFilterFn) {
        if(!attrsOrFilterFn.hasOwnProperty(key)) {
          continue;
        }
        if (modelProperty(model, key) !== attrsOrFilterFn[key]) return false;
      }

      return true;
    });
  };

  // Similar to where in behaviour, except that it only returns
  // the first match.
  this.find = function(attrsOrFilterFn, context) {
    var filterFn;

    if (OTHelpers.isFunction(attrsOrFilterFn)) {
      filterFn = attrsOrFilterFn;
    }
    else {
      filterFn = function(model) {
        for (var key in attrsOrFilterFn) {
          if(!attrsOrFilterFn.hasOwnProperty(key)) {
            continue;
          }
          if (modelProperty(model, key) !== attrsOrFilterFn[key]) return false;
        }

        return true;
      };
    }

    filterFn = OTHelpers.bind(filterFn, context);

    for (var i=0; i<_models.length; ++i) {
      if (filterFn(_models[i]) === true) return _models[i];
    }

    return null;
  };

  this.forEach = function(fn, context) {
    OTHelpers.forEach(_models, fn, context);
    return this;
  };

  this.add = function(model) {
    var id = modelProperty(model, _idField);

    if (this.has(id)) {
      OTHelpers.warn('Model ' + id + ' is already in the collection', _models);
      return this;
    }

    _byId[id] = _models.push(model) - 1;

    model.on('updated', onModelUpdate, this);
    model.on('destroyed', onModelDestroy, this);

    this.trigger('add', model);
    this.trigger('add:'+id, model);

    return this;
  };

  this.remove = function(model, reason) {
    var id = modelProperty(model, _idField);

    _models.splice(_byId[id], 1);

    // Shuffle everyone down one
    for (var i=_byId[id]; i<_models.length; ++i) {
      _byId[_models[i][_idField]] = i;
    }

    delete _byId[id];

    model.off('updated', onModelUpdate, this);
    model.off('destroyed', onModelDestroy, this);

    this.trigger('remove', model, reason);
    this.trigger('remove:'+id, model, reason);

    return this;
  };

  // Retrigger the add event behaviour for each model. You can also
  // select a subset of models to trigger using the same arguments
  // as the #where method.
  this._triggerAddEvents = function() {
    var models = this.where.apply(this, arguments);
    OTHelpers.forEach(models, function(model) {
      this.trigger('add', model);
      this.trigger('add:' + modelProperty(model, _idField), model);
    }, this);
  };

  this.length = function() {
    return _models.length;
  };
};


/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')

OTHelpers.castToBoolean = function(value, defaultValue) {
  if (value === undefined) return defaultValue;
  return value === 'true' || value === true;
};

OTHelpers.roundFloat = function(value, places) {
  return Number(value.toFixed(places));
};

/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')

(function() {

  var capabilities = {};

  // Registers a new capability type and a function that will indicate
  // whether this client has that capability.
  //
  //   OTHelpers.registerCapability('bundle', function() {
  //     return OTHelpers.hasCapabilities('webrtc') &&
  //                (OTHelpers.env.name === 'Chrome' || TBPlugin.isInstalled());
  //   });
  //
  OTHelpers.registerCapability = function(name, callback) {
    var _name = name.toLowerCase();

    if (capabilities.hasOwnProperty(_name)) {
      OTHelpers.error('Attempted to register', name, 'capability more than once');
      return;
    }

    if (!OTHelpers.isFunction(callback)) {
      OTHelpers.error('Attempted to register', name,
                              'capability with a callback that isn\' a function');
      return;
    }

    memoriseCapabilityTest(_name, callback);
  };


  // Wrap up a capability test in a function that memorises the
  // result.
  var memoriseCapabilityTest = function (name, callback) {
    capabilities[name] = function() {
      var result = callback();
      capabilities[name] = function() {
        return result;
      };

      return result;
    };
  };

  var testCapability = function (name) {
    return capabilities[name]();
  };


  // Returns true if all of the capability names passed in
  // exist and are met.
  //
  //  OTHelpers.hasCapabilities('bundle', 'rtcpMux')
  //
  OTHelpers.hasCapabilities = function(/* capability1, capability2, ..., capabilityN  */) {
    var capNames = prototypeSlice.call(arguments),
        name;

    for (var i=0; i<capNames.length; ++i) {
      name = capNames[i].toLowerCase();

      if (!capabilities.hasOwnProperty(name)) {
        OTHelpers.error('hasCapabilities was called with an unknown capability: ' + name);
        return false;
      }
      else if (testCapability(name) === false) {
        return false;
      }
    }

    return true;
  };

})();

/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')
// tb_require('./capabilities.js')

// Indicates if the client supports WebSockets.
OTHelpers.registerCapability('websockets', function() {
  return 'WebSocket' in window && window.WebSocket !== void 0;
});
// tb_require('../helpers.js')

/**@licence
 * Copyright (c) 2010 Caolan McMahon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 **/


(function() {

  OTHelpers.setImmediate = (function() {
    if (typeof process === 'undefined' || !(process.nextTick)) {
      if (typeof setImmediate === 'function') {
        return function (fn) {
          // not a direct alias for IE10 compatibility
          setImmediate(fn);
        };
      }
      return function (fn) {
        setTimeout(fn, 0);
      };
    }
    if (typeof setImmediate !== 'undefined') {
      return setImmediate;
    }
    return process.nextTick;
  })();

  OTHelpers.iterator = function(tasks) {
    var makeCallback = function (index) {
      var fn = function () {
        if (tasks.length) {
          tasks[index].apply(null, arguments);
        }
        return fn.next();
      };
      fn.next = function () {
        return (index < tasks.length - 1) ? makeCallback(index + 1) : null;
      };
      return fn;
    };
    return makeCallback(0);
  };

  OTHelpers.waterfall = function(array, done) {
    done = done || function () {};
    if (array.constructor !== Array) {
      return done(new Error('First argument to waterfall must be an array of functions'));
    }

    if (!array.length) {
      return done();
    }

    var next = function(iterator) {
      return function (err) {
        if (err) {
          done.apply(null, arguments);
          done = function () {};
        } else {
          var args = prototypeSlice.call(arguments, 1),
              nextFn = iterator.next();
          if (nextFn) {
            args.push(next(nextFn));
          } else {
            args.push(done);
          }
          OTHelpers.setImmediate(function() {
            iterator.apply(null, args);
          });
        }
      };
    };

    next(OTHelpers.iterator(array))();
  };

})();

/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')

(function() {

  var requestAnimationFrame = window.requestAnimationFrame ||
                              window.mozRequestAnimationFrame ||
                              window.webkitRequestAnimationFrame ||
                              window.msRequestAnimationFrame;

  if (requestAnimationFrame) {
    requestAnimationFrame = OTHelpers.bind(requestAnimationFrame, window);
  }
  else {
    var lastTime = 0;
    var startTime = OTHelpers.now();

    requestAnimationFrame = function(callback){
      var currTime = OTHelpers.now();
      var timeToCall = Math.max(0, 16 - (currTime - lastTime));
      var id = window.setTimeout(function() { callback(currTime - startTime); }, timeToCall);
      lastTime = currTime + timeToCall;
      return id;
    };
  }

  OTHelpers.requestAnimationFrame = requestAnimationFrame;
})();
/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')

(function() {

  // Singleton interval
  var logQueue = [],
      queueRunning = false;

  OTHelpers.Analytics = function(loggingUrl, debugFn) {

    var endPoint = loggingUrl + '/logging/ClientEvent',
        endPointQos = loggingUrl + '/logging/ClientQos',

        reportedErrors = {},

        send = function(data, isQos, callback) {
          OTHelpers.post((isQos ? endPointQos : endPoint) + '?_=' + OTHelpers.uuid.v4(), {
            body: data,
            xdomainrequest: ($.env.name === 'IE' && $.env.version < 10),
            overrideMimeType: 'text/plain',
            headers: {
              'Accept': 'text/plain',
              'Content-Type': 'application/json'
            }
          }, callback);
        },

        throttledPost = function() {
          // Throttle logs so that they only happen 1 at a time
          if (!queueRunning && logQueue.length > 0) {
            queueRunning = true;
            var curr = logQueue[0];

            // Remove the current item and send the next log
            var processNextItem = function() {
              logQueue.shift();
              queueRunning = false;
              throttledPost();
            };

            if (curr) {
              send(curr.data, curr.isQos, function(err) {
                if (err) {
                  var debugMsg = 'Failed to send ClientEvent, moving on to the next item.';
                  if (debugFn) {
                    debugFn(debugMsg);
                  } else {
                    console.log(debugMsg);
                  }
                  if (curr.onComplete) {
                    curr.onComplete(err);
                  }
                  // There was an error, move onto the next item
                }
                if (curr.onComplete) {
                  curr.onComplete(err);
                }
                setTimeout(processNextItem, 50);
              });
            }
          }
        },

        post = function(data, onComplete, isQos) {
          logQueue.push({
            data: data,
            onComplete: onComplete,
            isQos: isQos
          });

          throttledPost();
        },

        shouldThrottleError = function(code, type, partnerId) {
          if (!partnerId) return false;

          var errKey = [partnerId, type, code].join('_'),
          //msgLimit = DynamicConfig.get('exceptionLogging', 'messageLimitPerPartner', partnerId);
            msgLimit = 100;
          if (msgLimit === null || msgLimit === undefined) return false;
          return (reportedErrors[errKey] || 0) <= msgLimit;
        };

    // Log an error via ClientEvents.
    //
    // @param [String] code
    // @param [String] type
    // @param [String] message
    // @param [Hash] details additional error details
    //
    // @param [Hash] options the options to log the client event with.
    // @option options [String] action The name of the Event that we are logging. E.g.
    //  'TokShowLoaded'. Required.
    // @option options [String] variation Usually used for Split A/B testing, when you
    //  have multiple variations of the +_action+.
    // @option options [String] payload The payload. Required.
    // @option options [String] sessionId The active OpenTok session, if there is one
    // @option options [String] connectionId The active OpenTok connectionId, if there is one
    // @option options [String] partnerId
    // @option options [String] guid ...
    // @option options [String] streamId ...
    // @option options [String] section ...
    // @option options [String] clientVersion ...
    //
    // Reports will be throttled to X reports (see exceptionLogging.messageLimitPerPartner
    // from the dynamic config for X) of each error type for each partner. Reports can be
    // disabled/enabled globally or on a per partner basis (per partner settings
    // take precedence) using exceptionLogging.enabled.
    //
    this.logError = function(code, type, message, details, options) {
      if (!options) options = {};
      var partnerId = options.partnerId;

      if (shouldThrottleError(code, type, partnerId)) {
        //OT.log('ClientEvents.error has throttled an error of type ' + type + '.' +
        // code + ' for partner ' + (partnerId || 'No Partner Id'));
        return;
      }

      var errKey = [partnerId, type, code].join('_'),
      payload =  details ? details : null;

      reportedErrors[errKey] = typeof(reportedErrors[errKey]) !== 'undefined' ?
        reportedErrors[errKey] + 1 : 1;
      this.logEvent(OTHelpers.extend(options, {
        action: type + '.' + code,
        payload: payload
      }), false);
    };

    // Log a client event to the analytics backend.
    //
    // @example Logs a client event called 'foo'
    //  this.logEvent({
    //      action: 'foo',
    //      payload: 'bar',
    //      sessionId: sessionId,
    //      connectionId: connectionId
    //  }, false)
    //
    // @param [Hash] data the data to log the client event with.
    // @param [Boolean] qos Whether this is a QoS event.
    // @param [Boolean] throttle A number specifying the ratio of events to be sent
    //        out of the total number of events (other events are not ignored). If not
    //        set to a number, all events are sent.
    // @param [Number] completionHandler A completion handler function to call when the
    //                 client event POST request succeeds or fails. If it fails, an error
    //                 object is passed into the function. (See throttledPost().)
    //
    this.logEvent = function(data, qos, throttle, completionHandler) {
      if (!qos) qos = false;

      if (throttle && !isNaN(throttle)) {
        if (Math.random() > throttle) {
          return;
        }
      }

      // remove properties that have null values:
      for (var key in data) {
        if (data.hasOwnProperty(key) && data[key] === null) {
          delete data[key];
        }
      }

      // TODO: catch error when stringifying an object that has a circular reference
      data = JSON.stringify(data);

      post(data, completionHandler, qos);
    };

    // Log a client QOS to the analytics backend.
    // Log a client QOS to the analytics backend.
    // @option options [String] action The name of the Event that we are logging.
    //  E.g. 'TokShowLoaded'. Required.
    // @option options [String] variation Usually used for Split A/B testing, when
    //  you have multiple variations of the +_action+.
    // @option options [String] payload The payload. Required.
    // @option options [String] sessionId The active OpenTok session, if there is one
    // @option options [String] connectionId The active OpenTok connectionId, if there is one
    // @option options [String] partnerId
    // @option options [String] guid ...
    // @option options [String] streamId ...
    // @option options [String] section ...
    // @option options [String] clientVersion ...
    //
    this.logQOS = function(options) {
      this.logEvent(options, true);
    };
  };

})();

// AJAX helpers

/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')
// tb_require('./ajax/node.js')
// tb_require('./ajax/browser.js')

OTHelpers.get = function(url, options, callback) {
  var _options = OTHelpers.extend(options || {}, {
    method: 'GET'
  });
  OTHelpers.request(url, _options, callback);
};


OTHelpers.post = function(url, options, callback) {
  var _options = OTHelpers.extend(options || {}, {
    method: 'POST'
  });

  if(_options.xdomainrequest) {
    OTHelpers.xdomainRequest(url, _options, callback);
  } else {
    OTHelpers.request(url, _options, callback);
  }
};

/*!
 *  This is a modified version of Robert Kieffer awesome uuid.js library.
 *  The only modifications we've made are to remove the Node.js specific
 *  parts of the code and the UUID version 1 generator (which we don't
 *  use). The original copyright notice is below.
 *
 *     node-uuid/uuid.js
 *
 *     Copyright (c) 2010 Robert Kieffer
 *     Dual licensed under the MIT and GPL licenses.
 *     Documentation and details at https://github.com/broofa/node-uuid
 */
// tb_require('../helpers.js')

/*global crypto:true, Uint32Array:true, Buffer:true */
/*jshint browser:true, smarttabs:true*/

(function() {

  // Unique ID creation requires a high quality random # generator, but
  // Math.random() does not guarantee "cryptographic quality".  So we feature
  // detect for more robust APIs, normalizing each method to return 128-bits
  // (16 bytes) of random data.
  var mathRNG, whatwgRNG;

  // Math.random()-based RNG.  All platforms, very fast, unknown quality
  var _rndBytes = new Array(16);
  mathRNG = function() {
    var r, b = _rndBytes, i = 0;

    for (i = 0; i < 16; i++) {
      if ((i & 0x03) === 0) r = Math.random() * 0x100000000;
      b[i] = r >>> ((i & 0x03) << 3) & 0xff;
    }

    return b;
  };

  // WHATWG crypto-based RNG - http://wiki.whatwg.org/wiki/Crypto
  // WebKit only (currently), moderately fast, high quality
  if (window.crypto && crypto.getRandomValues) {
    var _rnds = new Uint32Array(4);
    whatwgRNG = function() {
      crypto.getRandomValues(_rnds);

      for (var c = 0 ; c < 16; c++) {
        _rndBytes[c] = _rnds[c >> 2] >>> ((c & 0x03) * 8) & 0xff;
      }
      return _rndBytes;
    };
  }

  // Select RNG with best quality
  var _rng = whatwgRNG || mathRNG;

  // Buffer class to use
  var BufferClass = typeof(Buffer) === 'function' ? Buffer : Array;

  // Maps for number <-> hex string conversion
  var _byteToHex = [];
  var _hexToByte = {};
  for (var i = 0; i < 256; i++) {
    _byteToHex[i] = (i + 0x100).toString(16).substr(1);
    _hexToByte[_byteToHex[i]] = i;
  }

  // **`parse()` - Parse a UUID into it's component bytes**
  function parse(s, buf, offset) {
    var i = (buf && offset) || 0, ii = 0;

    buf = buf || [];
    s.toLowerCase().replace(/[0-9a-f]{2}/g, function(oct) {
      if (ii < 16) { // Don't overflow!
        buf[i + ii++] = _hexToByte[oct];
      }
    });

    // Zero out remaining bytes if string was short
    while (ii < 16) {
      buf[i + ii++] = 0;
    }

    return buf;
  }

  // **`unparse()` - Convert UUID byte array (ala parse()) into a string**
  function unparse(buf, offset) {
    var i = offset || 0, bth = _byteToHex;
    return  bth[buf[i++]] + bth[buf[i++]] +
            bth[buf[i++]] + bth[buf[i++]] + '-' +
            bth[buf[i++]] + bth[buf[i++]] + '-' +
            bth[buf[i++]] + bth[buf[i++]] + '-' +
            bth[buf[i++]] + bth[buf[i++]] + '-' +
            bth[buf[i++]] + bth[buf[i++]] +
            bth[buf[i++]] + bth[buf[i++]] +
            bth[buf[i++]] + bth[buf[i++]];
  }

  // **`v4()` - Generate random UUID**

  // See https://github.com/broofa/node-uuid for API details
  function v4(options, buf, offset) {
    // Deprecated - 'format' argument, as supported in v1.2
    var i = buf && offset || 0;

    if (typeof(options) === 'string') {
      buf = options === 'binary' ? new BufferClass(16) : null;
      options = null;
    }
    options = options || {};

    var rnds = options.random || (options.rng || _rng)();

    // Per 4.4, set bits for version and `clock_seq_hi_and_reserved`
    rnds[6] = (rnds[6] & 0x0f) | 0x40;
    rnds[8] = (rnds[8] & 0x3f) | 0x80;

    // Copy bytes to buffer, if provided
    if (buf) {
      for (var ii = 0; ii < 16; ii++) {
        buf[i + ii] = rnds[ii];
      }
    }

    return buf || unparse(rnds);
  }

  // Export public API
  var uuid = v4;
  uuid.v4 = v4;
  uuid.parse = parse;
  uuid.unparse = unparse;
  uuid.BufferClass = BufferClass;

  // Export RNG options
  uuid.mathRNG = mathRNG;
  uuid.whatwgRNG = whatwgRNG;

  OTHelpers.uuid = uuid;

}());
/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')
// tb_require('../vendor/uuid.js')
// tb_require('./dom_events.js')

(function() {

  var _callAsync;

  // Is true if window.postMessage is supported.
  // This is not quite as simple as just looking for
  // window.postMessage as some older versions of IE
  // have a broken implementation of it.
  //
  var supportsPostMessage = (function () {
    if (window.postMessage) {
      // Check to see if postMessage fires synchronously,
      // if it does, then the implementation of postMessage
      // is broken.
      var postMessageIsAsynchronous = true;
      var oldOnMessage = window.onmessage;
      window.onmessage = function() {
        postMessageIsAsynchronous = false;
      };
      window.postMessage('', '*');
      window.onmessage = oldOnMessage;
      return postMessageIsAsynchronous;
    }
  })();

  if (supportsPostMessage) {
    var timeouts = [],
        messageName = 'OTHelpers.' + OTHelpers.uuid.v4() + '.zero-timeout';

    var removeMessageHandler = function() {
      timeouts = [];

      if(window.removeEventListener) {
        window.removeEventListener('message', handleMessage);
      } else if(window.detachEvent) {
        window.detachEvent('onmessage', handleMessage);
      }
    };

    var handleMessage = function(event) {
      if (event.source === window &&
          event.data === messageName) {

        if(OTHelpers.isFunction(event.stopPropagation)) {
          event.stopPropagation();
        }
        event.cancelBubble = true;

        if (!window.___othelpers) {
          removeMessageHandler();
          return;
        }

        if (timeouts.length > 0) {
          var args = timeouts.shift(),
              fn = args.shift();

          fn.apply(null, args);
        }
      }
    };

    // Ensure that we don't receive messages after unload
    // Yes, this seems to really happen in IE sometimes, usually
    // when iFrames are involved.
    OTHelpers.on(window, 'unload', removeMessageHandler);

    if(window.addEventListener) {
      window.addEventListener('message', handleMessage, true);
    } else if(window.attachEvent) {
      window.attachEvent('onmessage', handleMessage);
    }

    _callAsync = function (/* fn, [arg1, arg2, ..., argN] */) {
      timeouts.push(prototypeSlice.call(arguments));
      window.postMessage(messageName, '*');
    };
  }
  else {
    _callAsync = function (/* fn, [arg1, arg2, ..., argN] */) {
      var args = prototypeSlice.call(arguments),
          fn = args.shift();

      setTimeout(function() {
        fn.apply(null, args);
      }, 0);
    };
  }


  // Calls the function +fn+ asynchronously with the current execution.
  // This is most commonly used to execute something straight after
  // the current function.
  //
  // Any arguments in addition to +fn+ will be passed to +fn+ when it's
  // called.
  //
  // You would use this inplace of setTimeout(fn, 0) type constructs. callAsync
  // is preferable as it executes in a much more predictable time window,
  // unlike setTimeout which could execute anywhere from 2ms to several thousand
  // depending on the browser/context.
  //
  // It does this using window.postMessage, although if postMessage won't
  // work it will fallback to setTimeout.
  //
  OTHelpers.callAsync = _callAsync;


  // Wraps +handler+ in a function that will execute it asynchronously
  // so that it doesn't interfere with it's exceution context if it raises
  // an exception.
  OTHelpers.createAsyncHandler = function(handler) {
    return function() {
      var args = prototypeSlice.call(arguments);

      OTHelpers.callAsync(function() {
        handler.apply(null, args);
      });
    };
  };

})();

/*jshint browser:true, smarttabs:true */

// tb_require('../helpers.js')
// tb_require('./callbacks.js')
// tb_require('./dom_events.js')

OTHelpers.createElement = function(nodeName, attributes, children, doc) {
  var element = (doc || document).createElement(nodeName);

  if (attributes) {
    for (var name in attributes) {
      if (typeof(attributes[name]) === 'object') {
        if (!element[name]) element[name] = {};

        var subAttrs = attributes[name];
        for (var n in subAttrs) {
          element[name][n] = subAttrs[n];
        }
      }
      else if (name === 'className') {
        element.className = attributes[name];
      }
      else {
        element.setAttribute(name, attributes[name]);
      }
    }
  }

  var setChildren = function(child) {
    if(typeof child === 'string') {
      element.innerHTML = element.innerHTML + child;
    } else {
      element.appendChild(child);
    }
  };

  if($.isArray(children)) {
    $.forEach(children, setChildren);
  } else if(children) {
    setChildren(children);
  }

  return element;
};

OTHelpers.createButton = function(innerHTML, attributes, events) {
  var button = $.createElement('button', attributes, innerHTML);

  if (events) {
    for (var name in events) {
      if (events.hasOwnProperty(name)) {
        $.on(button, name, events[name]);
      }
    }

    button._boundEvents = events;
  }

  return button;
};
/*jshint browser:true, smarttabs:true */

// tb_require('../helpers.js')
// tb_require('./callbacks.js')

// DOM helpers

var firstElementChild;

// This mess is for IE8
if( typeof(document) !== 'undefined' &&
      document.createElement('div').firstElementChild !== void 0 ){
  firstElementChild = function firstElementChild (parentElement) {
    return parentElement.firstElementChild;
  };
}
else {
  firstElementChild = function firstElementChild (parentElement) {
    var el = parentElement.firstChild;

    do {
      if(el.nodeType===1){
        return el;
      }
      el = el.nextSibling;
    } while(el);

    return null;
  };
}


ElementCollection.prototype.appendTo = function(parentElement) {
  if (!parentElement) throw new Error('appendTo requires a DOMElement to append to.');

  return this.forEach(function(child) {
    parentElement.appendChild(child);
  });
};

ElementCollection.prototype.append = function() {
  var parentElement = this.first;
  if (!parentElement) return this;

  $.forEach(prototypeSlice.call(arguments), function(child) {
    parentElement.appendChild(child);
  });

  return this;
};

ElementCollection.prototype.prepend = function() {
  if (arguments.length === 0) return this;

  var parentElement = this.first,
      elementsToPrepend;

  if (!parentElement) return this;

  elementsToPrepend = prototypeSlice.call(arguments);

  if (!firstElementChild(parentElement)) {
    parentElement.appendChild(elementsToPrepend.shift());
  }

  $.forEach(elementsToPrepend, function(element) {
    parentElement.insertBefore(element, firstElementChild(parentElement));
  });

  return this;
};

ElementCollection.prototype.after = function(prevElement) {
  if (!prevElement) throw new Error('after requires a DOMElement to insert after');

  return this.forEach(function(element) {
    if (element.parentElement) {
      if (prevElement !== element.parentNode.lastChild) {
        element.parentElement.insertBefore(element, prevElement);
      }
      else {
        element.parentElement.appendChild(element);
      }
    }
  });
};

ElementCollection.prototype.before = function(nextElement) {
  if (!nextElement) {
    throw new Error('before requires a DOMElement to insert before');
  }

  return this.forEach(function(element) {
    if (element.parentElement) {
      element.parentElement.insertBefore(element, nextElement);
    }
  });
};

ElementCollection.prototype.remove = function () {
  return this.forEach(function(element) {
    if (element.parentNode) {
      element.parentNode.removeChild(element);
    }
  });
};

ElementCollection.prototype.empty = function () {
  return this.forEach(function(element) {
    // elements is a "live" NodesList collection. Meaning that the collection
    // itself will be mutated as we remove elements from the DOM. This means
    // that "while there are still elements" is safer than "iterate over each
    // element" as the collection length and the elements indices will be modified
    // with each iteration.
    while (element.firstChild) {
      element.removeChild(element.firstChild);
    }
  });
};


// Detects when an element is not part of the document flow because
// it or one of it's ancesters has display:none.
ElementCollection.prototype.isDisplayNone = function() {
  return this.some(function(element) {
    if ( (element.offsetWidth === 0 || element.offsetHeight === 0) &&
                $(element).css('display') === 'none') return true;

    if (element.parentNode && element.parentNode.style) {
      return $(element.parentNode).isDisplayNone();
    }
  });
};

ElementCollection.prototype.findElementWithDisplayNone = function(element) {
  return $.findElementWithDisplayNone(element);
};



OTHelpers.isElementNode = function(node) {
  return node && typeof node === 'object' && node.nodeType === 1;
};


// @remove
OTHelpers.removeElement = function(element) {
  $(element).remove();
};

// @remove
OTHelpers.removeElementById = function(elementId) {
  return $('#'+elementId).remove();
};

// @remove
OTHelpers.removeElementsByType = function(parentElem, type) {
  return $(type, parentElem).remove();
};

// @remove
OTHelpers.emptyElement = function(element) {
  return $(element).empty();
};





// @remove
OTHelpers.isDisplayNone = function(element) {
  return $(element).isDisplayNone();
};

OTHelpers.findElementWithDisplayNone = function(element) {
  if ( (element.offsetWidth === 0 || element.offsetHeight === 0) &&
            $.css(element, 'display') === 'none') return element;

  if (element.parentNode && element.parentNode.style) {
    return $.findElementWithDisplayNone(element.parentNode);
  }

  return null;
};


/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')
// tb_require('./environment.js')
// tb_require('./dom.js')

OTHelpers.Modal = function(options) {

  OTHelpers.eventing(this, true);

  var callback = arguments[arguments.length - 1];

  if(!OTHelpers.isFunction(callback)) {
    throw new Error('OTHelpers.Modal2 must be given a callback');
  }

  if(arguments.length < 2) {
    options = {};
  }

  var domElement = document.createElement('iframe');

  domElement.id = options.id || OTHelpers.uuid();
  domElement.style.position = 'absolute';
  domElement.style.position = 'fixed';
  domElement.style.height = '100%';
  domElement.style.width = '100%';
  domElement.style.top = '0px';
  domElement.style.left = '0px';
  domElement.style.right = '0px';
  domElement.style.bottom = '0px';
  domElement.style.zIndex = 1000;
  domElement.style.border = '0';

  try {
    domElement.style.backgroundColor = 'rgba(0,0,0,0.2)';
  } catch (err) {
    // Old IE browsers don't support rgba and we still want to show the upgrade message
    // but we just make the background of the iframe completely transparent.
    domElement.style.backgroundColor = 'transparent';
    domElement.setAttribute('allowTransparency', 'true');
  }

  domElement.scrolling = 'no';
  domElement.setAttribute('scrolling', 'no');

  // This is necessary for IE, as it will not inherit it's doctype from
  // the parent frame.
  var frameContent = '<!DOCTYPE html><html><head>' +
                    '<meta http-equiv="x-ua-compatible" content="IE=Edge">' +
                    '<meta http-equiv="Content-type" content="text/html; charset=utf-8">' +
                    '<title></title></head><body></body></html>';

  var wrappedCallback = function() {
    var doc = domElement.contentDocument || domElement.contentWindow.document;

    if (OTHelpers.env.iframeNeedsLoad) {
      doc.body.style.backgroundColor = 'transparent';
      doc.body.style.border = 'none';

      if (OTHelpers.env.name !== 'IE') {
        // Skip this for IE as we use the bookmarklet workaround
        // for THAT browser.
        doc.open();
        doc.write(frameContent);
        doc.close();
      }
    }

    callback(
      domElement.contentWindow,
      doc
    );
  };

  document.body.appendChild(domElement);

  if(OTHelpers.env.iframeNeedsLoad) {
    if (OTHelpers.env.name === 'IE') {
      // This works around some issues with IE and document.write.
      // Basically this works by slightly abusing the bookmarklet/scriptlet
      // functionality that all browsers support.
      domElement.contentWindow.contents = frameContent;
      /*jshint scripturl:true*/
      domElement.src = 'javascript:window["contents"]';
      /*jshint scripturl:false*/
    }

    OTHelpers.on(domElement, 'load', wrappedCallback);
  } else {
    setTimeout(wrappedCallback, 0);
  }

  this.close = function() {
    OTHelpers.removeElement(domElement);
    this.trigger('closed');
    this.element = domElement = null;
    return this;
  };

  this.element = domElement;

};

/*
 * getComputedStyle from
 * https://github.com/jonathantneal/Polyfills-for-IE8/blob/master/getComputedStyle.js

// tb_require('../helpers.js')
// tb_require('./dom.js')

/*jshint strict: false, eqnull: true, browser:true, smarttabs:true*/

(function() {

  /*jshint eqnull: true, browser: true */


  function getPixelSize(element, style, property, fontSize) {
    var sizeWithSuffix = style[property],
        size = parseFloat(sizeWithSuffix),
        suffix = sizeWithSuffix.split(/\d/)[0],
        rootSize;

    fontSize = fontSize != null ?
      fontSize : /%|em/.test(suffix) && element.parentElement ?
        getPixelSize(element.parentElement, element.parentElement.currentStyle, 'fontSize', null) :
        16;
    rootSize = property === 'fontSize' ?
      fontSize : /width/i.test(property) ? element.clientWidth : element.clientHeight;

    return (suffix === 'em') ?
      size * fontSize : (suffix === 'in') ?
        size * 96 : (suffix === 'pt') ?
          size * 96 / 72 : (suffix === '%') ?
            size / 100 * rootSize : size;
  }

  function setShortStyleProperty(style, property) {
    var
    borderSuffix = property === 'border' ? 'Width' : '',
    t = property + 'Top' + borderSuffix,
    r = property + 'Right' + borderSuffix,
    b = property + 'Bottom' + borderSuffix,
    l = property + 'Left' + borderSuffix;

    style[property] = (style[t] === style[r] === style[b] === style[l] ? [style[t]]
    : style[t] === style[b] && style[l] === style[r] ? [style[t], style[r]]
    : style[l] === style[r] ? [style[t], style[r], style[b]]
    : [style[t], style[r], style[b], style[l]]).join(' ');
  }

  function CSSStyleDeclaration(element) {
    var currentStyle = element.currentStyle,
        style = this,
        fontSize = getPixelSize(element, currentStyle, 'fontSize', null),
        property;

    for (property in currentStyle) {
      if (/width|height|margin.|padding.|border.+W/.test(property) && style[property] !== 'auto') {
        style[property] = getPixelSize(element, currentStyle, property, fontSize) + 'px';
      } else if (property === 'styleFloat') {
        /*jshint -W069 */
        style['float'] = currentStyle[property];
      } else {
        style[property] = currentStyle[property];
      }
    }

    setShortStyleProperty(style, 'margin');
    setShortStyleProperty(style, 'padding');
    setShortStyleProperty(style, 'border');

    style.fontSize = fontSize + 'px';

    return style;
  }

  CSSStyleDeclaration.prototype = {
    constructor: CSSStyleDeclaration,
    getPropertyPriority: function () {},
    getPropertyValue: function ( prop ) {
      return this[prop] || '';
    },
    item: function () {},
    removeProperty: function () {},
    setProperty: function () {},
    getPropertyCSSValue: function () {}
  };

  function getComputedStyle(element) {
    return new CSSStyleDeclaration(element);
  }


  OTHelpers.getComputedStyle = function(element) {
    if(element &&
        element.ownerDocument &&
        element.ownerDocument.defaultView &&
        element.ownerDocument.defaultView.getComputedStyle) {
      return element.ownerDocument.defaultView.getComputedStyle(element);
    } else {
      return getComputedStyle(element);
    }
  };

})();

/*jshint browser:true, smarttabs:true */

// tb_require('../helpers.js')
// tb_require('./callbacks.js')
// tb_require('./dom.js')

var observeStyleChanges = function observeStyleChanges (element, stylesToObserve, onChange) {
  var oldStyles = {};

  var getStyle = function getStyle(style) {
    switch (style) {
    case 'width':
      return $(element).width();

    case 'height':
      return $(element).height();

    default:
      return $(element).css(style);
    }
  };

  // get the inital values
  $.forEach(stylesToObserve, function(style) {
    oldStyles[style] = getStyle(style);
  });

  var observer = new MutationObserver(function(mutations) {
    var changeSet = {};

    $.forEach(mutations, function(mutation) {
      if (mutation.attributeName !== 'style') return;

      var isHidden = $.isDisplayNone(element);

      $.forEach(stylesToObserve, function(style) {
        if(isHidden && (style === 'width' || style === 'height')) return;

        var newValue = getStyle(style);

        if (newValue !== oldStyles[style]) {
          changeSet[style] = [oldStyles[style], newValue];
          oldStyles[style] = newValue;
        }
      });
    });

    if (!$.isEmpty(changeSet)) {
      // Do this after so as to help avoid infinite loops of mutations.
      $.callAsync(function() {
        onChange.call(null, changeSet);
      });
    }
  });

  observer.observe(element, {
    attributes:true,
    attributeFilter: ['style'],
    childList:false,
    characterData:false,
    subtree:false
  });

  return observer;
};

var observeNodeOrChildNodeRemoval = function observeNodeOrChildNodeRemoval (element, onChange) {
  var observer = new MutationObserver(function(mutations) {
    var removedNodes = [];

    $.forEach(mutations, function(mutation) {
      if (mutation.removedNodes.length) {
        removedNodes = removedNodes.concat(prototypeSlice.call(mutation.removedNodes));
      }
    });

    if (removedNodes.length) {
      // Do this after so as to help avoid infinite loops of mutations.
      $.callAsync(function() {
        onChange($(removedNodes));
      });
    }
  });

  observer.observe(element, {
    attributes:false,
    childList:true,
    characterData:false,
    subtree:true
  });

  return observer;
};

var observeSize = function (element, onChange) {
  var previousSize = {
    width: 0,
    height: 0
  };

  var interval = setInterval(function() {
    var rect = element.getBoundingClientRect();
    if (previousSize.width !== rect.width || previousSize.height !== rect.height) {
      onChange(rect, previousSize);
      previousSize = {
        width: rect.width,
        height: rect.height
      };
    }
  }, 1000 / 5);

  return {
    disconnect: function() {
      clearInterval(interval);
    }
  };
};

// Allows an +onChange+ callback to be triggered when specific style properties
// of +element+ are notified. The callback accepts a single parameter, which is
// a hash where the keys are the style property that changed and the values are
// an array containing the old and new values ([oldValue, newValue]).
//
// Width and Height changes while the element is display: none will not be
// fired until such time as the element becomes visible again.
//
// This function returns the MutationObserver itself. Once you no longer wish
// to observe the element you should call disconnect on the observer.
//
// Observing changes:
//  // observe changings to the width and height of object
//  dimensionsObserver = OTHelpers(object).observeStyleChanges(,
//                                                    ['width', 'height'], function(changeSet) {
//      OT.debug("The new width and height are " +
//                      changeSet.width[1] + ',' + changeSet.height[1]);
//  });
//
// Cleaning up
//  // stop observing changes
//  dimensionsObserver.disconnect();
//  dimensionsObserver = null;
//
ElementCollection.prototype.observeStyleChanges = function(stylesToObserve, onChange) {
  var observers = [];

  this.forEach(function(element) {
    observers.push(
      observeStyleChanges(element, stylesToObserve, onChange)
    );
  });

  return observers;
};

// trigger the +onChange+ callback whenever
// 1. +element+ is removed
// 2. or an immediate child of +element+ is removed.
//
// This function returns the MutationObserver itself. Once you no longer wish
// to observe the element you should call disconnect on the observer.
//
// Observing changes:
//  // observe changings to the width and height of object
//  nodeObserver = OTHelpers(object).observeNodeOrChildNodeRemoval(function(removedNodes) {
//      OT.debug("Some child nodes were removed");
//      removedNodes.forEach(function(node) {
//          OT.debug(node);
//      });
//  });
//
// Cleaning up
//  // stop observing changes
//  nodeObserver.disconnect();
//  nodeObserver = null;
//
ElementCollection.prototype.observeNodeOrChildNodeRemoval = function(onChange) {
  var observers = [];

  this.forEach(function(element) {
    observers.push(
      observeNodeOrChildNodeRemoval(element, onChange)
    );
  });

  return observers;
};

// trigger the +onChange+ callback whenever the width or the height of the element changes
//
// Once you no longer wish to observe the element you should call disconnect on the observer.
//
// Observing changes:
//  // observe changings to the width and height of object
//  sizeObserver = OTHelpers(object).observeSize(function(newSize, previousSize) {
//      OT.debug("The new width and height are " +
//                      newSize.width + ',' + newSize.height);
//  });
//
// Cleaning up
//  // stop observing changes
//  sizeObserver.disconnect();
//  sizeObserver = null;
//
ElementCollection.prototype.observeSize = function(onChange) {
  var observers = [];

  this.forEach(function(element) {
    observers.push(
      observeSize(element, onChange)
    );
  });

  return observers;
};


// @remove
OTHelpers.observeStyleChanges = function(element, stylesToObserve, onChange) {
  return $(element).observeStyleChanges(stylesToObserve, onChange)[0];
};

// @remove
OTHelpers.observeNodeOrChildNodeRemoval = function(element, onChange) {
  return $(element).observeNodeOrChildNodeRemoval(onChange)[0];
};

/*jshint browser:true, smarttabs:true */

// tb_require('../helpers.js')
// tb_require('./dom.js')
// tb_require('./capabilities.js')

// Returns true if the client supports element.classList
OTHelpers.registerCapability('classList', function() {
  return (typeof document !== 'undefined') && ('classList' in document.createElement('a'));
});


function hasClass (element, className) {
  if (!className) return false;

  if ($.hasCapabilities('classList')) {
    return element.classList.contains(className);
  }

  return element.className.indexOf(className) > -1;
}

function toggleClasses (element, classNames) {
  if (!classNames || classNames.length === 0) return;

  // Only bother targeting Element nodes, ignore Text Nodes, CDATA, etc
  if (element.nodeType !== 1) {
    return;
  }

  var numClasses = classNames.length,
      i = 0;

  if ($.hasCapabilities('classList')) {
    for (; i<numClasses; ++i) {
      element.classList.toggle(classNames[i]);
    }

    return;
  }

  var className = (' ' + element.className + ' ').replace(/[\s+]/, ' ');


  for (; i<numClasses; ++i) {
    if (hasClass(element, classNames[i])) {
      className = className.replace(' ' + classNames[i] + ' ', ' ');
    }
    else {
      className += classNames[i] + ' ';
    }
  }

  element.className = $.trim(className);
}

function addClass (element, classNames) {
  if (!classNames || classNames.length === 0) return;

  // Only bother targeting Element nodes, ignore Text Nodes, CDATA, etc
  if (element.nodeType !== 1) {
    return;
  }

  var numClasses = classNames.length,
      i = 0;

  if ($.hasCapabilities('classList')) {
    for (; i<numClasses; ++i) {
      element.classList.add(classNames[i]);
    }

    return;
  }

  // Here's our fallback to browsers that don't support element.classList

  if (!element.className && classNames.length === 1) {
    element.className = classNames.join(' ');
  }
  else {
    var setClass = ' ' + element.className + ' ';

    for (; i<numClasses; ++i) {
      if ( !~setClass.indexOf( ' ' + classNames[i] + ' ')) {
        setClass += classNames[i] + ' ';
      }
    }

    element.className = $.trim(setClass);
  }
}

function removeClass (element, classNames) {
  if (!classNames || classNames.length === 0) return;

  // Only bother targeting Element nodes, ignore Text Nodes, CDATA, etc
  if (element.nodeType !== 1) {
    return;
  }

  var numClasses = classNames.length,
      i = 0;

  if ($.hasCapabilities('classList')) {
    for (; i<numClasses; ++i) {
      element.classList.remove(classNames[i]);
    }

    return;
  }

  var className = (' ' + element.className + ' ').replace(/[\s+]/, ' ');

  for (; i<numClasses; ++i) {
    className = className.replace(' ' + classNames[i] + ' ', ' ');
  }

  element.className = $.trim(className);
}

ElementCollection.prototype.addClass = function (value) {
  if (value) {
    var classNames = $.trim(value).split(/\s+/);

    this.forEach(function(element) {
      addClass(element, classNames);
    });
  }

  return this;
};

ElementCollection.prototype.removeClass = function (value) {
  if (value) {
    var classNames = $.trim(value).split(/\s+/);

    this.forEach(function(element) {
      removeClass(element, classNames);
    });
  }

  return this;
};

ElementCollection.prototype.toggleClass = function (value) {
  if (value) {
    var classNames = $.trim(value).split(/\s+/);

    this.forEach(function(element) {
      toggleClasses(element, classNames);
    });
  }

  return this;
};

ElementCollection.prototype.hasClass = function (value) {
  return this.some(function(element) {
    return hasClass(element, value);
  });
};


// @remove
OTHelpers.addClass = function(element, className) {
  return $(element).addClass(className);
};

// @remove
OTHelpers.removeClass = function(element, value) {
  return $(element).removeClass(value);
};


/*jshint browser:true, smarttabs:true */

// tb_require('../helpers.js')
// tb_require('./dom.js')
// tb_require('./capabilities.js')

var specialDomProperties = {
  'for': 'htmlFor',
  'class': 'className'
};


// Gets or sets the attribute called +name+ for the first element in the collection
ElementCollection.prototype.attr = function (name, value) {
  if (OTHelpers.isObject(name)) {
    var actualName;

    for (var key in name) {
      actualName = specialDomProperties[key] || key;
      this.first.setAttribute(actualName, name[key]);
    }
  }
  else if (value === void 0) {
    return this.first.getAttribute(specialDomProperties[name] || name);
  }
  else {
    this.first.setAttribute(specialDomProperties[name] || name, value);
  }

  return this;
};


// Removes an attribute called +name+ for the every element in the collection.
ElementCollection.prototype.removeAttr = function (name) {
  var actualName = specialDomProperties[name] || name;

  this.forEach(function(element) {
    element.removeAttribute(actualName);
  });

  return this;
};


// Gets, and optionally sets, the html body of the first element
// in the collection. If the +html+ is provided then the first
// element's html body will be replaced with it.
//
ElementCollection.prototype.html = function (html) {
  if (html !== void 0) {
    this.first.innerHTML = html;
  }

  return this.first.innerHTML;
};


// Centers +element+ within the window. You can pass through the width and height
// if you know it, if you don't they will be calculated for you.
ElementCollection.prototype.center = function (width, height) {
  var $element;

  this.forEach(function(element) {
    $element = $(element);
    if (!width) width = parseInt($element.width(), 10);
    if (!height) height = parseInt($element.height(), 10);

    var marginLeft = -0.5 * width + 'px';
    var marginTop = -0.5 * height + 'px';

    $element.css('margin', marginTop + ' 0 0 ' + marginLeft)
            .addClass('OT_centered');
  });

  return this;
};


// @remove
// Centers +element+ within the window. You can pass through the width and height
// if you know it, if you don't they will be calculated for you.
OTHelpers.centerElement = function(element, width, height) {
  return $(element).center(width, height);
};

  /**
   * Methods to calculate element widths and heights.
   */
(function() {

  var _width = function(element) {
        if (element.offsetWidth > 0) {
          return element.offsetWidth + 'px';
        }

        return $(element).css('width');
      },

      _height = function(element) {
        if (element.offsetHeight > 0) {
          return element.offsetHeight + 'px';
        }

        return $(element).css('height');
      };

  ElementCollection.prototype.width = function (newWidth) {
    if (newWidth) {
      this.css('width', newWidth);
      return this;
    }
    else {
      if (this.isDisplayNone()) {
        return this.makeVisibleAndYield(function(element) {
          return _width(element);
        })[0];
      }
      else {
        return _width(this.get(0));
      }
    }
  };

  ElementCollection.prototype.height = function (newHeight) {
    if (newHeight) {
      this.css('height', newHeight);
      return this;
    }
    else {
      if (this.isDisplayNone()) {
        // We can't get the height, probably since the element is hidden.
        return this.makeVisibleAndYield(function(element) {
          return _height(element);
        })[0];
      }
      else {
        return _height(this.get(0));
      }
    }
  };

  // @remove
  OTHelpers.width = function(element, newWidth) {
    var ret = $(element).width(newWidth);
    return newWidth ? OTHelpers : ret;
  };

  // @remove
  OTHelpers.height = function(element, newHeight) {
    var ret = $(element).height(newHeight);
    return newHeight ? OTHelpers : ret;
  };

})();


// CSS helpers helpers

/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')
// tb_require('./dom.js')
// tb_require('./getcomputedstyle.js')

(function() {

  var displayStateCache = {},
      defaultDisplays = {};

  var defaultDisplayValueForElement = function (element) {
    if (defaultDisplays[element.ownerDocument] &&
      defaultDisplays[element.ownerDocument][element.nodeName]) {
      return defaultDisplays[element.ownerDocument][element.nodeName];
    }

    if (!defaultDisplays[element.ownerDocument]) defaultDisplays[element.ownerDocument] = {};

    // We need to know what display value to use for this node. The easiest way
    // is to actually create a node and read it out.
    var testNode = element.ownerDocument.createElement(element.nodeName),
        defaultDisplay;

    element.ownerDocument.body.appendChild(testNode);
    defaultDisplay = defaultDisplays[element.ownerDocument][element.nodeName] =
    $(testNode).css('display');

    $(testNode).remove();
    testNode = null;

    return defaultDisplay;
  };

  var isHidden = function (element) {
    var computedStyle = $.getComputedStyle(element);
    return computedStyle.getPropertyValue('display') === 'none';
  };

  var setCssProperties = function (element, hash) {
    var style = element.style;

    for (var cssName in hash) {
      if (hash.hasOwnProperty(cssName)) {
        style[cssName] = hash[cssName];
      }
    }
  };

  var setCssProperty = function (element, name, value) {
    element.style[name] = value;
  };

  var getCssProperty = function (element, unnormalisedName) {
    // Normalise vendor prefixes from the form MozTranform to -moz-transform
    // except for ms extensions, which are weird...

    var name = unnormalisedName.replace( /([A-Z]|^ms)/g, '-$1' ).toLowerCase(),
        computedStyle = $.getComputedStyle(element),
        currentValue = computedStyle.getPropertyValue(name);

    if (currentValue === '') {
      currentValue = element.style[name];
    }

    return currentValue;
  };

  var applyCSS = function(element, styles, callback) {
    var oldStyles = {},
        name,
        ret;

    // Backup the old styles
    for (name in styles) {
      if (styles.hasOwnProperty(name)) {
        // We intentionally read out of style here, instead of using the css
        // helper. This is because the css helper uses querySelector and we
        // only want to pull values out of the style (domeElement.style) hash.
        oldStyles[name] = element.style[name];

        $(element).css(name, styles[name]);
      }
    }

    ret = callback(element);

    // Restore the old styles
    for (name in styles) {
      if (styles.hasOwnProperty(name)) {
        $(element).css(name, oldStyles[name] || '');
      }
    }

    return ret;
  };

  ElementCollection.prototype.show = function() {
    return this.forEach(function(element) {
      var display = element.style.display;

      if (display === '' || display === 'none') {
        element.style.display = displayStateCache[element] || '';
        delete displayStateCache[element];
      }

      if (isHidden(element)) {
        // It's still hidden so there's probably a stylesheet that declares this
        // element as display:none;
        displayStateCache[element] = 'none';

        element.style.display = defaultDisplayValueForElement(element);
      }
    });
  };

  ElementCollection.prototype.hide = function() {
    return this.forEach(function(element) {
      if (element.style.display === 'none') return;

      displayStateCache[element] = element.style.display;
      element.style.display = 'none';
    });
  };

  ElementCollection.prototype.css = function(nameOrHash, value) {
    if (this.length === 0) return;

    if (typeof(nameOrHash) !== 'string') {

      return this.forEach(function(element) {
        setCssProperties(element, nameOrHash);
      });

    } else if (value !== undefined) {

      return this.forEach(function(element) {
        setCssProperty(element, nameOrHash, value);
      });

    } else {
      return getCssProperty(this.first, nameOrHash, value);
    }
  };

  // Apply +styles+ to +element+ while executing +callback+, restoring the previous
  // styles after the callback executes.
  ElementCollection.prototype.applyCSS = function (styles, callback) {
    var results = [];

    this.forEach(function(element) {
      results.push(applyCSS(element, styles, callback));
    });

    return results;
  };


  // Make +element+ visible while executing +callback+.
  ElementCollection.prototype.makeVisibleAndYield = function (callback) {
    var hiddenVisually = {
        display: 'block',
        visibility: 'hidden'
      },
      results = [];

    this.forEach(function(element) {
      // find whether it's the element or an ancestor that's display none and
      // then apply to whichever it is
      var targetElement = $.findElementWithDisplayNone(element);
      if (!targetElement) {
        results.push(void 0);
      }
      else {
        results.push(
          applyCSS(targetElement, hiddenVisually, callback)
        );
      }
    });

    return results;
  };


  // @remove
  OTHelpers.show = function(element) {
    return $(element).show();
  };

  // @remove
  OTHelpers.hide = function(element) {
    return $(element).hide();
  };

  // @remove
  OTHelpers.css = function(element, nameOrHash, value) {
    return $(element).css(nameOrHash, value);
  };

  // @remove
  OTHelpers.applyCSS = function(element, styles, callback) {
    return $(element).applyCSS(styles, callback);
  };

  // @remove
  OTHelpers.makeVisibleAndYield = function(element, callback) {
    return $(element).makeVisibleAndYield(callback);
  };

})();

// tb_require('../helpers.js')

/*!
 * @overview RSVP - a tiny implementation of Promises/A+.
 * @copyright Copyright (c) 2014 Yehuda Katz, Tom Dale, Stefan Penner and contributors
 * @license   Licensed under MIT license
 *            See https://raw.githubusercontent.com/tildeio/rsvp.js/master/LICENSE
 * @version   3.0.16
 */

/// OpenTok: Modified for inclusion in OpenTok. Disable all the module stuff and
/// just mount it on OTHelpers. Also tweaked some of the linting stuff as it conflicted
/// with our linter settings.

/* jshint ignore:start */
(function() {
    'use strict';
    function lib$rsvp$utils$$objectOrFunction(x) {
      return typeof x === 'function' || (typeof x === 'object' && x !== null);
    }

    function lib$rsvp$utils$$isFunction(x) {
      return typeof x === 'function';
    }

    function lib$rsvp$utils$$isMaybeThenable(x) {
      return typeof x === 'object' && x !== null;
    }

    var lib$rsvp$utils$$_isArray;
    if (!Array.isArray) {
      lib$rsvp$utils$$_isArray = function (x) {
        return Object.prototype.toString.call(x) === '[object Array]';
      };
    } else {
      lib$rsvp$utils$$_isArray = Array.isArray;
    }

    var lib$rsvp$utils$$isArray = lib$rsvp$utils$$_isArray;

    var lib$rsvp$utils$$now = Date.now || function() { return new Date().getTime(); };

    function lib$rsvp$utils$$F() { }

    var lib$rsvp$utils$$o_create = (Object.create || function (o) {
      if (arguments.length > 1) {
        throw new Error('Second argument not supported');
      }
      if (typeof o !== 'object') {
        throw new TypeError('Argument must be an object');
      }
      lib$rsvp$utils$$F.prototype = o;
      return new lib$rsvp$utils$$F();
    });
    function lib$rsvp$events$$indexOf(callbacks, callback) {
      for (var i=0, l=callbacks.length; i<l; i++) {
        if (callbacks[i] === callback) { return i; }
      }

      return -1;
    }

    function lib$rsvp$events$$callbacksFor(object) {
      var callbacks = object._promiseCallbacks;

      if (!callbacks) {
        callbacks = object._promiseCallbacks = {};
      }

      return callbacks;
    }

    var lib$rsvp$events$$default = {

      /**
        `RSVP.EventTarget.mixin` extends an object with EventTarget methods. For
        Example:

        ```javascript
        var object = {};

        RSVP.EventTarget.mixin(object);

        object.on('finished', function(event) {
          // handle event
        });

        object.trigger('finished', { detail: value });
        ```

        `EventTarget.mixin` also works with prototypes:

        ```javascript
        var Person = function() {};
        RSVP.EventTarget.mixin(Person.prototype);

        var yehuda = new Person();
        var tom = new Person();

        yehuda.on('poke', function(event) {
          console.log('Yehuda says OW');
        });

        tom.on('poke', function(event) {
          console.log('Tom says OW');
        });

        yehuda.trigger('poke');
        tom.trigger('poke');
        ```

        @method mixin
        @for RSVP.EventTarget
        @private
        @param {Object} object object to extend with EventTarget methods
      */
      'mixin': function(object) {
        object['on']      = this['on'];
        object['off']     = this['off'];
        object['trigger'] = this['trigger'];
        object._promiseCallbacks = undefined;
        return object;
      },

      /**
        Registers a callback to be executed when `eventName` is triggered

        ```javascript
        object.on('event', function(eventInfo){
          // handle the event
        });

        object.trigger('event');
        ```

        @method on
        @for RSVP.EventTarget
        @private
        @param {String} eventName name of the event to listen for
        @param {Function} callback function to be called when the event is triggered.
      */
      'on': function(eventName, callback) {
        var allCallbacks = lib$rsvp$events$$callbacksFor(this), callbacks;

        callbacks = allCallbacks[eventName];

        if (!callbacks) {
          callbacks = allCallbacks[eventName] = [];
        }

        if (lib$rsvp$events$$indexOf(callbacks, callback) === -1) {
          callbacks.push(callback);
        }
      },

      /**
        You can use `off` to stop firing a particular callback for an event:

        ```javascript
        function doStuff() { // do stuff! }
        object.on('stuff', doStuff);

        object.trigger('stuff'); // doStuff will be called

        // Unregister ONLY the doStuff callback
        object.off('stuff', doStuff);
        object.trigger('stuff'); // doStuff will NOT be called
        ```

        If you don't pass a `callback` argument to `off`, ALL callbacks for the
        event will not be executed when the event fires. For example:

        ```javascript
        var callback1 = function(){};
        var callback2 = function(){};

        object.on('stuff', callback1);
        object.on('stuff', callback2);

        object.trigger('stuff'); // callback1 and callback2 will be executed.

        object.off('stuff');
        object.trigger('stuff'); // callback1 and callback2 will not be executed!
        ```

        @method off
        @for RSVP.EventTarget
        @private
        @param {String} eventName event to stop listening to
        @param {Function} callback optional argument. If given, only the function
        given will be removed from the event's callback queue. If no `callback`
        argument is given, all callbacks will be removed from the event's callback
        queue.
      */
      'off': function(eventName, callback) {
        var allCallbacks = lib$rsvp$events$$callbacksFor(this), callbacks, index;

        if (!callback) {
          allCallbacks[eventName] = [];
          return;
        }

        callbacks = allCallbacks[eventName];

        index = lib$rsvp$events$$indexOf(callbacks, callback);

        if (index !== -1) { callbacks.splice(index, 1); }
      },

      /**
        Use `trigger` to fire custom events. For example:

        ```javascript
        object.on('foo', function(){
          console.log('foo event happened!');
        });
        object.trigger('foo');
        // 'foo event happened!' logged to the console
        ```

        You can also pass a value as a second argument to `trigger` that will be
        passed as an argument to all event listeners for the event:

        ```javascript
        object.on('foo', function(value){
          console.log(value.name);
        });

        object.trigger('foo', { name: 'bar' });
        // 'bar' logged to the console
        ```

        @method trigger
        @for RSVP.EventTarget
        @private
        @param {String} eventName name of the event to be triggered
        @param {Any} options optional value to be passed to any event handlers for
        the given `eventName`
      */
      'trigger': function(eventName, options) {
        var allCallbacks = lib$rsvp$events$$callbacksFor(this), callbacks, callback;

        if (callbacks = allCallbacks[eventName]) {
          // Don't cache the callbacks.length since it may grow
          for (var i=0; i<callbacks.length; i++) {
            callback = callbacks[i];

            callback(options);
          }
        }
      }
    };

    var lib$rsvp$config$$config = {
      instrument: false
    };

    lib$rsvp$events$$default['mixin'](lib$rsvp$config$$config);

    function lib$rsvp$config$$configure(name, value) {
      if (name === 'onerror') {
        // handle for legacy users that expect the actual
        // error to be passed to their function added via
        // `RSVP.configure('onerror', someFunctionHere);`
        lib$rsvp$config$$config['on']('error', value);
        return;
      }

      if (arguments.length === 2) {
        lib$rsvp$config$$config[name] = value;
      } else {
        return lib$rsvp$config$$config[name];
      }
    }

    var lib$rsvp$instrument$$queue = [];

    function lib$rsvp$instrument$$scheduleFlush() {
      setTimeout(function() {
        var entry;
        for (var i = 0; i < lib$rsvp$instrument$$queue.length; i++) {
          entry = lib$rsvp$instrument$$queue[i];

          var payload = entry.payload;

          payload.guid = payload.key + payload.id;
          payload.childGuid = payload.key + payload.childId;
          if (payload.error) {
            payload.stack = payload.error.stack;
          }

          lib$rsvp$config$$config['trigger'](entry.name, entry.payload);
        }
        lib$rsvp$instrument$$queue.length = 0;
      }, 50);
    }

    function lib$rsvp$instrument$$instrument(eventName, promise, child) {
      if (1 === lib$rsvp$instrument$$queue.push({
          name: eventName,
          payload: {
            key: promise._guidKey,
            id:  promise._id,
            eventName: eventName,
            detail: promise._result,
            childId: child && child._id,
            label: promise._label,
            timeStamp: lib$rsvp$utils$$now(),
            error: lib$rsvp$config$$config["instrument-with-stack"] ? new Error(promise._label) : null
          }})) {
            lib$rsvp$instrument$$scheduleFlush();
          }
      }
    var lib$rsvp$instrument$$default = lib$rsvp$instrument$$instrument;

    function  lib$rsvp$$internal$$withOwnPromise() {
      return new TypeError('A promises callback cannot return that same promise.');
    }

    function lib$rsvp$$internal$$noop() {}

    var lib$rsvp$$internal$$PENDING   = void 0;
    var lib$rsvp$$internal$$FULFILLED = 1;
    var lib$rsvp$$internal$$REJECTED  = 2;

    var lib$rsvp$$internal$$GET_THEN_ERROR = new lib$rsvp$$internal$$ErrorObject();

    function lib$rsvp$$internal$$getThen(promise) {
      try {
        return promise.then;
      } catch(error) {
        lib$rsvp$$internal$$GET_THEN_ERROR.error = error;
        return lib$rsvp$$internal$$GET_THEN_ERROR;
      }
    }

    function lib$rsvp$$internal$$tryThen(then, value, fulfillmentHandler, rejectionHandler) {
      try {
        then.call(value, fulfillmentHandler, rejectionHandler);
      } catch(e) {
        return e;
      }
    }

    function lib$rsvp$$internal$$handleForeignThenable(promise, thenable, then) {
      lib$rsvp$config$$config.async(function(promise) {
        var sealed = false;
        var error = lib$rsvp$$internal$$tryThen(then, thenable, function(value) {
          if (sealed) { return; }
          sealed = true;
          if (thenable !== value) {
            lib$rsvp$$internal$$resolve(promise, value);
          } else {
            lib$rsvp$$internal$$fulfill(promise, value);
          }
        }, function(reason) {
          if (sealed) { return; }
          sealed = true;

          lib$rsvp$$internal$$reject(promise, reason);
        }, 'Settle: ' + (promise._label || ' unknown promise'));

        if (!sealed && error) {
          sealed = true;
          lib$rsvp$$internal$$reject(promise, error);
        }
      }, promise);
    }

    function lib$rsvp$$internal$$handleOwnThenable(promise, thenable) {
      if (thenable._state === lib$rsvp$$internal$$FULFILLED) {
        lib$rsvp$$internal$$fulfill(promise, thenable._result);
      } else if (thenable._state === lib$rsvp$$internal$$REJECTED) {
        thenable._onError = null;
        lib$rsvp$$internal$$reject(promise, thenable._result);
      } else {
        lib$rsvp$$internal$$subscribe(thenable, undefined, function(value) {
          if (thenable !== value) {
            lib$rsvp$$internal$$resolve(promise, value);
          } else {
            lib$rsvp$$internal$$fulfill(promise, value);
          }
        }, function(reason) {
          lib$rsvp$$internal$$reject(promise, reason);
        });
      }
    }

    function lib$rsvp$$internal$$handleMaybeThenable(promise, maybeThenable) {
      if (maybeThenable.constructor === promise.constructor) {
        lib$rsvp$$internal$$handleOwnThenable(promise, maybeThenable);
      } else {
        var then = lib$rsvp$$internal$$getThen(maybeThenable);

        if (then === lib$rsvp$$internal$$GET_THEN_ERROR) {
          lib$rsvp$$internal$$reject(promise, lib$rsvp$$internal$$GET_THEN_ERROR.error);
        } else if (then === undefined) {
          lib$rsvp$$internal$$fulfill(promise, maybeThenable);
        } else if (lib$rsvp$utils$$isFunction(then)) {
          lib$rsvp$$internal$$handleForeignThenable(promise, maybeThenable, then);
        } else {
          lib$rsvp$$internal$$fulfill(promise, maybeThenable);
        }
      }
    }

    function lib$rsvp$$internal$$resolve(promise, value) {
      if (promise === value) {
        lib$rsvp$$internal$$fulfill(promise, value);
      } else if (lib$rsvp$utils$$objectOrFunction(value)) {
        lib$rsvp$$internal$$handleMaybeThenable(promise, value);
      } else {
        lib$rsvp$$internal$$fulfill(promise, value);
      }
    }

    function lib$rsvp$$internal$$publishRejection(promise) {
      if (promise._onError) {
        promise._onError(promise._result);
      }

      lib$rsvp$$internal$$publish(promise);
    }

    function lib$rsvp$$internal$$fulfill(promise, value) {
      if (promise._state !== lib$rsvp$$internal$$PENDING) { return; }

      promise._result = value;
      promise._state = lib$rsvp$$internal$$FULFILLED;

      if (promise._subscribers.length === 0) {
        if (lib$rsvp$config$$config.instrument) {
          lib$rsvp$instrument$$default('fulfilled', promise);
        }
      } else {
        lib$rsvp$config$$config.async(lib$rsvp$$internal$$publish, promise);
      }
    }

    function lib$rsvp$$internal$$reject(promise, reason) {
      if (promise._state !== lib$rsvp$$internal$$PENDING) { return; }
      promise._state = lib$rsvp$$internal$$REJECTED;
      promise._result = reason;
      lib$rsvp$config$$config.async(lib$rsvp$$internal$$publishRejection, promise);
    }

    function lib$rsvp$$internal$$subscribe(parent, child, onFulfillment, onRejection) {
      var subscribers = parent._subscribers;
      var length = subscribers.length;

      parent._onError = null;

      subscribers[length] = child;
      subscribers[length + lib$rsvp$$internal$$FULFILLED] = onFulfillment;
      subscribers[length + lib$rsvp$$internal$$REJECTED]  = onRejection;

      if (length === 0 && parent._state) {
        lib$rsvp$config$$config.async(lib$rsvp$$internal$$publish, parent);
      }
    }

    function lib$rsvp$$internal$$publish(promise) {
      var subscribers = promise._subscribers;
      var settled = promise._state;

      if (lib$rsvp$config$$config.instrument) {
        lib$rsvp$instrument$$default(settled === lib$rsvp$$internal$$FULFILLED ? 'fulfilled' : 'rejected', promise);
      }

      if (subscribers.length === 0) { return; }

      var child, callback, detail = promise._result;

      for (var i = 0; i < subscribers.length; i += 3) {
        child = subscribers[i];
        callback = subscribers[i + settled];

        if (child) {
          lib$rsvp$$internal$$invokeCallback(settled, child, callback, detail);
        } else {
          callback(detail);
        }
      }

      promise._subscribers.length = 0;
    }

    function lib$rsvp$$internal$$ErrorObject() {
      this.error = null;
    }

    var lib$rsvp$$internal$$TRY_CATCH_ERROR = new lib$rsvp$$internal$$ErrorObject();

    function lib$rsvp$$internal$$tryCatch(callback, detail) {
      try {
        return callback(detail);
      } catch(e) {
        lib$rsvp$$internal$$TRY_CATCH_ERROR.error = e;
        return lib$rsvp$$internal$$TRY_CATCH_ERROR;
      }
    }

    function lib$rsvp$$internal$$invokeCallback(settled, promise, callback, detail) {
      var hasCallback = lib$rsvp$utils$$isFunction(callback),
          value, error, succeeded, failed;

      if (hasCallback) {
        value = lib$rsvp$$internal$$tryCatch(callback, detail);

        if (value === lib$rsvp$$internal$$TRY_CATCH_ERROR) {
          failed = true;
          error = value.error;
          value = null;
        } else {
          succeeded = true;
        }

        if (promise === value) {
          lib$rsvp$$internal$$reject(promise, lib$rsvp$$internal$$withOwnPromise());
          return;
        }

      } else {
        value = detail;
        succeeded = true;
      }

      if (promise._state !== lib$rsvp$$internal$$PENDING) {
        // noop
      } else if (hasCallback && succeeded) {
        lib$rsvp$$internal$$resolve(promise, value);
      } else if (failed) {
        lib$rsvp$$internal$$reject(promise, error);
      } else if (settled === lib$rsvp$$internal$$FULFILLED) {
        lib$rsvp$$internal$$fulfill(promise, value);
      } else if (settled === lib$rsvp$$internal$$REJECTED) {
        lib$rsvp$$internal$$reject(promise, value);
      }
    }

    function lib$rsvp$$internal$$initializePromise(promise, resolver) {
      var resolved = false;
      try {
        resolver(function resolvePromise(value){
          if (resolved) { return; }
          resolved = true;
          lib$rsvp$$internal$$resolve(promise, value);
        }, function rejectPromise(reason) {
          if (resolved) { return; }
          resolved = true;
          lib$rsvp$$internal$$reject(promise, reason);
        });
      } catch(e) {
        lib$rsvp$$internal$$reject(promise, e);
      }
    }

    function lib$rsvp$enumerator$$makeSettledResult(state, position, value) {
      if (state === lib$rsvp$$internal$$FULFILLED) {
        return {
          state: 'fulfilled',
          value: value
        };
      } else {
        return {
          state: 'rejected',
          reason: value
        };
      }
    }

    function lib$rsvp$enumerator$$Enumerator(Constructor, input, abortOnReject, label) {
      this._instanceConstructor = Constructor;
      this.promise = new Constructor(lib$rsvp$$internal$$noop, label);
      this._abortOnReject = abortOnReject;

      if (this._validateInput(input)) {
        this._input     = input;
        this.length     = input.length;
        this._remaining = input.length;

        this._init();

        if (this.length === 0) {
          lib$rsvp$$internal$$fulfill(this.promise, this._result);
        } else {
          this.length = this.length || 0;
          this._enumerate();
          if (this._remaining === 0) {
            lib$rsvp$$internal$$fulfill(this.promise, this._result);
          }
        }
      } else {
        lib$rsvp$$internal$$reject(this.promise, this._validationError());
      }
    }

    var lib$rsvp$enumerator$$default = lib$rsvp$enumerator$$Enumerator;

    lib$rsvp$enumerator$$Enumerator.prototype._validateInput = function(input) {
      return lib$rsvp$utils$$isArray(input);
    };

    lib$rsvp$enumerator$$Enumerator.prototype._validationError = function() {
      return new Error('Array Methods must be provided an Array');
    };

    lib$rsvp$enumerator$$Enumerator.prototype._init = function() {
      this._result = new Array(this.length);
    };

    lib$rsvp$enumerator$$Enumerator.prototype._enumerate = function() {
      var length  = this.length;
      var promise = this.promise;
      var input   = this._input;

      for (var i = 0; promise._state === lib$rsvp$$internal$$PENDING && i < length; i++) {
        this._eachEntry(input[i], i);
      }
    };

    lib$rsvp$enumerator$$Enumerator.prototype._eachEntry = function(entry, i) {
      var c = this._instanceConstructor;
      if (lib$rsvp$utils$$isMaybeThenable(entry)) {
        if (entry.constructor === c && entry._state !== lib$rsvp$$internal$$PENDING) {
          entry._onError = null;
          this._settledAt(entry._state, i, entry._result);
        } else {
          this._willSettleAt(c.resolve(entry), i);
        }
      } else {
        this._remaining--;
        this._result[i] = this._makeResult(lib$rsvp$$internal$$FULFILLED, i, entry);
      }
    };

    lib$rsvp$enumerator$$Enumerator.prototype._settledAt = function(state, i, value) {
      var promise = this.promise;

      if (promise._state === lib$rsvp$$internal$$PENDING) {
        this._remaining--;

        if (this._abortOnReject && state === lib$rsvp$$internal$$REJECTED) {
          lib$rsvp$$internal$$reject(promise, value);
        } else {
          this._result[i] = this._makeResult(state, i, value);
        }
      }

      if (this._remaining === 0) {
        lib$rsvp$$internal$$fulfill(promise, this._result);
      }
    };

    lib$rsvp$enumerator$$Enumerator.prototype._makeResult = function(state, i, value) {
      return value;
    };

    lib$rsvp$enumerator$$Enumerator.prototype._willSettleAt = function(promise, i) {
      var enumerator = this;

      lib$rsvp$$internal$$subscribe(promise, undefined, function(value) {
        enumerator._settledAt(lib$rsvp$$internal$$FULFILLED, i, value);
      }, function(reason) {
        enumerator._settledAt(lib$rsvp$$internal$$REJECTED, i, reason);
      });
    };
    function lib$rsvp$promise$all$$all(entries, label) {
      return new lib$rsvp$enumerator$$default(this, entries, true /* abort on reject */, label).promise;
    }
    var lib$rsvp$promise$all$$default = lib$rsvp$promise$all$$all;
    function lib$rsvp$promise$race$$race(entries, label) {
      var Constructor = this;

      var promise = new Constructor(lib$rsvp$$internal$$noop, label);

      if (!lib$rsvp$utils$$isArray(entries)) {
        lib$rsvp$$internal$$reject(promise, new TypeError('You must pass an array to race.'));
        return promise;
      }

      var length = entries.length;

      function onFulfillment(value) {
        lib$rsvp$$internal$$resolve(promise, value);
      }

      function onRejection(reason) {
        lib$rsvp$$internal$$reject(promise, reason);
      }

      for (var i = 0; promise._state === lib$rsvp$$internal$$PENDING && i < length; i++) {
        lib$rsvp$$internal$$subscribe(Constructor.resolve(entries[i]), undefined, onFulfillment, onRejection);
      }

      return promise;
    }
    var lib$rsvp$promise$race$$default = lib$rsvp$promise$race$$race;
    function lib$rsvp$promise$resolve$$resolve(object, label) {
      var Constructor = this;

      if (object && typeof object === 'object' && object.constructor === Constructor) {
        return object;
      }

      var promise = new Constructor(lib$rsvp$$internal$$noop, label);
      lib$rsvp$$internal$$resolve(promise, object);
      return promise;
    }
    var lib$rsvp$promise$resolve$$default = lib$rsvp$promise$resolve$$resolve;
    function lib$rsvp$promise$reject$$reject(reason, label) {
      var Constructor = this;
      var promise = new Constructor(lib$rsvp$$internal$$noop, label);
      lib$rsvp$$internal$$reject(promise, reason);
      return promise;
    }
    var lib$rsvp$promise$reject$$default = lib$rsvp$promise$reject$$reject;

    var lib$rsvp$promise$$guidKey = 'rsvp_' + lib$rsvp$utils$$now() + '-';
    var lib$rsvp$promise$$counter = 0;

    function lib$rsvp$promise$$needsResolver() {
      throw new TypeError('You must pass a resolver function as the first argument to the promise constructor');
    }

    function lib$rsvp$promise$$needsNew() {
      throw new TypeError("Failed to construct 'Promise': Please use the 'new' operator, this object constructor cannot be called as a function.");
    }

    /**
      Promise objects represent the eventual result of an asynchronous operation. The
      primary way of interacting with a promise is through its `then` method, which
      registers callbacks to receive either a promiseâ€™s eventual value or the reason
      why the promise cannot be fulfilled.

      Terminology
      -----------

      - `promise` is an object or function with a `then` method whose behavior conforms to this specification.
      - `thenable` is an object or function that defines a `then` method.
      - `value` is any legal JavaScript value (including undefined, a thenable, or a promise).
      - `exception` is a value that is thrown using the throw statement.
      - `reason` is a value that indicates why a promise was rejected.
      - `settled` the final resting state of a promise, fulfilled or rejected.

      A promise can be in one of three states: pending, fulfilled, or rejected.

      Promises that are fulfilled have a fulfillment value and are in the fulfilled
      state.  Promises that are rejected have a rejection reason and are in the
      rejected state.  A fulfillment value is never a thenable.

      Promises can also be said to *resolve* a value.  If this value is also a
      promise, then the original promise's settled state will match the value's
      settled state.  So a promise that *resolves* a promise that rejects will
      itself reject, and a promise that *resolves* a promise that fulfills will
      itself fulfill.


      Basic Usage:
      ------------

      ```js
      var promise = new Promise(function(resolve, reject) {
        // on success
        resolve(value);

        // on failure
        reject(reason);
      });

      promise.then(function(value) {
        // on fulfillment
      }, function(reason) {
        // on rejection
      });
      ```

      Advanced Usage:
      ---------------

      Promises shine when abstracting away asynchronous interactions such as
      `XMLHttpRequest`s.

      ```js
      function getJSON(url) {
        return new Promise(function(resolve, reject){
          var xhr = new XMLHttpRequest();

          xhr.open('GET', url);
          xhr.onreadystatechange = handler;
          xhr.responseType = 'json';
          xhr.setRequestHeader('Accept', 'application/json');
          xhr.send();

          function handler() {
            if (this.readyState === this.DONE) {
              if (this.status === 200) {
                resolve(this.response);
              } else {
                reject(new Error('getJSON: `' + url + '` failed with status: [' + this.status + ']'));
              }
            }
          };
        });
      }

      getJSON('/posts.json').then(function(json) {
        // on fulfillment
      }, function(reason) {
        // on rejection
      });
      ```

      Unlike callbacks, promises are great composable primitives.

      ```js
      Promise.all([
        getJSON('/posts'),
        getJSON('/comments')
      ]).then(function(values){
        values[0] // => postsJSON
        values[1] // => commentsJSON

        return values;
      });
      ```

      @class RSVP.Promise
      @param {function} resolver
      @param {String} label optional string for labeling the promise.
      Useful for tooling.
      @constructor
    */
    function lib$rsvp$promise$$Promise(resolver, label) {
      this._id = lib$rsvp$promise$$counter++;
      this._label = label;
      this._state = undefined;
      this._result = undefined;
      this._subscribers = [];

      if (lib$rsvp$config$$config.instrument) {
        lib$rsvp$instrument$$default('created', this);
      }

      if (lib$rsvp$$internal$$noop !== resolver) {
        if (!lib$rsvp$utils$$isFunction(resolver)) {
          lib$rsvp$promise$$needsResolver();
        }

        if (!(this instanceof lib$rsvp$promise$$Promise)) {
          lib$rsvp$promise$$needsNew();
        }

        lib$rsvp$$internal$$initializePromise(this, resolver);
      }
    }

    var lib$rsvp$promise$$default = lib$rsvp$promise$$Promise;

    // deprecated
    lib$rsvp$promise$$Promise.cast = lib$rsvp$promise$resolve$$default;
    lib$rsvp$promise$$Promise.all = lib$rsvp$promise$all$$default;
    lib$rsvp$promise$$Promise.race = lib$rsvp$promise$race$$default;
    lib$rsvp$promise$$Promise.resolve = lib$rsvp$promise$resolve$$default;
    lib$rsvp$promise$$Promise.reject = lib$rsvp$promise$reject$$default;

    lib$rsvp$promise$$Promise.prototype = {
      constructor: lib$rsvp$promise$$Promise,

      _guidKey: lib$rsvp$promise$$guidKey,

      _onError: function (reason) {
        lib$rsvp$config$$config.async(function(promise) {
          setTimeout(function() {
            if (promise._onError) {
              lib$rsvp$config$$config['trigger']('error', reason);
            }
          }, 0);
        }, this);
      },

    /**
      The primary way of interacting with a promise is through its `then` method,
      which registers callbacks to receive either a promise's eventual value or the
      reason why the promise cannot be fulfilled.

      ```js
      findUser().then(function(user){
        // user is available
      }, function(reason){
        // user is unavailable, and you are given the reason why
      });
      ```

      Chaining
      --------

      The return value of `then` is itself a promise.  This second, 'downstream'
      promise is resolved with the return value of the first promise's fulfillment
      or rejection handler, or rejected if the handler throws an exception.

      ```js
      findUser().then(function (user) {
        return user.name;
      }, function (reason) {
        return 'default name';
      }).then(function (userName) {
        // If `findUser` fulfilled, `userName` will be the user's name, otherwise it
        // will be `'default name'`
      });

      findUser().then(function (user) {
        throw new Error('Found user, but still unhappy');
      }, function (reason) {
        throw new Error('`findUser` rejected and we're unhappy');
      }).then(function (value) {
        // never reached
      }, function (reason) {
        // if `findUser` fulfilled, `reason` will be 'Found user, but still unhappy'.
        // If `findUser` rejected, `reason` will be '`findUser` rejected and we're unhappy'.
      });
      ```
      If the downstream promise does not specify a rejection handler, rejection reasons will be propagated further downstream.

      ```js
      findUser().then(function (user) {
        throw new PedagogicalException('Upstream error');
      }).then(function (value) {
        // never reached
      }).then(function (value) {
        // never reached
      }, function (reason) {
        // The `PedgagocialException` is propagated all the way down to here
      });
      ```

      Assimilation
      ------------

      Sometimes the value you want to propagate to a downstream promise can only be
      retrieved asynchronously. This can be achieved by returning a promise in the
      fulfillment or rejection handler. The downstream promise will then be pending
      until the returned promise is settled. This is called *assimilation*.

      ```js
      findUser().then(function (user) {
        return findCommentsByAuthor(user);
      }).then(function (comments) {
        // The user's comments are now available
      });
      ```

      If the assimliated promise rejects, then the downstream promise will also reject.

      ```js
      findUser().then(function (user) {
        return findCommentsByAuthor(user);
      }).then(function (comments) {
        // If `findCommentsByAuthor` fulfills, we'll have the value here
      }, function (reason) {
        // If `findCommentsByAuthor` rejects, we'll have the reason here
      });
      ```

      Simple Example
      --------------

      Synchronous Example

      ```javascript
      var result;

      try {
        result = findResult();
        // success
      } catch(reason) {
        // failure
      }
      ```

      Errback Example

      ```js
      findResult(function(result, err){
        if (err) {
          // failure
        } else {
          // success
        }
      });
      ```

      Promise Example;

      ```javascript
      findResult().then(function(result){
        // success
      }, function(reason){
        // failure
      });
      ```

      Advanced Example
      --------------

      Synchronous Example

      ```javascript
      var author, books;

      try {
        author = findAuthor();
        books  = findBooksByAuthor(author);
        // success
      } catch(reason) {
        // failure
      }
      ```

      Errback Example

      ```js

      function foundBooks(books) {

      }

      function failure(reason) {

      }

      findAuthor(function(author, err){
        if (err) {
          failure(err);
          // failure
        } else {
          try {
            findBoooksByAuthor(author, function(books, err) {
              if (err) {
                failure(err);
              } else {
                try {
                  foundBooks(books);
                } catch(reason) {
                  failure(reason);
                }
              }
            });
          } catch(error) {
            failure(err);
          }
          // success
        }
      });
      ```

      Promise Example;

      ```javascript
      findAuthor().
        then(findBooksByAuthor).
        then(function(books){
          // found books
      }).catch(function(reason){
        // something went wrong
      });
      ```

      @method then
      @param {Function} onFulfilled
      @param {Function} onRejected
      @param {String} label optional string for labeling the promise.
      Useful for tooling.
      @return {Promise}
    */
      then: function(onFulfillment, onRejection, label) {
        var parent = this;
        var state = parent._state;

        if (state === lib$rsvp$$internal$$FULFILLED && !onFulfillment || state === lib$rsvp$$internal$$REJECTED && !onRejection) {
          if (lib$rsvp$config$$config.instrument) {
            lib$rsvp$instrument$$default('chained', this, this);
          }
          return this;
        }

        parent._onError = null;

        var child = new this.constructor(lib$rsvp$$internal$$noop, label);
        var result = parent._result;

        if (lib$rsvp$config$$config.instrument) {
          lib$rsvp$instrument$$default('chained', parent, child);
        }

        if (state) {
          var callback = arguments[state - 1];
          lib$rsvp$config$$config.async(function(){
            lib$rsvp$$internal$$invokeCallback(state, child, callback, result);
          });
        } else {
          lib$rsvp$$internal$$subscribe(parent, child, onFulfillment, onRejection);
        }

        return child;
      },

    /**
      `catch` is simply sugar for `then(undefined, onRejection)` which makes it the same
      as the catch block of a try/catch statement.

      ```js
      function findAuthor(){
        throw new Error('couldn't find that author');
      }

      // synchronous
      try {
        findAuthor();
      } catch(reason) {
        // something went wrong
      }

      // async with promises
      findAuthor().catch(function(reason){
        // something went wrong
      });
      ```

      @method catch
      @param {Function} onRejection
      @param {String} label optional string for labeling the promise.
      Useful for tooling.
      @return {Promise}
    */
      'catch': function(onRejection, label) {
        return this.then(null, onRejection, label);
      },

    /**
      `finally` will be invoked regardless of the promise's fate just as native
      try/catch/finally behaves

      Synchronous example:

      ```js
      findAuthor() {
        if (Math.random() > 0.5) {
          throw new Error();
        }
        return new Author();
      }

      try {
        return findAuthor(); // succeed or fail
      } catch(error) {
        return findOtherAuther();
      } finally {
        // always runs
        // doesn't affect the return value
      }
      ```

      Asynchronous example:

      ```js
      findAuthor().catch(function(reason){
        return findOtherAuther();
      }).finally(function(){
        // author was either found, or not
      });
      ```

      @method finally
      @param {Function} callback
      @param {String} label optional string for labeling the promise.
      Useful for tooling.
      @return {Promise}
    */
      'finally': function(callback, label) {
        var constructor = this.constructor;

        return this.then(function(value) {
          return constructor.resolve(callback()).then(function(){
            return value;
          });
        }, function(reason) {
          return constructor.resolve(callback()).then(function(){
            throw reason;
          });
        }, label);
      }
    };

    function lib$rsvp$all$settled$$AllSettled(Constructor, entries, label) {
      this._superConstructor(Constructor, entries, false /* don't abort on reject */, label);
    }

    lib$rsvp$all$settled$$AllSettled.prototype = lib$rsvp$utils$$o_create(lib$rsvp$enumerator$$default.prototype);
    lib$rsvp$all$settled$$AllSettled.prototype._superConstructor = lib$rsvp$enumerator$$default;
    lib$rsvp$all$settled$$AllSettled.prototype._makeResult = lib$rsvp$enumerator$$makeSettledResult;
    lib$rsvp$all$settled$$AllSettled.prototype._validationError = function() {
      return new Error('allSettled must be called with an array');
    };

    function lib$rsvp$all$settled$$allSettled(entries, label) {
      return new lib$rsvp$all$settled$$AllSettled(lib$rsvp$promise$$default, entries, label).promise;
    }
    var lib$rsvp$all$settled$$default = lib$rsvp$all$settled$$allSettled;
    function lib$rsvp$all$$all(array, label) {
      return lib$rsvp$promise$$default.all(array, label);
    }
    var lib$rsvp$all$$default = lib$rsvp$all$$all;
    var lib$rsvp$asap$$len = 0;
    var lib$rsvp$asap$$toString = {}.toString;
    var lib$rsvp$asap$$vertxNext;
    function lib$rsvp$asap$$asap(callback, arg) {
      lib$rsvp$asap$$queue[lib$rsvp$asap$$len] = callback;
      lib$rsvp$asap$$queue[lib$rsvp$asap$$len + 1] = arg;
      lib$rsvp$asap$$len += 2;
      if (lib$rsvp$asap$$len === 2) {
        // If len is 1, that means that we need to schedule an async flush.
        // If additional callbacks are queued before the queue is flushed, they
        // will be processed by this flush that we are scheduling.
        lib$rsvp$asap$$scheduleFlush();
      }
    }

    var lib$rsvp$asap$$default = lib$rsvp$asap$$asap;

    var lib$rsvp$asap$$browserWindow = (typeof window !== 'undefined') ? window : undefined;
    var lib$rsvp$asap$$browserGlobal = lib$rsvp$asap$$browserWindow || {};
    var lib$rsvp$asap$$BrowserMutationObserver = lib$rsvp$asap$$browserGlobal.MutationObserver || lib$rsvp$asap$$browserGlobal.WebKitMutationObserver;
    var lib$rsvp$asap$$isNode = typeof process !== 'undefined' && {}.toString.call(process) === '[object process]';

    // test for web worker but not in IE10
    var lib$rsvp$asap$$isWorker = typeof Uint8ClampedArray !== 'undefined' &&
      typeof importScripts !== 'undefined' &&
      typeof MessageChannel !== 'undefined';

    // node
    function lib$rsvp$asap$$useNextTick() {
      var nextTick = process.nextTick;
      // node version 0.10.x displays a deprecation warning when nextTick is used recursively
      // setImmediate should be used instead instead
      var version = process.versions.node.match(/^(?:(\d+)\.)?(?:(\d+)\.)?(\*|\d+)$/);
      if (Array.isArray(version) && version[1] === '0' && version[2] === '10') {
        nextTick = setImmediate;
      }
      return function() {
        nextTick(lib$rsvp$asap$$flush);
      };
    }

    // vertx
    function lib$rsvp$asap$$useVertxTimer() {
      return function() {
        lib$rsvp$asap$$vertxNext(lib$rsvp$asap$$flush);
      };
    }

    function lib$rsvp$asap$$useMutationObserver() {
      var iterations = 0;
      var observer = new lib$rsvp$asap$$BrowserMutationObserver(lib$rsvp$asap$$flush);
      var node = document.createTextNode('');
      observer.observe(node, { characterData: true });

      return function() {
        node.data = (iterations = ++iterations % 2);
      };
    }

    // web worker
    function lib$rsvp$asap$$useMessageChannel() {
      var channel = new MessageChannel();
      channel.port1.onmessage = lib$rsvp$asap$$flush;
      return function () {
        channel.port2.postMessage(0);
      };
    }

    function lib$rsvp$asap$$useSetTimeout() {
      return function() {
        setTimeout(lib$rsvp$asap$$flush, 1);
      };
    }

    var lib$rsvp$asap$$queue = new Array(1000);
    function lib$rsvp$asap$$flush() {
      for (var i = 0; i < lib$rsvp$asap$$len; i+=2) {
        var callback = lib$rsvp$asap$$queue[i];
        var arg = lib$rsvp$asap$$queue[i+1];

        callback(arg);

        lib$rsvp$asap$$queue[i] = undefined;
        lib$rsvp$asap$$queue[i+1] = undefined;
      }

      lib$rsvp$asap$$len = 0;
    }

    function lib$rsvp$asap$$attemptVertex() {
      try {
        var r = require;
        var vertx = r('vertx');
        lib$rsvp$asap$$vertxNext = vertx.runOnLoop || vertx.runOnContext;
        return lib$rsvp$asap$$useVertxTimer();
      } catch(e) {
        return lib$rsvp$asap$$useSetTimeout();
      }
    }

    var lib$rsvp$asap$$scheduleFlush;
    // Decide what async method to use to triggering processing of queued callbacks:
    if (lib$rsvp$asap$$isNode) {
      lib$rsvp$asap$$scheduleFlush = lib$rsvp$asap$$useNextTick();
    } else if (lib$rsvp$asap$$BrowserMutationObserver) {
      lib$rsvp$asap$$scheduleFlush = lib$rsvp$asap$$useMutationObserver();
    } else if (lib$rsvp$asap$$isWorker) {
      lib$rsvp$asap$$scheduleFlush = lib$rsvp$asap$$useMessageChannel();
    } else if (lib$rsvp$asap$$browserWindow === undefined && typeof require === 'function') {
      lib$rsvp$asap$$scheduleFlush = lib$rsvp$asap$$attemptVertex();
    } else {
      lib$rsvp$asap$$scheduleFlush = lib$rsvp$asap$$useSetTimeout();
    }
    function lib$rsvp$defer$$defer(label) {
      var deferred = { };

      deferred['promise'] = new lib$rsvp$promise$$default(function(resolve, reject) {
        deferred['resolve'] = resolve;
        deferred['reject'] = reject;
      }, label);

      return deferred;
    }
    var lib$rsvp$defer$$default = lib$rsvp$defer$$defer;
    function lib$rsvp$filter$$filter(promises, filterFn, label) {
      return lib$rsvp$promise$$default.all(promises, label).then(function(values) {
        if (!lib$rsvp$utils$$isFunction(filterFn)) {
          throw new TypeError("You must pass a function as filter's second argument.");
        }

        var length = values.length;
        var filtered = new Array(length);

        for (var i = 0; i < length; i++) {
          filtered[i] = filterFn(values[i]);
        }

        return lib$rsvp$promise$$default.all(filtered, label).then(function(filtered) {
          var results = new Array(length);
          var newLength = 0;

          for (var i = 0; i < length; i++) {
            if (filtered[i]) {
              results[newLength] = values[i];
              newLength++;
            }
          }

          results.length = newLength;

          return results;
        });
      });
    }
    var lib$rsvp$filter$$default = lib$rsvp$filter$$filter;

    function lib$rsvp$promise$hash$$PromiseHash(Constructor, object, label) {
      this._superConstructor(Constructor, object, true, label);
    }

    var lib$rsvp$promise$hash$$default = lib$rsvp$promise$hash$$PromiseHash;

    lib$rsvp$promise$hash$$PromiseHash.prototype = lib$rsvp$utils$$o_create(lib$rsvp$enumerator$$default.prototype);
    lib$rsvp$promise$hash$$PromiseHash.prototype._superConstructor = lib$rsvp$enumerator$$default;
    lib$rsvp$promise$hash$$PromiseHash.prototype._init = function() {
      this._result = {};
    };

    lib$rsvp$promise$hash$$PromiseHash.prototype._validateInput = function(input) {
      return input && typeof input === 'object';
    };

    lib$rsvp$promise$hash$$PromiseHash.prototype._validationError = function() {
      return new Error('Promise.hash must be called with an object');
    };

    lib$rsvp$promise$hash$$PromiseHash.prototype._enumerate = function() {
      var promise = this.promise;
      var input   = this._input;
      var results = [];

      for (var key in input) {
        if (promise._state === lib$rsvp$$internal$$PENDING && input.hasOwnProperty(key)) {
          results.push({
            position: key,
            entry: input[key]
          });
        }
      }

      var length = results.length;
      this._remaining = length;
      var result;

      for (var i = 0; promise._state === lib$rsvp$$internal$$PENDING && i < length; i++) {
        result = results[i];
        this._eachEntry(result.entry, result.position);
      }
    };

    function lib$rsvp$hash$settled$$HashSettled(Constructor, object, label) {
      this._superConstructor(Constructor, object, false, label);
    }

    lib$rsvp$hash$settled$$HashSettled.prototype = lib$rsvp$utils$$o_create(lib$rsvp$promise$hash$$default.prototype);
    lib$rsvp$hash$settled$$HashSettled.prototype._superConstructor = lib$rsvp$enumerator$$default;
    lib$rsvp$hash$settled$$HashSettled.prototype._makeResult = lib$rsvp$enumerator$$makeSettledResult;

    lib$rsvp$hash$settled$$HashSettled.prototype._validationError = function() {
      return new Error('hashSettled must be called with an object');
    };

    function lib$rsvp$hash$settled$$hashSettled(object, label) {
      return new lib$rsvp$hash$settled$$HashSettled(lib$rsvp$promise$$default, object, label).promise;
    }
    var lib$rsvp$hash$settled$$default = lib$rsvp$hash$settled$$hashSettled;
    function lib$rsvp$hash$$hash(object, label) {
      return new lib$rsvp$promise$hash$$default(lib$rsvp$promise$$default, object, label).promise;
    }
    var lib$rsvp$hash$$default = lib$rsvp$hash$$hash;
    function lib$rsvp$map$$map(promises, mapFn, label) {
      return lib$rsvp$promise$$default.all(promises, label).then(function(values) {
        if (!lib$rsvp$utils$$isFunction(mapFn)) {
          throw new TypeError("You must pass a function as map's second argument.");
        }

        var length = values.length;
        var results = new Array(length);

        for (var i = 0; i < length; i++) {
          results[i] = mapFn(values[i]);
        }

        return lib$rsvp$promise$$default.all(results, label);
      });
    }
    var lib$rsvp$map$$default = lib$rsvp$map$$map;

    function lib$rsvp$node$$Result() {
      this.value = undefined;
    }

    var lib$rsvp$node$$ERROR = new lib$rsvp$node$$Result();
    var lib$rsvp$node$$GET_THEN_ERROR = new lib$rsvp$node$$Result();

    function lib$rsvp$node$$getThen(obj) {
      try {
       return obj.then;
      } catch(error) {
        lib$rsvp$node$$ERROR.value= error;
        return lib$rsvp$node$$ERROR;
      }
    }


    function lib$rsvp$node$$tryApply(f, s, a) {
      try {
        f.apply(s, a);
      } catch(error) {
        lib$rsvp$node$$ERROR.value = error;
        return lib$rsvp$node$$ERROR;
      }
    }

    function lib$rsvp$node$$makeObject(_, argumentNames) {
      var obj = {};
      var name;
      var i;
      var length = _.length;
      var args = new Array(length);

      for (var x = 0; x < length; x++) {
        args[x] = _[x];
      }

      for (i = 0; i < argumentNames.length; i++) {
        name = argumentNames[i];
        obj[name] = args[i + 1];
      }

      return obj;
    }

    function lib$rsvp$node$$arrayResult(_) {
      var length = _.length;
      var args = new Array(length - 1);

      for (var i = 1; i < length; i++) {
        args[i - 1] = _[i];
      }

      return args;
    }

    function lib$rsvp$node$$wrapThenable(then, promise) {
      return {
        then: function(onFulFillment, onRejection) {
          return then.call(promise, onFulFillment, onRejection);
        }
      };
    }

    function lib$rsvp$node$$denodeify(nodeFunc, options) {
      var fn = function() {
        var self = this;
        var l = arguments.length;
        var args = new Array(l + 1);
        var arg;
        var promiseInput = false;

        for (var i = 0; i < l; ++i) {
          arg = arguments[i];

          if (!promiseInput) {
            // TODO: clean this up
            promiseInput = lib$rsvp$node$$needsPromiseInput(arg);
            if (promiseInput === lib$rsvp$node$$GET_THEN_ERROR) {
              var p = new lib$rsvp$promise$$default(lib$rsvp$$internal$$noop);
              lib$rsvp$$internal$$reject(p, lib$rsvp$node$$GET_THEN_ERROR.value);
              return p;
            } else if (promiseInput && promiseInput !== true) {
              arg = lib$rsvp$node$$wrapThenable(promiseInput, arg);
            }
          }
          args[i] = arg;
        }

        var promise = new lib$rsvp$promise$$default(lib$rsvp$$internal$$noop);

        args[l] = function(err, val) {
          if (err)
            lib$rsvp$$internal$$reject(promise, err);
          else if (options === undefined)
            lib$rsvp$$internal$$resolve(promise, val);
          else if (options === true)
            lib$rsvp$$internal$$resolve(promise, lib$rsvp$node$$arrayResult(arguments));
          else if (lib$rsvp$utils$$isArray(options))
            lib$rsvp$$internal$$resolve(promise, lib$rsvp$node$$makeObject(arguments, options));
          else
            lib$rsvp$$internal$$resolve(promise, val);
        };

        if (promiseInput) {
          return lib$rsvp$node$$handlePromiseInput(promise, args, nodeFunc, self);
        } else {
          return lib$rsvp$node$$handleValueInput(promise, args, nodeFunc, self);
        }
      };

      fn.__proto__ = nodeFunc;

      return fn;
    }

    var lib$rsvp$node$$default = lib$rsvp$node$$denodeify;

    function lib$rsvp$node$$handleValueInput(promise, args, nodeFunc, self) {
      var result = lib$rsvp$node$$tryApply(nodeFunc, self, args);
      if (result === lib$rsvp$node$$ERROR) {
        lib$rsvp$$internal$$reject(promise, result.value);
      }
      return promise;
    }

    function lib$rsvp$node$$handlePromiseInput(promise, args, nodeFunc, self){
      return lib$rsvp$promise$$default.all(args).then(function(args){
        var result = lib$rsvp$node$$tryApply(nodeFunc, self, args);
        if (result === lib$rsvp$node$$ERROR) {
          lib$rsvp$$internal$$reject(promise, result.value);
        }
        return promise;
      });
    }

    function lib$rsvp$node$$needsPromiseInput(arg) {
      if (arg && typeof arg === 'object') {
        if (arg.constructor === lib$rsvp$promise$$default) {
          return true;
        } else {
          return lib$rsvp$node$$getThen(arg);
        }
      } else {
        return false;
      }
    }
    function lib$rsvp$race$$race(array, label) {
      return lib$rsvp$promise$$default.race(array, label);
    }
    var lib$rsvp$race$$default = lib$rsvp$race$$race;
    function lib$rsvp$reject$$reject(reason, label) {
      return lib$rsvp$promise$$default.reject(reason, label);
    }
    var lib$rsvp$reject$$default = lib$rsvp$reject$$reject;
    function lib$rsvp$resolve$$resolve(value, label) {
      return lib$rsvp$promise$$default.resolve(value, label);
    }
    var lib$rsvp$resolve$$default = lib$rsvp$resolve$$resolve;
    function lib$rsvp$rethrow$$rethrow(reason) {
      setTimeout(function() {
        throw reason;
      });
      throw reason;
    }
    var lib$rsvp$rethrow$$default = lib$rsvp$rethrow$$rethrow;

    // default async is asap;
    lib$rsvp$config$$config.async = lib$rsvp$asap$$default;
    var lib$rsvp$$cast = lib$rsvp$resolve$$default;
    function lib$rsvp$$async(callback, arg) {
      lib$rsvp$config$$config.async(callback, arg);
    }

    function lib$rsvp$$on() {
      lib$rsvp$config$$config['on'].apply(lib$rsvp$config$$config, arguments);
    }

    function lib$rsvp$$off() {
      lib$rsvp$config$$config['off'].apply(lib$rsvp$config$$config, arguments);
    }

    // Set up instrumentation through `window.__PROMISE_INTRUMENTATION__`
    if (typeof window !== 'undefined' && typeof window['__PROMISE_INSTRUMENTATION__'] === 'object') {
      var lib$rsvp$$callbacks = window['__PROMISE_INSTRUMENTATION__'];
      lib$rsvp$config$$configure('instrument', true);
      for (var lib$rsvp$$eventName in lib$rsvp$$callbacks) {
        if (lib$rsvp$$callbacks.hasOwnProperty(lib$rsvp$$eventName)) {
          lib$rsvp$$on(lib$rsvp$$eventName, lib$rsvp$$callbacks[lib$rsvp$$eventName]);
        }
      }
    }

    var lib$rsvp$umd$$RSVP = {
      'race': lib$rsvp$race$$default,
      'Promise': lib$rsvp$promise$$default,
      'allSettled': lib$rsvp$all$settled$$default,
      'hash': lib$rsvp$hash$$default,
      'hashSettled': lib$rsvp$hash$settled$$default,
      'denodeify': lib$rsvp$node$$default,
      'on': lib$rsvp$$on,
      'off': lib$rsvp$$off,
      'map': lib$rsvp$map$$default,
      'filter': lib$rsvp$filter$$default,
      'resolve': lib$rsvp$resolve$$default,
      'reject': lib$rsvp$reject$$default,
      'all': lib$rsvp$all$$default,
      'rethrow': lib$rsvp$rethrow$$default,
      'defer': lib$rsvp$defer$$default,
      'EventTarget': lib$rsvp$events$$default,
      'configure': lib$rsvp$config$$configure,
      'async': lib$rsvp$$async
    };


    OTHelpers.RSVP = lib$rsvp$umd$$RSVP;
}).call(this);
/* jshint ignore:end */

/*global nodeEventing:true, browserEventing:true */

// tb_require('../../helpers.js')
// tb_require('../callbacks.js')
// tb_require('../../vendor/rsvp.js')
// tb_require('./eventing/event.js')
// tb_require('./eventing/node.js')
// tb_require('./eventing/browser.js')

/**
* This base class defines the <code>on</code>, <code>once</code>, and <code>off</code>
* methods of objects that can dispatch events.
*
* @class EventDispatcher
*/
OTHelpers.eventing = function(self, syncronous) {
  var _ = (nodeEventing || browserEventing)(this, syncronous);

 /**
  * Adds an event handler function for one or more events.
  *
  * <p>
  * The following code adds an event handler for one event:
  * </p>
  *
  * <pre>
  * obj.on("eventName", function (event) {
  *     // This is the event handler.
  * });
  * </pre>
  *
  * <p>If you pass in multiple event names and a handler method, the handler is
  * registered for each of those events:</p>
  *
  * <pre>
  * obj.on("eventName1 eventName2",
  *        function (event) {
  *            // This is the event handler.
  *        });
  * </pre>
  *
  * <p>You can also pass in a third <code>context</code> parameter (which is optional) to
  * define the value of <code>this</code> in the handler method:</p>
  *
  * <pre>obj.on("eventName",
  *        function (event) {
  *            // This is the event handler.
  *        },
  *        obj);
  * </pre>
  *
  * <p>
  * The method also supports an alternate syntax, in which the first parameter is an object
  * that is a hash map of event names and handler functions and the second parameter (optional)
  * is the context for this in each handler:
  * </p>
  * <pre>
  * obj.on(
  *    {
  *       eventName1: function (event) {
  *               // This is the handler for eventName1.
  *           },
  *       eventName2:  function (event) {
  *               // This is the handler for eventName2.
  *           }
  *    },
  *    obj);
  * </pre>
  *
  * <p>
  * If you do not add a handler for an event, the event is ignored locally.
  * </p>
  *
  * @param {String} type The string identifying the type of event. You can specify multiple event
  * names in this string, separating them with a space. The event handler will process each of
  * the events.
  * @param {Function} handler The handler function to process the event. This function takes
  * the event object as a parameter.
  * @param {Object} context (Optional) Defines the value of <code>this</code> in the event
  * handler function.
  *
  * @returns {EventDispatcher} The EventDispatcher object.
  *
  * @memberOf EventDispatcher
  * @method #on
  * @see <a href="#off">off()</a>
  * @see <a href="#once">once()</a>
  * @see <a href="#events">Events</a>
  */
  self.on = function(eventNames, handlerOrContext, context) {
    if (typeof(eventNames) === 'string' && handlerOrContext) {
      _.addListeners(eventNames.split(' '), handlerOrContext, context);
    }
    else {
      for (var name in eventNames) {
        if (eventNames.hasOwnProperty(name)) {
          _.addListeners([name], eventNames[name], handlerOrContext);
        }
      }
    }

    return this;
  };


 /**
  * Removes an event handler or handlers.
  *
  * <p>If you pass in one event name and a handler method, the handler is removed for that
  * event:</p>
  *
  * <pre>obj.off("eventName", eventHandler);</pre>
  *
  * <p>If you pass in multiple event names and a handler method, the handler is removed for
  * those events:</p>
  *
  * <pre>obj.off("eventName1 eventName2", eventHandler);</pre>
  *
  * <p>If you pass in an event name (or names) and <i>no</i> handler method, all handlers are
  * removed for those events:</p>
  *
  * <pre>obj.off("event1Name event2Name");</pre>
  *
  * <p>If you pass in no arguments, <i>all</i> event handlers are removed for all events
  * dispatched by the object:</p>
  *
  * <pre>obj.off();</pre>
  *
  * <p>
  * The method also supports an alternate syntax, in which the first parameter is an object that
  * is a hash map of event names and handler functions and the second parameter (optional) is
  * the context for this in each handler:
  * </p>
  * <pre>
  * obj.off(
  *    {
  *       eventName1: event1Handler,
  *       eventName2: event2Handler
  *    });
  * </pre>
  *
  * @param {String} type (Optional) The string identifying the type of event. You can
  * use a space to specify multiple events, as in "accessAllowed accessDenied
  * accessDialogClosed". If you pass in no <code>type</code> value (or other arguments),
  * all event handlers are removed for the object.
  * @param {Function} handler (Optional) The event handler function to remove. The handler
  * must be the same function object as was passed into <code>on()</code>. Be careful with
  * helpers like <code>bind()</code> that return a new function when called. If you pass in
  * no <code>handler</code>, all event handlers are removed for the specified event
  * <code>type</code>.
  * @param {Object} context (Optional) If you specify a <code>context</code>, the event handler
  * is removed for all specified events and handlers that use the specified context. (The
  * context must match the context passed into <code>on()</code>.)
  *
  * @returns {Object} The object that dispatched the event.
  *
  * @memberOf EventDispatcher
  * @method #off
  * @see <a href="#on">on()</a>
  * @see <a href="#once">once()</a>
  * @see <a href="#events">Events</a>
  */
  self.off = function(eventNames, handlerOrContext, context) {
    if (typeof eventNames === 'string') {

      if (handlerOrContext && $.isFunction(handlerOrContext)) {
        _.removeListeners(eventNames.split(' '), handlerOrContext, context);
      }
      else {
        _.removeAllListenersNamed(eventNames.split(' '));
      }

    } else if (!eventNames) {
      _.removeAllListeners();

    } else {
      for (var name in eventNames) {
        if (eventNames.hasOwnProperty(name)) {
          _.removeListeners([name], eventNames[name], context);
        }
      }
    }

    return this;
  };

 /**
  * Adds an event handler function for one or more events. Once the handler is called,
  * the specified handler method is removed as a handler for this event. (When you use
  * the <code>on()</code> method to add an event handler, the handler is <i>not</i>
  * removed when it is called.) The <code>once()</code> method is the equivilent of
  * calling the <code>on()</code>
  * method and calling <code>off()</code> the first time the handler is invoked.
  *
  * <p>
  * The following code adds a one-time event handler for the <code>accessAllowed</code> event:
  * </p>
  *
  * <pre>
  * obj.once("eventName", function (event) {
  *    // This is the event handler.
  * });
  * </pre>
  *
  * <p>If you pass in multiple event names and a handler method, the handler is registered
  * for each of those events:</p>
  *
  * <pre>obj.once("eventName1 eventName2"
  *          function (event) {
  *              // This is the event handler.
  *          });
  * </pre>
  *
  * <p>You can also pass in a third <code>context</code> parameter (which is optional) to define
  * the value of
  * <code>this</code> in the handler method:</p>
  *
  * <pre>obj.once("eventName",
  *          function (event) {
  *              // This is the event handler.
  *          },
  *          obj);
  * </pre>
  *
  * <p>
  * The method also supports an alternate syntax, in which the first parameter is an object that
  * is a hash map of event names and handler functions and the second parameter (optional) is the
  * context for this in each handler:
  * </p>
  * <pre>
  * obj.once(
  *    {
  *       eventName1: function (event) {
  *                  // This is the event handler for eventName1.
  *           },
  *       eventName2:  function (event) {
  *                  // This is the event handler for eventName1.
  *           }
  *    },
  *    obj);
  * </pre>
  *
  * @param {String} type The string identifying the type of event. You can specify multiple
  * event names in this string, separating them with a space. The event handler will process
  * the first occurence of the events. After the first event, the handler is removed (for
  * all specified events).
  * @param {Function} handler The handler function to process the event. This function takes
  * the event object as a parameter.
  * @param {Object} context (Optional) Defines the value of <code>this</code> in the event
  * handler function.
  *
  * @returns {Object} The object that dispatched the event.
  *
  * @memberOf EventDispatcher
  * @method #once
  * @see <a href="#on">on()</a>
  * @see <a href="#off">off()</a>
  * @see <a href="#events">Events</a>
  */

  self.once = function(eventNames, handler, context) {
    var handleThisOnce = function() {
      self.off(eventNames, handleThisOnce, context);
      handler.apply(context, arguments);
    };

    handleThisOnce.originalHandler = handler;

    self.on(eventNames, handleThisOnce, context);

    return this;
  };

  // Execute any listeners bound to the +event+ Event.
  //
  // Each handler will be executed async. On completion the defaultAction
  // handler will be executed with the args.
  //
  // @param [Event] event
  //    An Event object.
  //
  // @param [Function, Null, Undefined] defaultAction
  //    An optional function to execute after every other handler. This will execute even
  //    if there are listeners bound to this event. +defaultAction+ will be passed
  //    args as a normal handler would.
  //
  // @return this
  //
  self.dispatchEvent = function(event, defaultAction) {
    if (!event.type) {
      $.error('OTHelpers.Eventing.dispatchEvent: Event has no type');
      $.error(event);

      throw new Error('OTHelpers.Eventing.dispatchEvent: Event has no type');
    }

    if (!event.target) {
      event.target = this;
    }

    _.dispatchEvent(event, defaultAction);
    return this;
  };

  // Execute each handler for the event called +name+.
  //
  // Each handler will be executed async, and any exceptions that they throw will
  // be caught and logged
  //
  // How to pass these?
  //  * defaultAction
  //
  // @example
  //  foo.on('bar', function(name, message) {
  //    alert("Hello " + name + ": " + message);
  //  });
  //
  //  foo.trigger('OpenTok', 'asdf');     // -> Hello OpenTok: asdf
  //
  //
  // @param [String] eventName
  //    The name of this event.
  //
  // @param [Array] arguments
  //    Any additional arguments beyond +eventName+ will be passed to the handlers.
  //
  // @return this
  //
  self.trigger = function(/* eventName [, arg0, arg1, ..., argN ] */) {
    var args = prototypeSlice.call(arguments);

    // Shifting to remove the eventName from the other args
    _.trigger(args.shift(), args);

    return this;
  };

  // Alias of trigger for easier node compatibility
  self.emit = self.trigger;


  /**
  * Deprecated; use <a href="#on">on()</a> or <a href="#once">once()</a> instead.
  * <p>
  * This method registers a method as an event listener for a specific event.
  * <p>
  *
  * <p>
  *   If a handler is not registered for an event, the event is ignored locally. If the
  *   event listener function does not exist, the event is ignored locally.
  * </p>
  * <p>
  *   Throws an exception if the <code>listener</code> name is invalid.
  * </p>
  *
  * @param {String} type The string identifying the type of event.
  *
  * @param {Function} listener The function to be invoked when the object dispatches the event.
  *
  * @param {Object} context (Optional) Defines the value of <code>this</code> in the event
  * handler function.
  *
  * @memberOf EventDispatcher
  * @method #addEventListener
  * @see <a href="#on">on()</a>
  * @see <a href="#once">once()</a>
  * @see <a href="#events">Events</a>
  */
  // See 'on' for usage.
  // @depreciated will become a private helper function in the future.
  self.addEventListener = function(eventName, handler, context) {
    $.warn('The addEventListener() method is deprecated. Use on() or once() instead.');
    return self.on(eventName, handler, context);
  };


  /**
  * Deprecated; use <a href="#on">on()</a> or <a href="#once">once()</a> instead.
  * <p>
  * Removes an event listener for a specific event.
  * <p>
  *
  * <p>
  *   Throws an exception if the <code>listener</code> name is invalid.
  * </p>
  *
  * @param {String} type The string identifying the type of event.
  *
  * @param {Function} listener The event listener function to remove.
  *
  * @param {Object} context (Optional) If you specify a <code>context</code>, the event
  * handler is removed for all specified events and event listeners that use the specified
  context. (The context must match the context passed into
  * <code>addEventListener()</code>.)
  *
  * @memberOf EventDispatcher
  * @method #removeEventListener
  * @see <a href="#off">off()</a>
  * @see <a href="#events">Events</a>
  */
  // See 'off' for usage.
  // @depreciated will become a private helper function in the future.
  self.removeEventListener = function(eventName, handler, context) {
    $.warn('The removeEventListener() method is deprecated. Use off() instead.');
    return self.off(eventName, handler, context);
  };


  return self;
};

})(window, window.OTHelpers);


/**
 * @license  TB Plugin 0.4.0.12 6e40a4e v0.4.0.12-branch
 * http://www.tokbox.com/
 *
 * Copyright (c) 2015 TokBox, Inc.
 *
 * Date: October 28 03:45:04 2015
 *
 */

/* global scope:true */
/* exported OTPlugin */

/* jshint ignore:start */
(function(scope) {
/* jshint ignore:end */

// If we've already be setup, bail
if (scope.OTPlugin !== void 0) return;

var $ = OTHelpers;

// Magic number to avoid plugin crashes through a settimeout call
window.EmpiricDelay = 3000;

// TB must exist first, otherwise we can't do anything
// if (scope.OT === void 0) return;

// Establish the environment that we're running in
// Note: we don't currently support 64bit IE
var isSupported = ($.env.name === 'IE' && $.env.version >= 8 &&
                    $.env.userAgent.indexOf('x64') === -1),
    pluginIsReady = false;

var OTPlugin = {
  isSupported: function() { return isSupported; },
  isReady: function() { return pluginIsReady; },
  meta: {
    mimeType: 'application/x-opentokplugin,version=0.4.0.12',
    activeXName: 'TokBox.OpenTokPlugin.0.4.0.12',
    version: '0.4.0.12'
  },

  useLoggingFrom: function(host) {
    // TODO there's no way to revert this, should there be?
    OTPlugin.log = $.bind(host.log, host);
    OTPlugin.debug = $.bind(host.debug, host);
    OTPlugin.info = $.bind(host.info, host);
    OTPlugin.warn = $.bind(host.warn, host);
    OTPlugin.error = $.bind(host.error, host);
  }
};

// Add logging methods
$.useLogHelpers(OTPlugin);

scope.OTPlugin = OTPlugin;

$.registerCapability('otplugin', function() {
  return OTPlugin.isInstalled();
});

// If this client isn't supported we still make sure that OTPlugin is defined
// and the basic API (isSupported() and isInstalled()) is created.
if (!OTPlugin.isSupported()) {
  OTPlugin.isInstalled = function isInstalled() { return false; };
  return;
}

// tb_require('./header.js')

/* exported shim */

// Shims for various missing things from JS
// Applied only after init is called to prevent unnecessary polution
var shim = function shim() {
  if (!Function.prototype.bind) {
    Function.prototype.bind = function(oThis) {
      if (typeof this !== 'function') {
        // closest thing possible to the ECMAScript 5 internal IsCallable function
        throw new TypeError('Function.prototype.bind - what is trying to be bound is not callable');
      }

      var aArgs = Array.prototype.slice.call(arguments, 1),
          fToBind = this,
          FNOP = function() {},
          fBound = function() {
            return fToBind.apply(this instanceof FNOP && oThis ?
                          this : oThis, aArgs.concat(Array.prototype.slice.call(arguments)));
          };

      FNOP.prototype = this.prototype;
      fBound.prototype = new FNOP();

      return fBound;
    };
  }

  if (!Array.isArray) {
    Array.isArray = function(vArg) {
      return Object.prototype.toString.call(vArg) === '[object Array]';
    };
  }

  if (!Array.prototype.indexOf) {
    Array.prototype.indexOf = function(searchElement, fromIndex) {
      var i,
          pivot = (fromIndex) ? fromIndex : 0,
          length;

      if (!this) {
        throw new TypeError();
      }

      length = this.length;

      if (length === 0 || pivot >= length) {
        return -1;
      }

      if (pivot < 0) {
        pivot = length - Math.abs(pivot);
      }

      for (i = pivot; i < length; i++) {
        if (this[i] === searchElement) {
          return i;
        }
      }
      return -1;
    };
  }

  if (!Array.prototype.map)
  {
    Array.prototype.map = function(fun /*, thisArg */)
    {
      'use strict';

      if (this === void 0 || this === null)
        throw new TypeError();

      var t = Object(this);
      var len = t.length >>> 0;
      if (typeof fun !== 'function') {
        throw new TypeError();
      }

      var res = new Array(len);
      var thisArg = arguments.length >= 2 ? arguments[1] : void 0;
      for (var i = 0; i < len; i++)
      {
        // NOTE: Absolute correctness would demand Object.defineProperty
        //       be used.  But this method is fairly new, and failure is
        //       possible only if Object.prototype or Array.prototype
        //       has a property |i| (very unlikely), so use a less-correct
        //       but more portable alternative.
        if (i in t)
          res[i] = fun.call(thisArg, t[i], i, t);
      }

      return res;
    };
  }
};

// tb_require('./header.js')
// tb_require('./shims.js')

/* exported  TaskQueue */

var TaskQueue = function() {
  var Proto = function TaskQueue() {},
      api = new Proto(),
      tasks = [];

  api.add = function(fn, context) {
    if (context) {
      tasks.push($.bind(fn, context));
    } else {
      tasks.push(fn);
    }
  };

  api.runAll = function() {
    var task;
    while ((task = tasks.shift())) {
      task();
    }
  };

  return api;
};

// tb_require('./header.js')
// tb_require('./shims.js')

/* global curryCallAsync:true */
/* exported RumorSocket */

var RumorSocket = function(plugin, server) {
  var Proto = function RumorSocket() {},
      api = new Proto(),
      connected = false,
      rumorID,
      _onOpen,
      _onClose;

  try {
    rumorID = plugin._.RumorInit(server, '');
  } catch (e) {
    OTPlugin.error('Error creating the Rumor Socket: ', e.message);
  }

  if (!rumorID) {
    throw new Error('Could not initialise OTPlugin rumor connection');
  }

  api.open = function() {
    connected = true;
    plugin._.RumorOpen(rumorID);
  };

  api.close = function(code, reason) {
    if (connected) {
      connected = false;
      plugin._.RumorClose(rumorID, code, reason);
    }

    plugin.removeRef(api);
  };

  api.destroy = function() {
    this.close();
  };

  api.send = function(msg) {
    plugin._.RumorSend(rumorID, msg.type, msg.toAddress,
      JSON.parse(JSON.stringify(msg.headers)), msg.data);
  };

  api.onOpen = function(callback) {
    _onOpen = callback;
  };

  api.onClose = function(callback) {
    _onClose = callback;
  };

  api.onError = function(callback) {
    plugin._.SetOnRumorError(rumorID, curryCallAsync(callback));
  };

  api.onMessage = function(callback) {
    plugin._.SetOnRumorMessage(rumorID, curryCallAsync(callback));
  };

  plugin.addRef(api);

  plugin._.SetOnRumorOpen(rumorID, curryCallAsync(function() {
    if (_onOpen && $.isFunction(_onOpen)) {
      _onOpen.call(null);
    }
  }));

  plugin._.SetOnRumorClose(rumorID, curryCallAsync(function(code) {
    _onClose(code);

    // We're done. Clean up ourselves
    plugin.removeRef(api);
  }));

  return api;
};

// tb_require('./header.js')
// tb_require('./shims.js')

/* exported  refCountBehaviour */

var refCountBehaviour = function refCountBehaviour(api) {
  var _liveObjects = [];

  api.addRef = function(ref) {
    _liveObjects.push(ref);
    return api;
  };

  api.removeRef = function(ref) {
    if (_liveObjects.length === 0) return;

    var index = _liveObjects.indexOf(ref);
    if (index !== -1) {
      _liveObjects.splice(index, 1);
    }

    if (_liveObjects.length === 0) {
      api.destroy();
    }

    return api;
  };

  api.removeAllRefs = function() {
    while (_liveObjects.length) {
      _liveObjects.shift().destroy();
    }
  };
};

// tb_require('./header.js')
// tb_require('./shims.js')
// tb_require('./task_queue.js')

/* global curryCallAsync:true, TaskQueue:true */
/* exported  pluginEventingBehaviour */

var pluginEventingBehaviour = function pluginEventingBehaviour(api) {
  var eventHandlers = {},
      pendingTasks = new TaskQueue(),
      isReady = false;

  var onCustomEvent = function() {
    var args = Array.prototype.slice.call(arguments);
    api.emit(args.shift(), args);
  };

  var devicesChanged = function() {
    var args = Array.prototype.slice.call(arguments);
    OTPlugin.debug(args);
    api.emit('devicesChanged', args);
  };

  api.on = function(name, callback, context) {
    if (!eventHandlers.hasOwnProperty(name)) {
      eventHandlers[name] = [];
    }

    eventHandlers[name].push([callback, context]);
    return api;
  };

  api.off = function(name, callback, context) {
    if (!eventHandlers.hasOwnProperty(name) ||
        eventHandlers[name].length === 0) {
      return;
    }

    $.filter(eventHandlers[name], function(listener) {
      return listener[0] === callback &&
              listener[1] === context;
    });

    return api;
  };

  api.once = function(name, callback, context) {
    var fn = function() {
      api.off(name, fn);
      return callback.apply(context, arguments);
    };

    api.on(name, fn);
    return api;
  };

  api.emit = function(name, args) {
    $.callAsync(function() {
      if (!eventHandlers.hasOwnProperty(name) && eventHandlers[name].length) {
        return;
      }

      $.forEach(eventHandlers[name], function(handler) {
        handler[0].apply(handler[1], args);
      });
    });

    return api;
  };

  // Calling this will bind a listener to the devicesChanged events that
  // the plugin emits and then rebroadcast them.
  api.listenForDeviceChanges = function() {
    if (!isReady) {
      pendingTasks.add(api.listenForDeviceChanges, api);
      return;
    }

    setTimeout(function() {
      api._.registerXCallback('devicesChanged', devicesChanged);
    }, window.EmpiricDelay);
  };

  var onReady = function onReady(readyCallback) {
    if (api._.on) {
      // If the plugin supports custom events we'll use them
      api._.on(-1, {
        customEvent: curryCallAsync(onCustomEvent)
      });
    }

    var internalReadyCallback = function() {
      // It's not safe to do most plugin operations until the plugin
      // is ready for us to do so. We use isReady as a guard in
      isReady = true;

      pendingTasks.runAll();
      readyCallback.call(api);
    };

    // Only the main plugin has an initialise method
    if (api._.initialise) {
      api.on('ready', curryCallAsync(internalReadyCallback));
      api._.initialise();
    } else {
      internalReadyCallback.call(api);
    }
  };

  return function(completion) {
    onReady(function(err) {
      if (err) {
        OTPlugin.error('Error while starting up plugin ' + api.uuid + ': ' + err);
        completion(err);
        return;
      }

      OTPlugin.debug('Plugin ' + api.id + ' is loaded');
      completion(void 0, api);
    });
  };
};

// tb_require('./header.js')
// tb_require('./shims.js')
// tb_require('./ref_count_behaviour.js')
// tb_require('./plugin_eventing_behaviour.js')

/* global refCountBehaviour:true, pluginEventingBehaviour:true, scope:true */
/* exported createPluginProxy, curryCallAsync, makeMediaPeerProxy, makeMediaCapturerProxy */

var PROXY_LOAD_TIMEOUT = 5000;

var objectTimeouts = {};

var curryCallAsync = function curryCallAsync(fn) {
  return function() {
    var args = Array.prototype.slice.call(arguments);
    args.unshift(fn);
    $.callAsync.apply($, args);
  };
};

var clearGlobalCallback = function clearGlobalCallback(callbackId) {
  if (!callbackId) return;

  if (objectTimeouts[callbackId]) {
    clearTimeout(objectTimeouts[callbackId]);
    objectTimeouts[callbackId] = null;
  }

  if (scope[callbackId]) {
    try {
      delete scope[callbackId];
    } catch (err) {
      scope[callbackId] = void 0;
    }
  }
};

var waitOnGlobalCallback = function waitOnGlobalCallback(callbackId, completion) {
  objectTimeouts[callbackId] = setTimeout(function() {
    clearGlobalCallback(callbackId);
    completion('The object timed out while loading.');
  }, PROXY_LOAD_TIMEOUT);

  scope[callbackId] = function() {
    clearGlobalCallback(callbackId);

    var args = Array.prototype.slice.call(arguments);
    args.unshift(null);
    completion.apply(null, args);
  };
};

var generateCallbackID = function generateCallbackID() {
  return 'OTPlugin_loaded_' + $.uuid().replace(/\-+/g, '');
};

var generateObjectHtml = function generateObjectHtml(callbackId, options) {
  options = options || {};

  var objBits = [],
    attrs = [
      'type="' + options.mimeType + '"',
      'id="' + callbackId + '_obj"',
      'tb_callback_id="' + callbackId + '"',
      'width="0" height="0"'
    ],
    params = {
      userAgent: $.env.userAgent.toLowerCase(),
      windowless: options.windowless,
      onload: callbackId
    };

  if (options.isVisible !== true) {
    attrs.push('visibility="hidden"');
  }

  objBits.push('<object ' + attrs.join(' ') + '>');

  for (var name in params) {
    if (params.hasOwnProperty(name)) {
      objBits.push('<param name="' + name + '" value="' + params[name] + '" />');
    }
  }

  objBits.push('</object>');
  return objBits.join('');
};

var createObject = function createObject(callbackId, options, completion) {
  options = options || {};

  var html = generateObjectHtml(callbackId, options),
      doc = options.doc || scope.document;

  // if (options.robust !== false) {
  //   new createFrame(html, callbackId, scope[callbackId], function(frame, win, doc) {
  //     var object = doc.getElementById(callbackId+'_obj');
  //     object.removeAttribute('id');
  //     completion(void 0, object, frame);
  //   });
  // }
  // else {

  doc.body.insertAdjacentHTML('beforeend', html);
  var object = doc.body.querySelector('#' + callbackId + '_obj');

  // object.setAttribute('type', options.mimeType);

  completion(void 0, object);
  // }
};

// Reference counted wrapper for a plugin object
var createPluginProxy = function(options, completion) {
  var Proto = function PluginProxy() {},
      api = new Proto(),
      waitForReadySignal = pluginEventingBehaviour(api);

  refCountBehaviour(api);

  // Assign +plugin+ to this object and setup all the public
  // accessors that relate to the DOM Object.
  //
  var setPlugin = function setPlugin(plugin) {
        if (plugin) {
          api._ = plugin;
          api.parentElement = plugin.parentElement;
          api.$ = $(plugin);
        } else {
          api._ = null;
          api.parentElement = null;
          api.$ = $();
        }
      };

  api.uuid = generateCallbackID();

  api.isValid = function() {
    return api._.valid;
  };

  api.destroy = function() {
    api.removeAllRefs();
    setPlugin(null);

    // Let listeners know that they should do any final book keeping
    // that relates to us.
    api.emit('destroy');
  };

  api.enumerateDevices = function(completion) {
    api._.enumerateDevices(completion);
  };

  /// Initialise

  // The next statement creates the raw plugin object accessor on the Proxy.
  // This is null until we actually have created the Object.
  setPlugin(null);

  waitOnGlobalCallback(api.uuid, function(err) {
    if (err) {
      completion('The plugin with the mimeType of ' +
                      options.mimeType + ' timed out while loading: ' + err);

      api.destroy();
      return;
    }

    api._.setAttribute('id', 'tb_plugin_' + api._.uuid);
    api._.removeAttribute('tb_callback_id');
    api.uuid = api._.uuid;
    api.id = api._.id;

    waitForReadySignal(function(err) {
      if (err) {
        completion('Error while starting up plugin ' + api.uuid + ': ' + err);
        api.destroy();
        return;
      }

      completion(void 0, api);
    });
  });

  createObject(api.uuid, options, function(err, plugin) {
    setPlugin(plugin);
  });

  return api;
};

// Specialisation for the MediaCapturer API surface
var makeMediaCapturerProxy = function makeMediaCapturerProxy(api) {
  api.selectSources = function() {
    return api._.selectSources.apply(api._, arguments);
  };

  api.listenForDeviceChanges();
  return api;
};

// Specialisation for the MediaPeer API surface
var makeMediaPeerProxy = function makeMediaPeerProxy(api) {
  api.setStream = function(stream, completion) {
    api._.setStream(stream);

    if (completion) {
      if (stream.hasVideo()) {
        // FIX ME renderingStarted currently doesn't first
        // api.once('renderingStarted', completion);
        var verifyStream = function() {
          if (!api._) {
            completion(new $.Error('The plugin went away before the stream could be bound.'));
            return;
          }

          if (api._.videoWidth > 0) {
            // This fires a little too soon.
            setTimeout(completion, 200);
          } else {
            setTimeout(verifyStream, 500);
          }
        };

        setTimeout(verifyStream, 500);
      } else {
        // TODO Investigate whether there is a good way to detect
        // when the audio is ready. Does it even matter?

        // This fires a little too soon.
        setTimeout(completion, 200);
      }
    }

    return api;
  };

  return api;
};

// tb_require('./header.js')
// tb_require('./shims.js')
// tb_require('./proxy.js')

/* exported VideoContainer */

var VideoContainer = function(plugin, stream) {
  var Proto = function VideoContainer() {},
      api = new Proto();

  api.domElement = plugin._;
  api.$ = $(plugin._);
  api.parentElement = plugin._.parentNode;

  plugin.addRef(api);

  api.appendTo = function(parentDomElement) {
    if (parentDomElement && plugin._.parentNode !== parentDomElement) {
      OTPlugin.debug('VideoContainer appendTo', parentDomElement);
      parentDomElement.appendChild(plugin._);
      api.parentElement = parentDomElement;
    }
  };

  api.show = function(completion) {
    OTPlugin.debug('VideoContainer show');
    plugin._.removeAttribute('width');
    plugin._.removeAttribute('height');
    plugin.setStream(stream, completion);
    $.show(plugin._);
    return api;
  };

  api.setSize = function(width, height) {
    plugin._.setAttribute('width', width);
    plugin._.setAttribute('height', height);
    return api;
  };

  api.width = function(newWidth) {
    if (newWidth !== void 0) {
      OTPlugin.debug('VideoContainer set width to ' + newWidth);
      plugin._.setAttribute('width', newWidth);
    }

    return plugin._.getAttribute('width');
  };

  api.height = function(newHeight) {
    if (newHeight !== void 0) {
      OTPlugin.debug('VideoContainer set height to ' + newHeight);
      plugin._.setAttribute('height', newHeight);
    }

    return plugin._.getAttribute('height');
  };

  api.volume = function(newVolume) {
    if (newVolume !== void 0) {
      // TODO
      OTPlugin.debug('VideoContainer setVolume not implemented: called with ' + newVolume);
    } else {
      OTPlugin.debug('VideoContainer getVolume not implemented');
    }

    return 0.5;
  };

  api.getImgData = function() {
    return plugin._.getImgData('image/png');
  };

  api.videoWidth = function() {
    return plugin._.videoWidth;
  };

  api.videoHeight = function() {
    return plugin._.videoHeight;
  };

  api.destroy = function() {
    plugin._.setStream(null);
    plugin.removeRef(api);
  };

  return api;
};

// tb_require('./header.js')
// tb_require('./shims.js')
// tb_require('./proxy.js')

/* exported RTCStatsReport */

var RTCStatsReport = function RTCStatsReport(reports) {
  for (var id in reports) {
    if (reports.hasOwnProperty(id)) {
      this[id] = reports[id];
    }
  }
};

RTCStatsReport.prototype.forEach = function(callback, context) {
  for (var id in this) {
    if (this.hasOwnProperty(id)) {
      callback.call(context, this[id]);
    }
  }
};

// tb_require('./header.js')
// tb_require('./shims.js')
// tb_require('./proxy.js')

/* global createPluginProxy:true, makeMediaPeerProxy:true, makeMediaCapturerProxy:true */
/* exported PluginProxies  */

var PluginProxies = (function() {
  var Proto = function PluginProxies() {},
      api = new Proto(),
      proxies = {};

  /// Private API

  // This is called whenever a Proxy's destroy event fires.
  var cleanupProxyOnDestroy = function cleanupProxyOnDestroy(object) {
    if (api.mediaCapturer && api.mediaCapturer.id === object.id) {
      api.mediaCapturer = null;
    } else if (proxies.hasOwnProperty(object.id)) {
      delete proxies[object.id];
    }

    if (object.$) {
      object.$.remove();
    }
  };

  /// Public API

  // Public accessor for the MediaCapturer
  api.mediaCapturer = null;

  api.removeAll = function removeAll() {
    for (var id in proxies) {
      if (proxies.hasOwnProperty(id)) {
        proxies[id].destroy();
      }
    }

    if (api.mediaCapturer) api.mediaCapturer.destroy();
  };

  api.create = function create(options, completion) {
    var proxy = createPluginProxy(options, completion);

    proxies[proxy.uuid] = proxy;

    // Clean up after this Proxy when it's destroyed.
    proxy.on('destroy', function() {
      cleanupProxyOnDestroy(proxy);
    });

    return proxy;
  };

  api.createMediaPeer = function createMediaPeer(options, completion) {
    if ($.isFunction(options)) {
      completion = options;
      options = {};
    }

    var mediaPeer =  api.create($.extend(options || {}, {
      mimeType: OTPlugin.meta.mimeType,
      isVisible: true,
      windowless: true
    }), function(err) {
      if (err) {
        completion.call(OTPlugin, err);
        return;
      }

      proxies[mediaPeer.id] = mediaPeer;
      completion.call(OTPlugin, void 0, mediaPeer);
    });

    makeMediaPeerProxy(mediaPeer);
  };

  api.createMediaCapturer = function createMediaCapturer(completion) {
    if (api.mediaCapturer) {
      completion.call(OTPlugin, void 0, api.mediaCapturer);
      return api;
    }

    api.mediaCapturer = api.create({
      mimeType: OTPlugin.meta.mimeType,
      isVisible: false,
      windowless: false
    }, function(err) {
      completion.call(OTPlugin, err, api.mediaCapturer);
    });

    makeMediaCapturerProxy(api.mediaCapturer);
  };

  return api;
})();

// tb_require('./header.js')
// tb_require('./shims.js')
// tb_require('./proxy.js')
// tb_require('./stats.js')

/* global MediaStream:true, RTCStatsReport:true, curryCallAsync:true */
/* exported PeerConnection */

// Our RTCPeerConnection shim, it should look like a normal PeerConection
// from the outside, but it actually delegates to our plugin.
//
var PeerConnection = function(iceServers, options, plugin, ready) {
  var Proto = function PeerConnection() {},
      api = new Proto(),
      id = $.uuid(),
      hasLocalDescription = false,
      hasRemoteDescription = false,
      candidates = [],
      inited = false,
      deferMethods = [],
      events;

  plugin.addRef(api);

  events = {
    addstream: [],
    removestream: [],
    icecandidate: [],
    signalingstatechange: [],
    iceconnectionstatechange: []
  };

  var onAddIceCandidate = function onAddIceCandidate() {/* success */},

      onAddIceCandidateFailed = function onAddIceCandidateFailed(err) {
        OTPlugin.error('Failed to process candidate');
        OTPlugin.error(err);
      },

      processPendingCandidates = function processPendingCandidates() {
        for (var i = 0; i < candidates.length; ++i) {
          plugin._.addIceCandidate(id, candidates[i], onAddIceCandidate, onAddIceCandidateFailed);
        }
      },

      deferMethod = function deferMethod(method) {
        return function() {
          if (inited === true) {
            return method.apply(api, arguments);
          }

          deferMethods.push([method, arguments]);
        };
      },

      processDeferredMethods = function processDeferredMethods() {
        var m;
        while ((m = deferMethods.shift())) {
          m[0].apply(api, m[1]);
        }
      },

      triggerEvent = function triggerEvent(/* eventName [, arg1, arg2, ..., argN] */) {
        var args = Array.prototype.slice.call(arguments),
            eventName = args.shift();

        if (!events.hasOwnProperty(eventName)) {
          OTPlugin.error('PeerConnection does not have an event called: ' + eventName);
          return;
        }

        $.forEach(events[eventName], function(listener) {
          listener.apply(null, args);
        });
      },

      bindAndDelegateEvents = function bindAndDelegateEvents(events) {
        for (var name in events) {
          if (events.hasOwnProperty(name)) {
            events[name] = curryCallAsync(events[name]);
          }
        }

        plugin._.on(id, events);
      },

      addStream = function addStream(streamJson) {
        setTimeout(function() {
          var stream = MediaStream.fromJson(streamJson, plugin),
              event = {stream: stream, target: api};

          if (api.onaddstream && $.isFunction(api.onaddstream)) {
            $.callAsync(api.onaddstream, event);
          }

          triggerEvent('addstream', event);
        }, window.EmpiricDelay);
      },

      removeStream = function removeStream(streamJson) {
        var stream = MediaStream.fromJson(streamJson, plugin),
            event = {stream: stream, target: api};

        if (api.onremovestream && $.isFunction(api.onremovestream)) {
          $.callAsync(api.onremovestream, event);
        }

        triggerEvent('removestream', event);
      },

      iceCandidate = function iceCandidate(candidateSdp, sdpMid, sdpMLineIndex) {
        var candidate = new OTPlugin.RTCIceCandidate({
          candidate: candidateSdp,
          sdpMid: sdpMid,
          sdpMLineIndex: sdpMLineIndex
        });

        var event = {candidate: candidate, target: api};

        if (api.onicecandidate && $.isFunction(api.onicecandidate)) {
          $.callAsync(api.onicecandidate, event);
        }

        triggerEvent('icecandidate', event);
      },

      signalingStateChange = function signalingStateChange(state) {
        api.signalingState = state;
        var event = {state: state, target: api};

        if (api.onsignalingstatechange &&
                $.isFunction(api.onsignalingstatechange)) {
          $.callAsync(api.onsignalingstatechange, event);
        }

        triggerEvent('signalingstate', event);
      },

      iceConnectionChange = function iceConnectionChange(state) {
        api.iceConnectionState = state;
        var event = {state: state, target: api};

        if (api.oniceconnectionstatechange &&
                $.isFunction(api.oniceconnectionstatechange)) {
          $.callAsync(api.oniceconnectionstatechange, event);
        }

        triggerEvent('iceconnectionstatechange', event);
      };

  api.createOffer = deferMethod(function(success, error, constraints) {
    OTPlugin.debug('createOffer', constraints);
    plugin._.createOffer(id, function(type, sdp) {
      success(new OTPlugin.RTCSessionDescription({
        type: type,
        sdp: sdp
      }));
    }, error, constraints || {});
  });

  api.createAnswer = deferMethod(function(success, error, constraints) {
    OTPlugin.debug('createAnswer', constraints);
    plugin._.createAnswer(id, function(type, sdp) {
      success(new OTPlugin.RTCSessionDescription({
        type: type,
        sdp: sdp
      }));
    }, error, constraints || {});
  });

  api.setLocalDescription = deferMethod(function(description, success, error) {
    OTPlugin.debug('setLocalDescription');

    plugin._.setLocalDescription(id, description, function() {
      hasLocalDescription = true;

      if (hasRemoteDescription) processPendingCandidates();
      if (success) success.call(null);
    }, error);
  });

  api.setRemoteDescription = deferMethod(function(description, success, error) {
    OTPlugin.debug('setRemoteDescription');

    plugin._.setRemoteDescription(id, description, function() {
      hasRemoteDescription = true;

      if (hasLocalDescription) processPendingCandidates();
      if (success) success.call(null);
    }, error);
  });

  api.addIceCandidate = deferMethod(function(candidate) {
    OTPlugin.debug('addIceCandidate');

    if (hasLocalDescription && hasRemoteDescription) {
      plugin._.addIceCandidate(id, candidate, onAddIceCandidate, onAddIceCandidateFailed);
    } else {
      candidates.push(candidate);
    }
  });

  api.addStream = deferMethod(function(stream) {
    var constraints = {};
    plugin._.addStream(id, stream, constraints);
  });

  api.removeStream = deferMethod(function(stream) {
    plugin._.removeStream(id, stream);
  });

  api.getRemoteStreams = function() {
    return $.map(plugin._.getRemoteStreams(id), function(stream) {
      return MediaStream.fromJson(stream, plugin);
    });
  };

  api.getLocalStreams = function() {
    return $.map(plugin._.getLocalStreams(id), function(stream) {
      return MediaStream.fromJson(stream, plugin);
    });
  };

  api.getStreamById = function(streamId) {
    return MediaStream.fromJson(plugin._.getStreamById(id, streamId), plugin);
  };

  api.getStats = deferMethod(function(mediaStreamTrack, success, error) {
    plugin._.getStats(id, mediaStreamTrack || null, curryCallAsync(function(statsReportJson) {
      var report = new RTCStatsReport(JSON.parse(statsReportJson));
      success(report);
    }), error);
  });

  api.close = function() {
    plugin._.destroyPeerConnection(id);
    plugin.removeRef(this);
  };

  api.destroy = function() {
    api.close();
  };

  api.addEventListener = function(event, handler /* [, useCapture] we ignore this */) {
    if (events[event] === void 0) {
      return;
    }

    events[event].push(handler);
  };

  api.removeEventListener = function(event, handler /* [, useCapture] we ignore this */) {
    if (events[event] === void 0) {
      return;
    }

    events[event] = $.filter(events[event], handler);
  };

  // These should appear to be null, instead of undefined, if no
  // callbacks are assigned. This more closely matches how the native
  // objects appear and allows 'if (pc.onsignalingstatechange)' type
  // feature detection to work.
  api.onaddstream = null;
  api.onremovestream = null;
  api.onicecandidate = null;
  api.onsignalingstatechange = null;
  api.oniceconnectionstatechange = null;

  // Both username and credential must exist, otherwise the plugin throws an error
  $.forEach(iceServers.iceServers, function(iceServer) {
    if (!iceServer.username) iceServer.username = '';
    if (!iceServer.credential) iceServer.credential = '';
  });

  if (!plugin._.initPeerConnection(id, iceServers, options)) {
    ready(new $.error('Failed to initialise PeerConnection'));
    return;
  }

  // This will make sense once init becomes async
  bindAndDelegateEvents({
    addStream: addStream,
    removeStream: removeStream,
    iceCandidate: iceCandidate,
    signalingStateChange: signalingStateChange,
    iceConnectionChange: iceConnectionChange
  });

  inited = true;
  processDeferredMethods();
  ready(void 0, api);

  return api;
};

PeerConnection.create = function(iceServers, options, plugin, ready) {
  new PeerConnection(iceServers, options, plugin, ready);
};

// tb_require('./header.js')
// tb_require('./shims.js')
// tb_require('./proxy.js')
// tb_require('./video_container.js')

/* global VideoContainer:true */
/* exported MediaStream */

var MediaStreamTrack = function(mediaStreamId, options, plugin) {
  var Proto = function MediaStreamTrack() {},
      api = new Proto();

  api.id = options.id;
  api.kind = options.kind;
  api.label = options.label;
  api.enabled = $.castToBoolean(options.enabled);
  api.streamId = mediaStreamId;

  api.setEnabled = function(enabled) {
    api.enabled = $.castToBoolean(enabled);

    if (api.enabled) {
      plugin._.enableMediaStreamTrack(mediaStreamId, api.id);
    } else {
      plugin._.disableMediaStreamTrack(mediaStreamId, api.id);
    }
  };

  return api;
};

var MediaStream = function(options, plugin) {
  var Proto = function MediaStream() {},
      api = new Proto(),
      audioTracks = [],
      videoTracks = [];

  api.id = options.id;
  plugin.addRef(api);

  // TODO
  // api.ended =
  // api.onended =

  if (options.videoTracks) {
    options.videoTracks.map(function(track) {
      videoTracks.push(new MediaStreamTrack(options.id, track, plugin));
    });
  }

  if (options.audioTracks) {
    options.audioTracks.map(function(track) {
      audioTracks.push(new MediaStreamTrack(options.id, track, plugin));
    });
  }

  var hasTracksOfType = function(type) {
    var tracks = type === 'video' ? videoTracks : audioTracks;

    return $.some(tracks, function(track) {
      return track.enabled;
    });
  };

  api.getVideoTracks = function() { return videoTracks; };
  api.getAudioTracks = function() { return audioTracks; };

  api.getTrackById = function(id) {
    videoTracks.concat(audioTracks).forEach(function(track) {
      if (track.id === id) return track;
    });

    return null;
  };

  api.hasVideo = function() {
    return hasTracksOfType('video');
  };

  api.hasAudio = function() {
    return hasTracksOfType('audio');
  };

  api.addTrack = function(/* MediaStreamTrack */) {
    // TODO
  };

  api.removeTrack = function(/* MediaStreamTrack */) {
    // TODO
  };

  api.stop = function() {
    plugin._.stopMediaStream(api.id);
    plugin.removeRef(api);
  };

  api.destroy = function() {
    api.stop();
  };

  // Private MediaStream API
  api._ = {
    plugin: plugin,

    // Get a VideoContainer to render the stream in.
    render: function() {
      return new VideoContainer(plugin, api);
    }
  };

  return api;
};

MediaStream.fromJson = function(json, plugin) {
  if (!json) return null;
  return new MediaStream(JSON.parse(json), plugin);
};

// tb_require('./header.js')
// tb_require('./shims.js')
// tb_require('./proxy.js')
// tb_require('./video_container.js')

/* exported MediaConstraints */

var MediaConstraints = function(userConstraints) {
  var constraints = $.clone(userConstraints);

  this.hasVideo = constraints.video !== void 0 && constraints.video !== false;
  this.hasAudio = constraints.audio !== void 0 && constraints.audio !== false;

  if (constraints.video === true) constraints.video = {};
  if (constraints.audio === true)  constraints.audio = {};

  if (this.hasVideo && !constraints.video.mandatory) {
    constraints.video.mandatory = {};
  }

  if (this.hasAudio && !constraints.audio.mandatory) {
    constraints.audio.mandatory = {};
  }

  this.screenSharing = this.hasVideo &&
                (constraints.video.mandatory.chromeMediaSource === 'screen' ||
                 constraints.video.mandatory.chromeMediaSource === 'window');

  this.audio = constraints.audio;
  this.video = constraints.video;

  this.setVideoSource = function(sourceId) {
    if (sourceId !== void 0) constraints.video.mandatory.sourceId =  sourceId;
    else delete constraints.video;
  };

  this.setAudioSource = function(sourceId) {
    if (sourceId !== void 0) constraints.audio.mandatory.sourceId =  sourceId;
    else delete constraints.audio;
  };

  this.toHash = function() {
    return constraints;
  };
};

// tb_require('./header.js')
// tb_require('./shims.js')

/* global scope:true */
/* exported  createFrame */

var createFrame = function createFrame(bodyContent, callbackId, callback) {
  var Proto = function Frame() {},
      api = new Proto(),
      domElement = scope.document.createElement('iframe');

  domElement.id = 'OTPlugin_frame_' + $.uuid().replace(/\-+/g, '');
  domElement.style.border = '0';

  try {
    domElement.style.backgroundColor = 'rgba(0,0,0,0)';
  } catch (err) {
    // Old IE browsers don't support rgba
    domElement.style.backgroundColor = 'transparent';
    domElement.setAttribute('allowTransparency', 'true');
  }

  domElement.scrolling = 'no';
  domElement.setAttribute('scrolling', 'no');

  // This is necessary for IE, as it will not inherit it's doctype from
  // the parent frame.
  var frameContent = '<!DOCTYPE html><html><head>' +
                    '<meta http-equiv="x-ua-compatible" content="IE=Edge">' +
                    '<meta http-equiv="Content-type" content="text/html; charset=utf-8">' +
                    '<title></title></head><body>' +
                    bodyContent +
                    '<script>window.parent["' + callbackId + '"](' +
                      'document.querySelector("object")' +
                    ');</script></body></html>';

  var wrappedCallback = function() {
    OTPlugin.log('LOADED IFRAME');
    var doc = domElement.contentDocument || domElement.contentWindow.document;

    if ($.env.iframeNeedsLoad) {
      doc.body.style.backgroundColor = 'transparent';
      doc.body.style.border = 'none';

      if ($.env.name !== 'IE') {
        // Skip this for IE as we use the bookmarklet workaround
        // for THAT browser.
        doc.open();
        doc.write(frameContent);
        doc.close();
      }
    }

    if (callback) {
      callback(
        api,
        domElement.contentWindow,
        doc
      );
    }
  };

  scope.document.body.appendChild(domElement);

  if ($.env.iframeNeedsLoad) {
    if ($.env.name === 'IE') {
      // This works around some issues with IE and document.write.
      // Basically this works by slightly abusing the bookmarklet/scriptlet
      // functionality that all browsers support.
      domElement.contentWindow.contents = frameContent;
      /*jshint scripturl:true*/
      domElement.src = 'javascript:window["contents"]';
      /*jshint scripturl:false*/
    }

    $.on(domElement, 'load', wrappedCallback);
  } else {
    setTimeout(wrappedCallback, 0);
  }

  api.reparent = function reparent(target) {
    // document.adoptNode(domElement);
    target.appendChild(domElement);
  };

  api.element = domElement;

  return api;
};

// tb_require('./header.js')
// tb_require('./shims.js')
// tb_require('./plugin_proxies.js')

/* global OT:true, OTPlugin:true, ActiveXObject:true,
          PluginProxies:true, curryCallAsync:true */

/* exported AutoUpdater:true */
var AutoUpdater;

(function() {

  var autoUpdaterController,
      updaterMimeType,        // <- cached version, use getInstallerMimeType instead
      installedVersion = -1;  // <- cached version, use getInstallerMimeType instead

  var versionGreaterThan = function versionGreaterThan(version1, version2) {
    if (version1 === version2) return false;
    if (version1 === -1) return version2;
    if (version2 === -1) return version1;

    if (version1.indexOf('.') === -1 && version2.indexOf('.') === -1) {
      return version1 > version2;
    }

    // The versions have multiple components (i.e. 0.10.30) and
    // must be compared piecewise.
    // Note: I'm ignoring the case where one version has multiple
    // components and the other doesn't.
    var v1 = version1.split('.'),
        v2 = version2.split('.'),
        versionLength = (v1.length > v2.length ? v2 : v1).length;

    for (var i = 0; i < versionLength; ++i) {
      if (parseInt(v1[i], 10) > parseInt(v2[i], 10)) {
        return true;
      }
    }

    // Special case, v1 has extra components but the initial components
    // were identical, we assume this means newer but it might also mean
    // that someone changed versioning systems.
    if (i < v1.length) {
      return true;
    }

    return false;
  };

  // Work out the full mimeType (including the currently installed version)
  // of the installer.
  var findMimeTypeAndVersion = function findMimeTypeAndVersion() {

    if (updaterMimeType !== void 0) {
      return updaterMimeType;
    }

    var activeXControlId = 'TokBox.OpenTokPluginInstaller',
        installPluginName = 'OpenTokPluginInstaller',
        unversionedMimeType = 'application/x-opentokplugininstaller',
        plugin = navigator.plugins[activeXControlId] || navigator.plugins[installPluginName];

    installedVersion = -1;

    if (plugin) {
      // Look through the supported mime-types for the version
      // There should only be one mime-type in our use case, and
      // if there's more than one they should all have the same
      // version.
      var numMimeTypes = plugin.length,
          extractVersion = new RegExp(unversionedMimeType.replace('-', '\\-') +
                                                      ',version=([0-9a-zA-Z-_.]+)', 'i'),
          mimeType,
          bits;

      for (var i = 0; i < numMimeTypes; ++i) {
        mimeType = plugin[i];

        // Look through the supported mimeTypes and find
        // the newest one.
        if (mimeType && mimeType.enabledPlugin &&
            (mimeType.enabledPlugin.name === plugin.name) &&
            mimeType.type.indexOf(unversionedMimeType) !== -1) {

          bits = extractVersion.exec(mimeType.type);

          if (bits !== null && versionGreaterThan(bits[1], installedVersion)) {
            installedVersion = bits[1];
          }
        }
      }
    } else if ($.env.name === 'IE') {
      // This may mean that the installer plugin is not installed.
      // Although it could also mean that we're on IE 9 and below,
      // which does not support navigator.plugins. Fallback to
      // using 'ActiveXObject' instead.
      try {
        plugin = new ActiveXObject(activeXControlId);
        installedVersion = plugin.getMasterVersion();
      } catch (e) {}
    }

    updaterMimeType = installedVersion !== -1 ?
                              unversionedMimeType + ',version=' + installedVersion :
                              null;
  };

  var getInstallerMimeType = function getInstallerMimeType() {
    if (updaterMimeType === void 0) {
      findMimeTypeAndVersion();
    }

    return updaterMimeType;
  };

  var getInstalledVersion = function getInstalledVersion() {
    if (installedVersion === void 0) {
      findMimeTypeAndVersion();
    }

    return installedVersion;
  };

  // Version 0.4.0.4 autoupdate was broken. We want to prompt
  // for install on 0.4.0.4 or earlier. We're also including
  // earlier versions just in case. Version 0.4.0.10 also
  // had a broken updater, we'll treat that version the same
  // way.
  var hasBrokenUpdater = function() {
    var _broken = getInstalledVersion() === '0.4.0.9' ||
                  !versionGreaterThan(getInstalledVersion(), '0.4.0.4');

    hasBrokenUpdater = function() { return _broken; };
    return _broken;
  };

  AutoUpdater = function() {
    var plugin;

    var getControllerCurry = function getControllerFirstCurry(fn) {
      return function() {
        if (plugin) {
          return fn(void 0, arguments);
        }

        PluginProxies.create({
          mimeType: getInstallerMimeType(),
          isVisible: false,
          windowless: false
        }, function(err, p) {
          plugin = p;

          if (err) {
            OTPlugin.error('Error while loading the AutoUpdater: ' + err);
            return;
          }

          return fn.apply(void 0, arguments);
        });
      };
    };

    // Returns true if the version of the plugin installed on this computer
    // does not match the one expected by this version of OTPlugin.
    this.isOutOfDate = function() {
      return versionGreaterThan(OTPlugin.meta.version, getInstalledVersion());
    };

    this.autoUpdate = getControllerCurry(function() {
      var modal = OT.Dialogs.Plugin.updateInProgress(),
          analytics = new OT.Analytics(),
        payload = {
          ieVersion: $.env.version,
          pluginOldVersion: OTPlugin.installedVersion(),
          pluginNewVersion: OTPlugin.version()
        };

      var success = curryCallAsync(function() {
            analytics.logEvent({
              action: 'OTPluginAutoUpdate',
              variation: 'Success',
              partnerId: OT.APIKEY,
              payload: JSON.stringify(payload)
            });

            plugin.destroy();

            modal.close();
            OT.Dialogs.Plugin.updateComplete().on({
              reload: function() {
                window.location.reload();
              }
            });
          }),

          error = curryCallAsync(function(errorCode, errorMessage, systemErrorCode) {
            payload.errorCode = errorCode;
            payload.systemErrorCode = systemErrorCode;

            analytics.logEvent({
              action: 'OTPluginAutoUpdate',
              variation: 'Failure',
              partnerId: OT.APIKEY,
              payload: JSON.stringify(payload)
            });

            plugin.destroy();

            modal.close();
            var updateMessage = errorMessage + ' (' + errorCode +
                                      '). Please restart your browser and try again.';

            modal = OT.Dialogs.Plugin.updateComplete(updateMessage).on({
              reload: function() {
                modal.close();
              }
            });

            OTPlugin.error('autoUpdate failed: ' + errorMessage + ' (' + errorCode +
                                      '). Please restart your browser and try again.');
            // TODO log client event
          }),

          progress = curryCallAsync(function(progress) {
            modal.setUpdateProgress(progress.toFixed());
            // modalBody.innerHTML = 'Updating...' + progress.toFixed() + '%';
          });

      plugin._.updatePlugin(OTPlugin.pathToInstaller(), success, error, progress);
    });

    this.destroy = function() {
      if (plugin) plugin.destroy();
    };

    // Refresh the plugin list so that we'll hopefully detect newer versions
    if (navigator.plugins) {
      navigator.plugins.refresh(false);
    }
  };

  AutoUpdater.get = function(completion) {
    if (!autoUpdaterController) {
      if (!this.isinstalled()) {
        completion.call(null, 'Plugin was not installed');
        return;
      }

      autoUpdaterController = new AutoUpdater();
    }

    completion.call(null, void 0, autoUpdaterController);
  };

  AutoUpdater.isinstalled = function() {
    return getInstallerMimeType() !== null && !hasBrokenUpdater();
  };

  AutoUpdater.installedVersion = function() {
    return getInstalledVersion();
  };

})();

// tb_require('./header.js')
// tb_require('./shims.js')
// tb_require('./proxy.js')
// tb_require('./auto_updater.js')

/* global PluginProxies */
/* exported MediaDevices  */

// Exposes a enumerateDevices method and emits a devicechange event
//
// http://w3c.github.io/mediacapture-main/#idl-def-MediaDevices
//
var MediaDevices = function() {
  var Proto = function MediaDevices() {},
      api = new Proto();

  api.enumerateDevices = function enumerateDevices(completion) {
    OTPlugin.ready(function(error) {
      if (error) {
        completion(error);
      } else {
        PluginProxies.mediaCapturer.enumerateDevices(completion);
      }
    });
  };

  api.addListener = function addListener(fn, context) {
    OTPlugin.ready(function(error) {
      if (error) {
        // No error message here, ready failing would have
        // created a bunch elsewhere
        return;
      }

      PluginProxies.mediaCapturer.on('devicesChanged', fn, context);
    });
  };

  api.removeListener = function removeListener(fn, context) {
    if (OTPlugin.isReady()) {
      PluginProxies.mediaCapturer.off('devicesChanged', fn, context);
    }
  };

  return api;
};

// tb_require('./header.js')
// tb_require('./shims.js')
// tb_require('./proxy.js')
// tb_require('./auto_updater.js')
// tb_require('./media_constraints.js')
// tb_require('./peer_connection.js')
// tb_require('./media_stream.js')
// tb_require('./video_container.js')
// tb_require('./rumor.js')

/* global scope, shim, pluginIsReady:true, PluginProxies, AutoUpdater */
/* export registerReadyListener, notifyReadyListeners, onDomReady */

var readyCallbacks = [],

    // This stores the error from the load attempt. We use
    // this if registerReadyListener gets called after a load
    // attempt fails.
    loadError;

var // jshint -W098
    destroy = function destroy() {
      PluginProxies.removeAll();
    },

    registerReadyListener = function registerReadyListener(callback) {
      if (!$.isFunction(callback)) {
        OTPlugin.warn('registerReadyListener was called with something that was not a function.');
        return;
      }

      if (OTPlugin.isReady()) {
        callback.call(OTPlugin, loadError);
      } else {
        readyCallbacks.push(callback);
      }
    },

    notifyReadyListeners = function notifyReadyListeners() {
      var callback;

      while ((callback = readyCallbacks.pop()) && $.isFunction(callback)) {
        callback.call(OTPlugin, loadError);
      }
    },

    onDomReady = function onDomReady() {
      AutoUpdater.get(function(err, updater) {
        if (err) {
          loadError = 'Error while loading the AutoUpdater: ' + err;
          notifyReadyListeners();
          return;
        }

        // If the plugin is out of date then we kick off the
        // auto update process and then bail out.
        if (updater.isOutOfDate()) {
          updater.autoUpdate();
          return;
        }

        // Inject the controller object into the page, wait for it to load or timeout...
        PluginProxies.createMediaCapturer(function(err) {
          loadError = err;

          if (!loadError && (!PluginProxies.mediaCapturer ||
                !PluginProxies.mediaCapturer.isValid())) {
            loadError = 'The TB Plugin failed to load properly';
          }

          pluginIsReady = true;
          notifyReadyListeners();

          $.onDOMUnload(destroy);
        });
      });
    };

// tb_require('./header.js')
// tb_require('./shims.js')
// tb_require('./proxy.js')
// tb_require('./auto_updater.js')
// tb_require('./media_constraints.js')
// tb_require('./peer_connection.js')
// tb_require('./media_stream.js')
// tb_require('./media_devices.js')
// tb_require('./video_container.js')
// tb_require('./rumor.js')
// tb_require('./lifecycle.js')

/* global AutoUpdater,
          RumorSocket,
          MediaConstraints, PeerConnection, MediaStream,
          registerReadyListener,
          PluginProxies,
          MediaDevices */

OTPlugin.isInstalled = function isInstalled() {
  if (!this.isSupported()) return false;
  return AutoUpdater.isinstalled();
};

OTPlugin.version = function version() {
  return OTPlugin.meta.version;
};

OTPlugin.installedVersion = function installedVersion() {
  return AutoUpdater.installedVersion();
};

// Returns a URI to the OTPlugin installer that is paired with
// this version of OTPlugin.js.
OTPlugin.pathToInstaller = function pathToInstaller() {
  return 'https://s3.amazonaws.com/otplugin.tokbox.com/v' +
                    OTPlugin.meta.version + '/OpenTokPluginMain.msi';
};

// Trigger +callback+ when the plugin is ready
//
// Most of the public API cannot be called until
// the plugin is ready.
//
OTPlugin.ready = function ready(callback) {
  registerReadyListener(callback);
};

// Helper function for OTPlugin.getUserMedia
var _getUserMedia = function _getUserMedia(mediaConstraints, success, error) {
  PluginProxies.createMediaPeer(function(err, plugin) {
    if (err) {
      error.call(OTPlugin, err);
      return;
    }

    plugin._.getUserMedia(mediaConstraints.toHash(), function(streamJson) {
      success.call(OTPlugin, MediaStream.fromJson(streamJson, plugin));
    }, error);
  });
};

// Equivalent to: window.getUserMedia(constraints, success, error);
//
// Except that the constraints won't be identical
OTPlugin.getUserMedia = function getUserMedia(userConstraints, success, error) {
  var constraints = new MediaConstraints(userConstraints);

  if (constraints.screenSharing) {
    _getUserMedia(constraints, success, error);
  } else {
    var sources = [];
    if (constraints.hasVideo) sources.push('video');
    if (constraints.hasAudio) sources.push('audio');

    PluginProxies.mediaCapturer.selectSources(sources, function(captureDevices) {
      for (var key in captureDevices) {
        if (captureDevices.hasOwnProperty(key)) {
          OTPlugin.debug(key + ' Capture Device: ' + captureDevices[key]);
        }
      }

      // Use the sources to acquire the hardware and start rendering
      constraints.setVideoSource(captureDevices.video);
      constraints.setAudioSource(captureDevices.audio);

      _getUserMedia(constraints, success, error);
    }, error);
  }
};

OTPlugin.enumerateDevices = function(completion) {
  OTPlugin.ready(function(error) {
    if (error) {
      completion(error);
    } else {
      PluginProxies.mediaCapturer.enumerateDevices(completion);
    }
  });
};

OTPlugin.initRumorSocket = function(messagingURL, completion) {
  OTPlugin.ready(function(error) {
    if (error) {
      completion(error);
    } else {
      completion(null, new RumorSocket(PluginProxies.mediaCapturer, messagingURL));
    }
  });
};

// Equivalent to: var pc = new window.RTCPeerConnection(iceServers, options);
//
// Except that it is async and takes a completion handler
OTPlugin.initPeerConnection = function initPeerConnection(iceServers,
                                                          options,
                                                          localStream,
                                                          completion) {

  var gotPeerObject = function(err, plugin) {
    if (err) {
      completion.call(OTPlugin, err);
      return;
    }

    OTPlugin.debug('Got PeerConnection for ' + plugin.id);

    PeerConnection.create(iceServers, options, plugin, function(err, peerConnection) {
      if (err) {
        completion.call(OTPlugin, err);
        return;
      }

      completion.call(OTPlugin, null, peerConnection);
    });
  };

  // @fixme this is nasty and brittle. We need some way to use the same Object
  // for the PeerConnection that was used for the getUserMedia call (in the case
  // of publishers). We don't really have a way of implicitly associating them though.
  // Hence, publishers will have to pass through their localStream (if they have one)
  // and we will look up the original Object and use that. Otherwise we generate
  // a new one.
  if (localStream && localStream._.plugin) {
    gotPeerObject(null, localStream._.plugin);
  } else {
    PluginProxies.createMediaPeer(gotPeerObject);
  }
};

// A RTCSessionDescription like object exposed for native WebRTC compatability
OTPlugin.RTCSessionDescription = function RTCSessionDescription(options) {
  this.type = options.type;
  this.sdp = options.sdp;
};

// A RTCIceCandidate like object exposed for native WebRTC compatability
OTPlugin.RTCIceCandidate = function RTCIceCandidate(options) {
  this.sdpMid = options.sdpMid;
  this.sdpMLineIndex = parseInt(options.sdpMLineIndex, 10);
  this.candidate = options.candidate;
};

OTPlugin.mediaDevices = new MediaDevices();


// tb_require('./api.js')

/* global shim, onDomReady */

shim();

$.onDOMLoad(onDomReady);

/* jshint ignore:start */
})(this);
/* jshint ignore:end */



/* exported _setCertificates */

var _setCertificates = function(pcConfig, completion) {
  if (
    OT.$.env.name === 'Firefox' &&
    window.mozRTCPeerConnection &&
    window.mozRTCPeerConnection.generateCertificate
  ) {
    window.mozRTCPeerConnection.generateCertificate({
      name: 'RSASSA-PKCS1-v1_5',
      hash: 'SHA-256',
      modulusLength: 2048,
      publicExponent: new Uint8Array([1, 0, 1])
    }).catch(function(err) {
      completion(err);
    }).then(function(cert) {
      pcConfig.certificates = [cert];
      completion(undefined, pcConfig);
    });
  } else {
    OT.$.callAsync(function() {
      completion(undefined, pcConfig);
    });
  }
};

/* exported videoContentResizesMixin */

var videoContentResizesMixin = function(self, domElement) {

  var width = domElement.videoWidth,
      height = domElement.videoHeight,
      stopped = true;

  function actor() {
    if (stopped) {
      return;
    }
    if (width !== domElement.videoWidth || height !== domElement.videoHeight) {
      self.trigger('videoDimensionsChanged',
        { width: width, height: height },
        { width: domElement.videoWidth, height: domElement.videoHeight }
      );
      width = domElement.videoWidth;
      height = domElement.videoHeight;
    }
    waiter();
  }

  function waiter() {
    self.whenTimeIncrements(function() {
      window.requestAnimationFrame(actor);
    });
  }

  self.startObservingSize = function() {
    stopped = false;
    waiter();
  };

  self.stopObservingSize = function() {
    stopped = true;
  };

};

/* jshint ignore:start */
!(function(window, OT) {
/* jshint ignore:end */

// tb_require('./header.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */

// This is not obvious, so to prevent end-user frustration we'll let them know
// explicitly rather than failing with a bunch of permission errors. We don't
// handle this using an OT Exception as it's really only a development thing.
if (location.protocol === 'file:') {
  /*global alert*/
  alert('You cannot test a page using WebRTC through the file system due to browser ' +
    'permissions. You must run it over a web server.');
}

var OT = window.OT || {};

// Define the APIKEY this is a global parameter which should not change
OT.APIKEY = (function() {
  // Script embed
  var scriptSrc = (function() {
    var s = document.getElementsByTagName('script');
    s = s[s.length - 1];
    s = s.getAttribute('src') || s.src;
    return s;
  })();

  var m = scriptSrc.match(/[\?\&]apikey=([^&]+)/i);
  return m ? m[1] : '';
})();

if (!window.OT) window.OT = OT;
if (!window.TB) window.TB = OT;

// tb_require('../js/ot.js')

OT.properties = {
  version: 'v2.6.8',         // The current version (eg. v2.0.4) (This is replaced by gradle)
  build: 'fae7901',    // The current build hash (This is replaced by gradle)

  // Whether or not to turn on debug logging by default
  debug: 'false',
  // The URL of the tokbox website
  websiteURL: 'http://www.tokbox.com',

  // The URL of the CDN
  cdnURL: 'http://static.opentok.com',
  // The URL to use for logging
  loggingURL: 'http://hlg.tokbox.com/prod',

  // The anvil API URL
  apiURL: 'http://anvil.opentok.com',

  // What protocol to use when connecting to the rumor web socket
  messagingProtocol: 'wss',
  // What port to use when connection to the rumor web socket
  messagingPort: 443,

  // If this environment supports SSL
  supportSSL: 'true',
  // The CDN to use if we're using SSL
  cdnURLSSL: 'https://static.opentok.com',
  // The URL to use for logging
  loggingURLSSL: 'https://hlg.tokbox.com/prod',

  // The anvil API URL to use if we're using SSL
  apiURLSSL: 'https://anvil.opentok.com',

  minimumVersion: {
    firefox: parseFloat('37'),
    chrome: parseFloat('39')
  }
};

// tb_require('../ot.js')
// tb_require('../../conf/properties.js');

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

// Mount OTHelpers on OT.$
OT.$ = window.OTHelpers;

// Allow events to be bound on OT
OT.$.eventing(OT);

// REMOVE THIS POST IE MERGE

OT.$.defineGetters = function(self, getters, enumerable) {
  var propsDefinition = {};

  if (enumerable === void 0) enumerable = false;

  for (var key in getters) {
    if (!getters.hasOwnProperty(key)) {
      continue;
    }

    propsDefinition[key] = {
      get: getters[key],
      enumerable: enumerable
    };
  }

  Object.defineProperties(self, propsDefinition);
};

// STOP REMOVING HERE

// OT.$.Modal was OT.Modal before the great common-js-helpers move
OT.Modal = OT.$.Modal;

// Add logging methods
OT.$.useLogHelpers(OT);

var _debugHeaderLogged = false,
    _setLogLevel = OT.setLogLevel;

// On the first time log level is set to DEBUG (or higher) show version info.
OT.setLogLevel = function(level) {
  // Set OT.$ to the same log level
  OT.$.setLogLevel(level);
  var retVal = _setLogLevel.call(OT, level);
  if (OT.shouldLog(OT.DEBUG) && !_debugHeaderLogged) {
    OT.debug('OpenTok JavaScript library ' + OT.properties.version);
    OT.debug('Release notes: ' + OT.properties.websiteURL +
      '/opentok/webrtc/docs/js/release-notes.html');
    OT.debug('Known issues: ' + OT.properties.websiteURL +
      '/opentok/webrtc/docs/js/release-notes.html#knownIssues');
    _debugHeaderLogged = true;
  }
  OT.debug('OT.setLogLevel(' + retVal + ')');
  return retVal;
};

var debugTrue = OT.properties.debug === 'true' || OT.properties.debug === true;
OT.setLogLevel(debugTrue ? OT.DEBUG : OT.ERROR);

// Patch the userAgent to ref OTPlugin, if it's installed.
if (OTPlugin && OTPlugin.isInstalled()) {
  OT.$.env.userAgent += '; OTPlugin ' + OTPlugin.version();
}

// @todo remove this
OT.$.userAgent = function() { return OT.$.env.userAgent; };

/**
  * Sets the API log level.
  * <p>
  * Calling <code>OT.setLogLevel()</code> sets the log level for runtime log messages that
  * are the OpenTok library generates. The default value for the log level is <code>OT.ERROR</code>.
  * </p>
  * <p>
  * The OpenTok JavaScript library displays log messages in the debugger console (such as
  * Firebug), if one exists.
  * </p>
  * <p>
  * The following example logs the session ID to the console, by calling <code>OT.log()</code>.
  * The code also logs an error message when it attempts to publish a stream before the Session
  * object dispatches a <code>sessionConnected</code> event.
  * </p>
  * <pre>
  * OT.setLogLevel(OT.LOG);
  * session = OT.initSession(sessionId);
  * OT.log(sessionId);
  * publisher = OT.initPublisher("publishContainer");
  * session.publish(publisher);
  * </pre>
  *
  * @param {Number} logLevel The degree of logging desired by the developer:
  *
  * <p>
  * <ul>
  *   <li>
  *     <code>OT.NONE</code> &#151; API logging is disabled.
  *   </li>
  *   <li>
  *     <code>OT.ERROR</code> &#151; Logging of errors only.
  *   </li>
  *   <li>
  *     <code>OT.WARN</code> &#151; Logging of warnings and errors.
  *   </li>
  *   <li>
  *     <code>OT.INFO</code> &#151; Logging of other useful information, in addition to
  *     warnings and errors.
  *   </li>
  *   <li>
  *     <code>OT.LOG</code> &#151; Logging of <code>OT.log()</code> messages, in addition
  *     to OpenTok info, warning,
  *     and error messages.
  *   </li>
  *   <li>
  *     <code>OT.DEBUG</code> &#151; Fine-grained logging of all API actions, as well as
  *     <code>OT.log()</code> messages.
  *   </li>
  * </ul>
  * </p>
  *
  * @name OT.setLogLevel
  * @memberof OT
  * @function
  * @see <a href="#log">OT.log()</a>
  */

/**
  * Sends a string to the the debugger console (such as Firebug), if one exists.
  * However, the function only logs to the console if you have set the log level
  * to <code>OT.LOG</code> or <code>OT.DEBUG</code>,
  * by calling <code>OT.setLogLevel(OT.LOG)</code> or <code>OT.setLogLevel(OT.DEBUG)</code>.
  *
  * @param {String} message The string to log.
  *
  * @name OT.log
  * @memberof OT
  * @function
  * @see <a href="#setLogLevel">OT.setLogLevel()</a>
  */

// tb_require('../../../helpers/helpers.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

// Rumor Messaging for JS
//
// https://tbwiki.tokbox.com/index.php/Rumor_:_Messaging_FrameWork
//
// @todo Rumor {
//     Add error codes for all the error cases
//     Add Dependability commands
// }

OT.Rumor = {
  MessageType: {
    // This is used to subscribe to address/addresses. The address/addresses the
    // client specifies here is registered on the server. Once any message is sent to
    // that address/addresses, the client receives that message.
    SUBSCRIBE: 0,

    // This is used to unsubscribe to address / addresses. Once the client unsubscribe
    // to an address, it will stop getting messages sent to that address.
    UNSUBSCRIBE: 1,

    // This is used to send messages to arbitrary address/ addresses. Messages can be
    // anything and Rumor will not care about what is included.
    MESSAGE: 2,

    // This will be the first message that the client sends to the server. It includes
    // the uniqueId for that client connection and a disconnect_notify address that will
    // be notified once the client disconnects.
    CONNECT: 3,

    // This will be the message used by the server to notify an address that a
    // client disconnected.
    DISCONNECT: 4,

    //Enhancements to support Keepalives
    PING: 7,
    PONG: 8,
    STATUS: 9
  }
};

// tb_require('../../../helpers/helpers.js')
// tb_require('./rumor.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT, OTPlugin */

!(function() {

  OT.Rumor.PluginSocket = function(messagingURL, events) {

    var webSocket,
        state = 'initializing';

    OTPlugin.initRumorSocket(messagingURL, OT.$.bind(function(err, rumorSocket) {
      if (err) {
        state = 'closed';
        events.onClose({ code: 4999 });
      } else if (state === 'initializing') {
        webSocket = rumorSocket;

        webSocket.onOpen(function() {
          state = 'open';
          events.onOpen();
        });
        webSocket.onClose(function(error) {
          state = 'closed'; /* CLOSED */
          events.onClose({ code: error });
        });
        webSocket.onError(function(error) {
          state = 'closed'; /* CLOSED */
          events.onError(error);
          /* native websockets seem to do this, so should we */
          events.onClose({ code: error });
        });

        webSocket.onMessage(function(type, addresses, headers, payload) {
          var msg = new OT.Rumor.Message(type, addresses, headers, payload);
          events.onMessage(msg);
        });

        webSocket.open();
      } else {
        this.close();
      }
    }, this));

    this.close = function() {
      if (state === 'initializing' || state === 'closed') {
        state = 'closed';
        return;
      }

      webSocket.close(1000, '');
    };

    this.send = function(msg) {
      if (state === 'open') {
        webSocket.send(msg);
      }
    };

    this.isClosed = function() {
      return state === 'closed';
    };

  };

}(this));

// tb_require('../../../helpers/helpers.js')

// https://code.google.com/p/stringencoding/
// An implementation of http://encoding.spec.whatwg.org/#api
// Modified by TokBox to remove all encoding support except for utf-8

/**
 * @license  Copyright 2014 Joshua Bell
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Original source: https://github.com/inexorabletash/text-encoding
 ***/
/*jshint unused:false*/

(function(global) {
  'use strict';

  if (OT.$.env && OT.$.env.name === 'IE' && OT.$.env.version < 10) {
    return; // IE 8 doesn't do websockets. No websockets, no encoding.
  }

  if ((global.TextEncoder !== void 0) && (global.TextDecoder !== void 0)) {
    // defer to the native ones
    return;
  }

  /* jshint ignore:start */

  //
  // Utilities
  //

  /**
   * @param {number} a The number to test.
   * @param {number} min The minimum value in the range, inclusive.
   * @param {number} max The maximum value in the range, inclusive.
   * @return {boolean} True if a >= min and a <= max.
   */
  function inRange(a, min, max) {
    return min <= a && a <= max;
  }

  /**
   * @param {number} n The numerator.
   * @param {number} d The denominator.
   * @return {number} The result of the integer division of n by d.
   */
  function div(n, d) {
    return Math.floor(n / d);
  }

  //
  // Implementation of Encoding specification
  // http://dvcs.w3.org/hg/encoding/raw-file/tip/Overview.html
  //

  //
  // 3. Terminology
  //

  //
  // 4. Encodings
  //

  /** @const */
  var EOF_byte = -1;
  /** @const */
  var EOF_code_point = -1;

  /**
   * @constructor
   * @param {Uint8Array} bytes Array of bytes that provide the stream.
   */
  function ByteInputStream(bytes) {
    /** @type {number} */
    var pos = 0;

    /** @return {number} Get the next byte from the stream. */
    this.get = function() {
      return (pos >= bytes.length) ? EOF_byte : Number(bytes[pos]);
    };

    /** @param {number} n Number (positive or negative) by which to
     *      offset the byte pointer. */
    this.offset = function(n) {
      pos += n;
      if (pos < 0) {
        throw new Error('Seeking past start of the buffer');
      }
      if (pos > bytes.length) {
        throw new Error('Seeking past EOF');
      }
    };

    /**
     * @param {Array.<number>} test Array of bytes to compare against.
     * @return {boolean} True if the start of the stream matches the test
     *     bytes.
     */
    this.match = function(test) {
      if (test.length > pos + bytes.length) {
        return false;
      }
      var i;
      for (i = 0; i < test.length; i += 1) {
        if (Number(bytes[pos + i]) !== test[i]) {
          return false;
        }
      }
      return true;
    };
  }

  /**
   * @constructor
   * @param {Array.<number>} bytes The array to write bytes into.
   */
  function ByteOutputStream(bytes) {
    /** @type {number} */
    var pos = 0;

    /**
     * @param {...number} var_args The byte or bytes to emit into the stream.
     * @return {number} The last byte emitted.
     */
    this.emit = function(var_args) {
      /** @type {number} */
      var last = EOF_byte;
      var i;
      for (i = 0; i < arguments.length; ++i) {
        last = Number(arguments[i]);
        bytes[pos++] = last;
      }
      return last;
    };
  }

  /**
   * @constructor
   * @param {string} string The source of code units for the stream.
   */
  function CodePointInputStream(string) {
    /**
     * @param {string} string Input string of UTF-16 code units.
     * @return {Array.<number>} Code points.
     */
    function stringToCodePoints(string) {
      /** @type {Array.<number>} */
      var cps = [];
      // Based on http://www.w3.org/TR/WebIDL/#idl-DOMString
      var i = 0, n = string.length;
      while (i < string.length) {
        var c = string.charCodeAt(i);
        if (!inRange(c, 0xD800, 0xDFFF)) {
          cps.push(c);
        } else if (inRange(c, 0xDC00, 0xDFFF)) {
          cps.push(0xFFFD);
        } else { // (inRange(cu, 0xD800, 0xDBFF))
          if (i === n - 1) {
            cps.push(0xFFFD);
          } else {
            var d = string.charCodeAt(i + 1);
            if (inRange(d, 0xDC00, 0xDFFF)) {
              var a = c & 0x3FF;
              var b = d & 0x3FF;
              i += 1;
              cps.push(0x10000 + (a << 10) + b);
            } else {
              cps.push(0xFFFD);
            }
          }
        }
        i += 1;
      }
      return cps;
    }

    /** @type {number} */
    var pos = 0;
    /** @type {Array.<number>} */
    var cps = stringToCodePoints(string);

    /** @param {number} n The number of bytes (positive or negative)
     *      to advance the code point pointer by.*/
    this.offset = function(n) {
      pos += n;
      if (pos < 0) {
        throw new Error('Seeking past start of the buffer');
      }
      if (pos > cps.length) {
        throw new Error('Seeking past EOF');
      }
    };


    /** @return {number} Get the next code point from the stream. */
    this.get = function() {
      if (pos >= cps.length) {
        return EOF_code_point;
      }
      return cps[pos];
    };
  }

  /**
   * @constructor
   */
  function CodePointOutputStream() {
    /** @type {string} */
    var string = '';

    /** @return {string} The accumulated string. */
    this.string = function() {
      return string;
    };

    /** @param {number} c The code point to encode into the stream. */
    this.emit = function(c) {
      if (c <= 0xFFFF) {
        string += String.fromCharCode(c);
      } else {
        c -= 0x10000;
        string += String.fromCharCode(0xD800 + ((c >> 10) & 0x3ff));
        string += String.fromCharCode(0xDC00 + (c & 0x3ff));
      }
    };
  }

  /**
   * @constructor
   * @param {string} message Description of the error.
   */
  function EncodingError(message) {
    this.name = 'EncodingError';
    this.message = message;
    this.code = 0;
  }
  EncodingError.prototype = Error.prototype;

  /**
   * @param {boolean} fatal If true, decoding errors raise an exception.
   * @param {number=} opt_code_point Override the standard fallback code point.
   * @return {number} The code point to insert on a decoding error.
   */
  function decoderError(fatal, opt_code_point) {
    if (fatal) {
      throw new EncodingError('Decoder error');
    }
    return opt_code_point || 0xFFFD;
  }

  /**
   * @param {number} code_point The code point that could not be encoded.
   */
  function encoderError(code_point) {
    throw new EncodingError('The code point ' + code_point +
                            ' could not be encoded.');
  }

  /**
   * @param {string} label The encoding label.
   * @return {?{name:string,labels:Array.<string>}}
   */
  function getEncoding(label) {
    label = String(label).trim().toLowerCase();
    if (Object.prototype.hasOwnProperty.call(label_to_encoding, label)) {
      return label_to_encoding[label];
    }
    return null;
  }

  /** @type {Array.<{encodings: Array.<{name:string,labels:Array.<string>}>,
   *      heading: string}>} */
  var encodings = [
    {
      'encodings': [
        {
          'labels': [
            'unicode-1-1-utf-8',
            'utf-8',
            'utf8'
          ],
          'name': 'utf-8'
        }
      ],
      'heading': 'The Encoding'
    },
    {
      'encodings': [
        {
          'labels': [
            'cp864',
            'ibm864'
          ],
          'name': 'ibm864'
        },
        {
          'labels': [
            'cp866',
            'ibm866'
          ],
          'name': 'ibm866'
        },
        {
          'labels': [
            'csisolatin2',
            'iso-8859-2',
            'iso-ir-101',
            'iso8859-2',
            'iso_8859-2',
            'l2',
            'latin2'
          ],
          'name': 'iso-8859-2'
        },
        {
          'labels': [
            'csisolatin3',
            'iso-8859-3',
            'iso_8859-3',
            'iso-ir-109',
            'l3',
            'latin3'
          ],
          'name': 'iso-8859-3'
        },
        {
          'labels': [
            'csisolatin4',
            'iso-8859-4',
            'iso_8859-4',
            'iso-ir-110',
            'l4',
            'latin4'
          ],
          'name': 'iso-8859-4'
        },
        {
          'labels': [
            'csisolatincyrillic',
            'cyrillic',
            'iso-8859-5',
            'iso_8859-5',
            'iso-ir-144'
          ],
          'name': 'iso-8859-5'
        },
        {
          'labels': [
            'arabic',
            'csisolatinarabic',
            'ecma-114',
            'iso-8859-6',
            'iso_8859-6',
            'iso-ir-127'
          ],
          'name': 'iso-8859-6'
        },
        {
          'labels': [
            'csisolatingreek',
            'ecma-118',
            'elot_928',
            'greek',
            'greek8',
            'iso-8859-7',
            'iso_8859-7',
            'iso-ir-126'
          ],
          'name': 'iso-8859-7'
        },
        {
          'labels': [
            'csisolatinhebrew',
            'hebrew',
            'iso-8859-8',
            'iso-8859-8-i',
            'iso-ir-138',
            'iso_8859-8',
            'visual'
          ],
          'name': 'iso-8859-8'
        },
        {
          'labels': [
            'csisolatin6',
            'iso-8859-10',
            'iso-ir-157',
            'iso8859-10',
            'l6',
            'latin6'
          ],
          'name': 'iso-8859-10'
        },
        {
          'labels': [
            'iso-8859-13'
          ],
          'name': 'iso-8859-13'
        },
        {
          'labels': [
            'iso-8859-14',
            'iso8859-14'
          ],
          'name': 'iso-8859-14'
        },
        {
          'labels': [
            'iso-8859-15',
            'iso_8859-15'
          ],
          'name': 'iso-8859-15'
        },
        {
          'labels': [
            'iso-8859-16'
          ],
          'name': 'iso-8859-16'
        },
        {
          'labels': [
            'koi8-r',
            'koi8_r'
          ],
          'name': 'koi8-r'
        },
        {
          'labels': [
            'koi8-u'
          ],
          'name': 'koi8-u'
        },
        {
          'labels': [
            'csmacintosh',
            'mac',
            'macintosh',
            'x-mac-roman'
          ],
          'name': 'macintosh'
        },
        {
          'labels': [
            'iso-8859-11',
            'tis-620',
            'windows-874'
          ],
          'name': 'windows-874'
        },
        {
          'labels': [
            'windows-1250',
            'x-cp1250'
          ],
          'name': 'windows-1250'
        },
        {
          'labels': [
            'windows-1251',
            'x-cp1251'
          ],
          'name': 'windows-1251'
        },
        {
          'labels': [
            'ascii',
            'ansi_x3.4-1968',
            'csisolatin1',
            'iso-8859-1',
            'iso8859-1',
            'iso_8859-1',
            'l1',
            'latin1',
            'us-ascii',
            'windows-1252'
          ],
          'name': 'windows-1252'
        },
        {
          'labels': [
            'cp1253',
            'windows-1253'
          ],
          'name': 'windows-1253'
        },
        {
          'labels': [
            'csisolatin5',
            'iso-8859-9',
            'iso-ir-148',
            'l5',
            'latin5',
            'windows-1254'
          ],
          'name': 'windows-1254'
        },
        {
          'labels': [
            'cp1255',
            'windows-1255'
          ],
          'name': 'windows-1255'
        },
        {
          'labels': [
            'cp1256',
            'windows-1256'
          ],
          'name': 'windows-1256'
        },
        {
          'labels': [
            'windows-1257'
          ],
          'name': 'windows-1257'
        },
        {
          'labels': [
            'cp1258',
            'windows-1258'
          ],
          'name': 'windows-1258'
        },
        {
          'labels': [
            'x-mac-cyrillic',
            'x-mac-ukrainian'
          ],
          'name': 'x-mac-cyrillic'
        }
      ],
      'heading': 'Legacy single-byte encodings'
    },
    {
      'encodings': [
        {
          'labels': [
            'chinese',
            'csgb2312',
            'csiso58gb231280',
            'gb2312',
            'gbk',
            'gb_2312',
            'gb_2312-80',
            'iso-ir-58',
            'x-gbk'
          ],
          'name': 'gbk'
        },
        {
          'labels': [
            'gb18030'
          ],
          'name': 'gb18030'
        },
        {
          'labels': [
            'hz-gb-2312'
          ],
          'name': 'hz-gb-2312'
        }
      ],
      'heading': 'Legacy multi-byte Chinese (simplified) encodings'
    },
    {
      'encodings': [
        {
          'labels': [
            'big5',
            'big5-hkscs',
            'cn-big5',
            'csbig5',
            'x-x-big5'
          ],
          'name': 'big5'
        }
      ],
      'heading': 'Legacy multi-byte Chinese (traditional) encodings'
    },
    {
      'encodings': [
        {
          'labels': [
            'cseucpkdfmtjapanese',
            'euc-jp',
            'x-euc-jp'
          ],
          'name': 'euc-jp'
        },
        {
          'labels': [
            'csiso2022jp',
            'iso-2022-jp'
          ],
          'name': 'iso-2022-jp'
        },
        {
          'labels': [
            'csshiftjis',
            'ms_kanji',
            'shift-jis',
            'shift_jis',
            'sjis',
            'windows-31j',
            'x-sjis'
          ],
          'name': 'shift_jis'
        }
      ],
      'heading': 'Legacy multi-byte Japanese encodings'
    },
    {
      'encodings': [
        {
          'labels': [
            'cseuckr',
            'csksc56011987',
            'euc-kr',
            'iso-ir-149',
            'korean',
            'ks_c_5601-1987',
            'ks_c_5601-1989',
            'ksc5601',
            'ksc_5601',
            'windows-949'
          ],
          'name': 'euc-kr'
        },
        {
          'labels': [
            'csiso2022kr',
            'iso-2022-kr'
          ],
          'name': 'iso-2022-kr'
        }
      ],
      'heading': 'Legacy multi-byte Korean encodings'
    },
    {
      'encodings': [
        {
          'labels': [
            'utf-16',
            'utf-16le'
          ],
          'name': 'utf-16'
        },
        {
          'labels': [
            'utf-16be'
          ],
          'name': 'utf-16be'
        }
      ],
      'heading': 'Legacy utf-16 encodings'
    }
  ];

  var name_to_encoding = {};
  var label_to_encoding = {};
  encodings.forEach(function(category) {
    category.encodings.forEach(function(encoding) {
      name_to_encoding[encoding.name] = encoding;
      encoding.labels.forEach(function(label) {
        label_to_encoding[label] = encoding;
      });
    });
  });

  //
  // 5. Indexes
  //

  /**
   * @param {number} pointer The |pointer| to search for.
   * @param {Array.<?number>} index The |index| to search within.
   * @return {?number} The code point corresponding to |pointer| in |index|,
   *     or null if |code point| is not in |index|.
   */
  function indexCodePointFor(pointer, index) {
    return (index || [])[pointer] || null;
  }

  /**
   * @param {number} code_point The |code point| to search for.
   * @param {Array.<?number>} index The |index| to search within.
   * @return {?number} The first pointer corresponding to |code point| in
   *     |index|, or null if |code point| is not in |index|.
   */
  function indexPointerFor(code_point, index) {
    var pointer = index.indexOf(code_point);
    return pointer === -1 ? null : pointer;
  }

  /** @type {Object.<string, (Array.<number>|Array.<Array.<number>>)>} */
  var indexes = global['encoding-indexes'] || {};


  //
  // 7. The encoding
  //

  // 7.1 utf-8

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function UTF8Decoder(options) {
    var fatal = options.fatal;
    var utf8_code_point = 0,
        utf8_bytes_needed = 0,
        utf8_bytes_seen = 0,
        utf8_lower_boundary = 0;

    /**
     * @param {ByteInputStream} byte_pointer The byte stream to decode.
     * @return {?number} The next code point decoded, or null if not enough
     *     data exists in the input stream to decode a complete code point.
     */
    this.decode = function(byte_pointer) {
      var bite = byte_pointer.get();
      if (bite === EOF_byte) {
        if (utf8_bytes_needed !== 0) {
          return decoderError(fatal);
        }
        return EOF_code_point;
      }
      byte_pointer.offset(1);

      if (utf8_bytes_needed === 0) {
        if (inRange(bite, 0x00, 0x7F)) {
          return bite;
        }
        if (inRange(bite, 0xC2, 0xDF)) {
          utf8_bytes_needed = 1;
          utf8_lower_boundary = 0x80;
          utf8_code_point = bite - 0xC0;
        } else if (inRange(bite, 0xE0, 0xEF)) {
          utf8_bytes_needed = 2;
          utf8_lower_boundary = 0x800;
          utf8_code_point = bite - 0xE0;
        } else if (inRange(bite, 0xF0, 0xF4)) {
          utf8_bytes_needed = 3;
          utf8_lower_boundary = 0x10000;
          utf8_code_point = bite - 0xF0;
        } else {
          return decoderError(fatal);
        }
        utf8_code_point = utf8_code_point * Math.pow(64, utf8_bytes_needed);
        return null;
      }
      if (!inRange(bite, 0x80, 0xBF)) {
        utf8_code_point = 0;
        utf8_bytes_needed = 0;
        utf8_bytes_seen = 0;
        utf8_lower_boundary = 0;
        byte_pointer.offset(-1);
        return decoderError(fatal);
      }
      utf8_bytes_seen += 1;
      utf8_code_point = utf8_code_point + (bite - 0x80) *
          Math.pow(64, utf8_bytes_needed - utf8_bytes_seen);
      if (utf8_bytes_seen !== utf8_bytes_needed) {
        return null;
      }
      var code_point = utf8_code_point;
      var lower_boundary = utf8_lower_boundary;
      utf8_code_point = 0;
      utf8_bytes_needed = 0;
      utf8_bytes_seen = 0;
      utf8_lower_boundary = 0;
      if (inRange(code_point, lower_boundary, 0x10FFFF) &&
          !inRange(code_point, 0xD800, 0xDFFF)) {
        return code_point;
      }
      return decoderError(fatal);
    };
  }

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function UTF8Encoder(options) {
    var fatal = options.fatal;
    /**
     * @param {ByteOutputStream} output_byte_stream Output byte stream.
     * @param {CodePointInputStream} code_point_pointer Input stream.
     * @return {number} The last byte emitted.
     */
    this.encode = function(output_byte_stream, code_point_pointer) {
      var code_point = code_point_pointer.get();
      if (code_point === EOF_code_point) {
        return EOF_byte;
      }
      code_point_pointer.offset(1);
      if (inRange(code_point, 0xD800, 0xDFFF)) {
        return encoderError(code_point);
      }
      if (inRange(code_point, 0x0000, 0x007f)) {
        return output_byte_stream.emit(code_point);
      }
      var count, offset;
      if (inRange(code_point, 0x0080, 0x07FF)) {
        count = 1;
        offset = 0xC0;
      } else if (inRange(code_point, 0x0800, 0xFFFF)) {
        count = 2;
        offset = 0xE0;
      } else if (inRange(code_point, 0x10000, 0x10FFFF)) {
        count = 3;
        offset = 0xF0;
      }
      var result = output_byte_stream.emit(
          div(code_point, Math.pow(64, count)) + offset);
      while (count > 0) {
        var temp = div(code_point, Math.pow(64, count - 1));
        result = output_byte_stream.emit(0x80 + (temp % 64));
        count -= 1;
      }
      return result;
    };
  }

  name_to_encoding['utf-8'].getEncoder = function(options) {
    return new UTF8Encoder(options);
  };
  name_to_encoding['utf-8'].getDecoder = function(options) {
    return new UTF8Decoder(options);
  };


  // NOTE: currently unused
  /**
   * @param {string} label The encoding label.
   * @param {ByteInputStream} input_stream The byte stream to test.
   */
  function detectEncoding(label, input_stream) {
    if (input_stream.match([0xFF, 0xFE])) {
      input_stream.offset(2);
      return 'utf-16';
    }
    if (input_stream.match([0xFE, 0xFF])) {
      input_stream.offset(2);
      return 'utf-16be';
    }
    if (input_stream.match([0xEF, 0xBB, 0xBF])) {
      input_stream.offset(3);
      return 'utf-8';
    }
    return label;
  }

  /**
   * @param {string} label The encoding label.
   * @param {ByteInputStream} input_stream The byte stream to test.
   */
  function consumeBOM(label, input_stream) {
    if (input_stream.match([0xFF, 0xFE]) && label === 'utf-16') {
      input_stream.offset(2);
      return;
    }
    if (input_stream.match([0xFE, 0xFF]) && label == 'utf-16be') {
      input_stream.offset(2);
      return;
    }
    if (input_stream.match([0xEF, 0xBB, 0xBF]) && label == 'utf-8') {
      input_stream.offset(3);
      return;
    }
  }

  //
  // Implementation of Text Encoding Web API
  //

  /** @const */
  var DEFAULT_ENCODING = 'utf-8';

  /**
   * @constructor
   * @param {string=} opt_encoding The label of the encoding;
   *     defaults to 'utf-8'.
   * @param {{fatal: boolean}=} options
   */
  function TextEncoder(opt_encoding, options) {
    if (!this || this === global) {
      return new TextEncoder(opt_encoding, options);
    }
    opt_encoding = opt_encoding ? String(opt_encoding) : DEFAULT_ENCODING;
    options = Object(options);
    /** @private */
    this._encoding = getEncoding(opt_encoding);
    if (this._encoding === null || (this._encoding.name !== 'utf-8' &&
                                    this._encoding.name !== 'utf-16' &&
                                    this._encoding.name !== 'utf-16be'))
      throw new TypeError('Unknown encoding: ' + opt_encoding);
    /* @private @type {boolean} */
    this._streaming = false;
    /** @private */
    this._encoder = null;
    /* @private @type {{fatal: boolean}=} */
    this._options = { fatal: Boolean(options.fatal) };

    if (Object.defineProperty) {
      Object.defineProperty(
          this, 'encoding',
          { get: function() { return this._encoding.name; } });
    } else {
      this.encoding = this._encoding.name;
    }

    return this;
  }

  TextEncoder.prototype = {
    /**
     * @param {string=} opt_string The string to encode.
     * @param {{stream: boolean}=} options
     */
    encode: function encode(opt_string, options) {
      opt_string = opt_string ? String(opt_string) : '';
      options = Object(options);
      // TODO: any options?
      if (!this._streaming) {
        this._encoder = this._encoding.getEncoder(this._options);
      }
      this._streaming = Boolean(options.stream);

      var bytes = [];
      var output_stream = new ByteOutputStream(bytes);
      var input_stream = new CodePointInputStream(opt_string);
      while (input_stream.get() !== EOF_code_point) {
        this._encoder.encode(output_stream, input_stream);
      }
      if (!this._streaming) {
        var last_byte;
        do {
          last_byte = this._encoder.encode(output_stream, input_stream);
        } while (last_byte !== EOF_byte);
        this._encoder = null;
      }
      return new Uint8Array(bytes);
    }
  };


  /**
   * @constructor
   * @param {string=} opt_encoding The label of the encoding;
   *     defaults to 'utf-8'.
   * @param {{fatal: boolean}=} options
   */
  function TextDecoder(opt_encoding, options) {
    if (!this || this === global) {
      return new TextDecoder(opt_encoding, options);
    }
    opt_encoding = opt_encoding ? String(opt_encoding) : DEFAULT_ENCODING;
    options = Object(options);
    /** @private */
    this._encoding = getEncoding(opt_encoding);
    if (this._encoding === null)
      throw new TypeError('Unknown encoding: ' + opt_encoding);

    /* @private @type {boolean} */
    this._streaming = false;
    /** @private */
    this._decoder = null;
    /* @private @type {{fatal: boolean}=} */
    this._options = { fatal: Boolean(options.fatal) };

    if (Object.defineProperty) {
      Object.defineProperty(
          this, 'encoding',
          { get: function() { return this._encoding.name; } });
    } else {
      this.encoding = this._encoding.name;
    }

    return this;
  }

  // TODO: Issue if input byte stream is offset by decoder
  // TODO: BOM detection will not work if stream header spans multiple calls
  // (last N bytes of previous stream may need to be retained?)
  TextDecoder.prototype = {
    /**
     * @param {ArrayBufferView=} opt_view The buffer of bytes to decode.
     * @param {{stream: boolean}=} options
     */
    decode: function decode(opt_view, options) {
      if (opt_view && !('buffer' in opt_view && 'byteOffset' in opt_view &&
                        'byteLength' in opt_view)) {
        throw new TypeError('Expected ArrayBufferView');
      } else if (!opt_view) {
        opt_view = new Uint8Array(0);
      }
      options = Object(options);

      if (!this._streaming) {
        this._decoder = this._encoding.getDecoder(this._options);
      }
      this._streaming = Boolean(options.stream);

      var bytes = new Uint8Array(opt_view.buffer,
                                 opt_view.byteOffset,
                                 opt_view.byteLength);
      var input_stream = new ByteInputStream(bytes);

      if (!this._BOMseen) {
        // TODO: Don't do this until sufficient bytes are present
        this._BOMseen = true;
        consumeBOM(this._encoding.name, input_stream);
      }

      var output_stream = new CodePointOutputStream(), code_point;
      while (input_stream.get() !== EOF_byte) {
        code_point = this._decoder.decode(input_stream);
        if (code_point !== null && code_point !== EOF_code_point) {
          output_stream.emit(code_point);
        }
      }
      if (!this._streaming) {
        do {
          code_point = this._decoder.decode(input_stream);
          if (code_point !== null && code_point !== EOF_code_point) {
            output_stream.emit(code_point);
          }
        } while (code_point !== EOF_code_point &&
                 input_stream.get() != EOF_byte);
        this._decoder = null;
      }
      return output_stream.string();
    }
  };

  global['TextEncoder'] = global['TextEncoder'] || TextEncoder;
  global['TextDecoder'] = global['TextDecoder'] || TextDecoder;

  /* jshint ignore:end */

}(this));

// tb_require('../../../helpers/helpers.js')
// tb_require('./encoding.js')
// tb_require('./rumor.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT, TextEncoder, TextDecoder */

//
//
// @references
// * https://tbwiki.tokbox.com/index.php/Rumor_Message_Packet
// * https://tbwiki.tokbox.com/index.php/Rumor_Protocol
//
OT.Rumor.Message = function(type, toAddress, headers, data) {
  this.type = type;
  this.toAddress = toAddress;
  this.headers = headers;
  this.data = data;

  this.transactionId = this.headers['TRANSACTION-ID'];
  this.status = this.headers.STATUS;
  this.isError = !(this.status && this.status[0] === '2');
};

OT.Rumor.Message.prototype.serialize = function() {
  var offset = 8,
      cBuf = 7,
      address = [],
      headerKey = [],
      headerVal = [],
      strArray,
      dataView,
      i,
      j;

  // The number of addresses
  cBuf++;

  // Write out the address.
  for (i = 0; i < this.toAddress.length; i++) {
    /*jshint newcap:false */
    address.push(new TextEncoder('utf-8').encode(this.toAddress[i]));
    cBuf += 2;
    cBuf += address[i].length;
  }

  // The number of parameters
  cBuf++;

  // Write out the params
  i = 0;

  for (var key in this.headers) {
    if (!this.headers.hasOwnProperty(key)) {
      continue;
    }
    headerKey.push(new TextEncoder('utf-8').encode(key));
    headerVal.push(new TextEncoder('utf-8').encode(this.headers[key]));
    cBuf += 4;
    cBuf += headerKey[i].length;
    cBuf += headerVal[i].length;

    i++;
  }

  dataView = new TextEncoder('utf-8').encode(this.data);
  cBuf += dataView.length;

  // Let's allocate a binary blob of this size
  var buffer = new ArrayBuffer(cBuf);
  var uint8View = new Uint8Array(buffer, 0, cBuf);

  // We don't include the header in the lenght.
  cBuf -= 4;

  // Write out size (in network order)
  uint8View[0] = (cBuf & 0xFF000000) >>> 24;
  uint8View[1] = (cBuf & 0x00FF0000) >>> 16;
  uint8View[2] = (cBuf & 0x0000FF00) >>>  8;
  uint8View[3] = (cBuf & 0x000000FF) >>>  0;

  // Write out reserved bytes
  uint8View[4] = 0;
  uint8View[5] = 0;

  // Write out message type
  uint8View[6] = this.type;
  uint8View[7] = this.toAddress.length;

  // Now just copy over the encoded values..
  for (i = 0; i < address.length; i++) {
    strArray = address[i];
    uint8View[offset++] = strArray.length >> 8 & 0xFF;
    uint8View[offset++] = strArray.length >> 0 & 0xFF;
    for (j = 0; j < strArray.length; j++) {
      uint8View[offset++] = strArray[j];
    }
  }

  uint8View[offset++] = headerKey.length;

  // Write out the params
  for (i = 0; i < headerKey.length; i++) {
    strArray = headerKey[i];
    uint8View[offset++] = strArray.length >> 8 & 0xFF;
    uint8View[offset++] = strArray.length >> 0 & 0xFF;
    for (j = 0; j < strArray.length; j++) {
      uint8View[offset++] = strArray[j];
    }

    strArray = headerVal[i];
    uint8View[offset++] = strArray.length >> 8 & 0xFF;
    uint8View[offset++] = strArray.length >> 0 & 0xFF;
    for (j = 0; j < strArray.length; j++) {
      uint8View[offset++] = strArray[j];
    }
  }

  // And finally the data
  for (i = 0; i < dataView.length; i++) {
    uint8View[offset++] = dataView[i];
  }

  return buffer;
};

function toArrayBuffer(buffer) {
  var ab = new ArrayBuffer(buffer.length);
  var view = new Uint8Array(ab);
  for (var i = 0; i < buffer.length; ++i) {
    view[i] = buffer[i];
  }
  return ab;
}

OT.Rumor.Message.deserialize = function(buffer) {

  if (typeof Buffer !== 'undefined' &&
    Buffer.isBuffer(buffer)) {
    buffer = toArrayBuffer(buffer);
  }
  var cBuf = 0,
      type,
      offset = 8,
      uint8View = new Uint8Array(buffer),
      strView,
      headerlen,
      headers,
      keyStr,
      valStr,
      length,
      i;

  // Write out size (in network order)
  cBuf += uint8View[0] << 24;
  cBuf += uint8View[1] << 16;
  cBuf += uint8View[2] <<  8;
  cBuf += uint8View[3] <<  0;

  type = uint8View[6];
  var address = [];

  for (i = 0; i < uint8View[7]; i++) {
    length = uint8View[offset++] << 8;
    length += uint8View[offset++];
    strView = new Uint8Array(buffer, offset, length);
    /*jshint newcap:false */
    address[i] = new TextDecoder('utf-8').decode(strView);
    offset += length;
  }

  headerlen = uint8View[offset++];
  headers = {};

  for (i = 0; i < headerlen; i++) {
    length = uint8View[offset++] << 8;
    length += uint8View[offset++];
    strView = new Uint8Array(buffer, offset, length);
    keyStr = new TextDecoder('utf-8').decode(strView);
    offset += length;

    length = uint8View[offset++] << 8;
    length += uint8View[offset++];
    strView = new Uint8Array(buffer, offset, length);
    valStr = new TextDecoder('utf-8').decode(strView);
    headers[keyStr] = valStr;
    offset += length;
  }

  var dataView = new Uint8Array(buffer, offset);
  var data = new TextDecoder('utf-8').decode(dataView);

  return new OT.Rumor.Message(type, address, headers, data);
};

OT.Rumor.Message.Connect = function(uniqueId, notifyDisconnectAddress) {
  var headers = {
    uniqueId: uniqueId,
    notifyDisconnectAddress: notifyDisconnectAddress
  };

  return new OT.Rumor.Message(OT.Rumor.MessageType.CONNECT, [], headers, '');
};

OT.Rumor.Message.Disconnect = function() {
  return new OT.Rumor.Message(OT.Rumor.MessageType.DISCONNECT, [], {}, '');
};

OT.Rumor.Message.Subscribe = function(topics) {
  return new OT.Rumor.Message(OT.Rumor.MessageType.SUBSCRIBE, topics, {}, '');
};

OT.Rumor.Message.Unsubscribe = function(topics) {
  return new OT.Rumor.Message(OT.Rumor.MessageType.UNSUBSCRIBE, topics, {}, '');
};

OT.Rumor.Message.Publish = function(topics, message, headers) {
  return new OT.Rumor.Message(OT.Rumor.MessageType.MESSAGE, topics, headers || {}, message || '');
};

// This message is used to implement keepalives on the persistent
// socket connection between the client and server. Every time the
// client sends a PING to the server, the server will respond with
// a PONG.
OT.Rumor.Message.Ping = function() {
  return new OT.Rumor.Message(OT.Rumor.MessageType.PING, [], {}, '');
};

// tb_require('../../../helpers/helpers.js')
// tb_require('./rumor.js')
// tb_require('./message.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

!(function() {

  var BUFFER_DRAIN_INTERVAL = 100,
      // The total number of times to retest the websocket's send buffer
      BUFFER_DRAIN_MAX_RETRIES = 10;

  OT.Rumor.NativeSocket = function(TheWebSocket, messagingURL, events) {

    var webSocket,
        disconnectWhenSendBufferIsDrained,
        bufferDrainTimeout,           // Timer to poll whether th send buffer has been drained
        close;

    webSocket = new TheWebSocket(messagingURL);
    webSocket.binaryType = 'arraybuffer';

    webSocket.onopen = events.onOpen;
    webSocket.onclose = events.onClose;
    webSocket.onerror = events.onError;

    webSocket.onmessage = function(message) {
      if (!OT) {
        // In IE 10/11, This can apparently be called after
        // the page is unloaded and OT is garbage-collected
        return;
      }

      var msg = OT.Rumor.Message.deserialize(message.data);
      events.onMessage(msg);
    };

    // Ensure that the WebSocket send buffer is fully drained before disconnecting
    // the socket. If the buffer doesn't drain after a certain length of time
    // we give up and close it anyway.
    disconnectWhenSendBufferIsDrained =
      function disconnectWhenSendBufferIsDrained(bufferDrainRetries) {
      if (!webSocket) return;

      if (bufferDrainRetries === void 0) bufferDrainRetries = 0;
      if (bufferDrainTimeout) clearTimeout(bufferDrainTimeout);

      if (webSocket.bufferedAmount > 0 &&
        (bufferDrainRetries + 1) <= BUFFER_DRAIN_MAX_RETRIES) {
        bufferDrainTimeout = setTimeout(disconnectWhenSendBufferIsDrained,
          BUFFER_DRAIN_INTERVAL, bufferDrainRetries + 1);

      } else {
        close();
      }
    };

    close = function close() {
      webSocket.close();
    };

    this.close = function(drainBuffer) {
      if (drainBuffer) {
        disconnectWhenSendBufferIsDrained();
      } else {
        close();
      }
    };

    this.send = function(msg) {
      webSocket.send(msg.serialize());
    };

    this.isClosed = function() {
      return webSocket.readyState === 3;
    };

  };

}(this));

// tb_require('../../../helpers/helpers.js')
// tb_require('./message.js')
// tb_require('./native_socket.js')
// tb_require('./plugin_socket.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

var WEB_SOCKET_KEEP_ALIVE_INTERVAL = 9000,

    // Magic Connectivity Timeout Constant: We wait 9*the keep alive interval,
    // on the third keep alive we trigger the timeout if we haven't received the
    // server pong.
    WEB_SOCKET_CONNECTIVITY_TIMEOUT = 5 * WEB_SOCKET_KEEP_ALIVE_INTERVAL - 100,

    wsCloseErrorCodes,
    errorMap;

// https://developer.mozilla.org/en-US/docs/Web/API/CloseEvent#Close_codes
// http://docs.oracle.com/javaee/7/api/javax/websocket/CloseReason.CloseCodes.html
wsCloseErrorCodes = {
  1002:  'The endpoint is terminating the connection due to a protocol error. ' +
    '(CLOSE_PROTOCOL_ERROR)',
  1003:  'The connection is being terminated because the endpoint received data of ' +
    'a type it cannot accept (for example, a text-only endpoint received binary data). ' +
    '(CLOSE_UNSUPPORTED)',
  1004:  'The endpoint is terminating the connection because a data frame was received ' +
    'that is too large. (CLOSE_TOO_LARGE)',
  1005:  'Indicates that no status code was provided even though one was expected. ' +
  '(CLOSE_NO_STATUS)',
  1006:  'Used to indicate that a connection was closed abnormally (that is, with no ' +
    'close frame being sent) when a status code is expected. (CLOSE_ABNORMAL)',
  1007: 'Indicates that an endpoint is terminating the connection because it has received ' +
    'data within a message that was not consistent with the type of the message (e.g., ' +
    'non-UTF-8 [RFC3629] data within a text message)',
  1008: 'Indicates that an endpoint is terminating the connection because it has received a ' +
    'message that violates its policy.  This is a generic status code that can be returned ' +
    'when there is no other more suitable status code (e.g., 1003 or 1009) or if there is a ' +
    'need to hide specific details about the policy',
  1009: 'Indicates that an endpoint is terminating the connection because it has received a ' +
    'message that is too big for it to process',
  1011: 'Indicates that a server is terminating the connection because it encountered an ' +
    'unexpected condition that prevented it from fulfilling the request',

  // .... codes in the 4000-4999 range are available for use by applications.
  4001: 'Connectivity loss was detected as it was too long since the socket received the ' +
    'last PONG message',
  4010: 'Timed out while waiting for the Rumor socket to connect.',
  4020: 'Error code unavailable.',
  4030: 'Exception was thrown during Rumor connection, possibly because of a blocked port.'
};

errorMap = {
  CLOSE_PROTOCOL_ERROR: 1002,
  CLOSE_UNSUPPORTED: 1003,
  CLOSE_TOO_LARGE: 1004,
  CLOSE_NO_STATUS: 1005,
  CLOSE_ABNORMAL: 1006,
  CLOSE_TIMEOUT: 4010,
  CLOSE_FALLBACK_CODE: 4020,
  CLOSE_CONNECT_EXCEPTION: 4030
};

OT.Rumor.SocketError = function(code, message) {
  this.code = code;
  this.message = message;
};

// The NativeSocket bit is purely to make testing simpler, it defaults to WebSocket
// so in normal operation you would omit it.
OT.Rumor.Socket = function(messagingURL, notifyDisconnectAddress, NativeSocket) {

  var _this = this;

  var states = ['disconnected',  'error', 'connected', 'connecting', 'disconnecting'],
      webSocket,
      id,
      onOpen,
      onError,
      onClose,
      onMessage,
      connectCallback,
      connectTimeout,
      lastMessageTimestamp,         // The timestamp of the last message received
      keepAliveTimer;               // Timer for the connectivity checks

  //// Private API
  var stateChanged = function(newState) {
        switch (newState) {
          case 'disconnected':
          case 'error':
            webSocket = null;
            if (onClose) {
              var error;
              if (hasLostConnectivity()) {
                error = new Error(wsCloseErrorCodes[4001]);
                error.code = 4001;
              }
              onClose(error);
            }
            break;
        }
      },

      setState = OT.$.statable(this, states, 'disconnected', stateChanged),

      validateCallback = function validateCallback(name, callback) {
        if (callback === null || !OT.$.isFunction(callback)) {
          throw new Error('The Rumor.Socket ' + name +
            ' callback must be a valid function or null');
        }
      },

      raiseError = function raiseError(code, extraDetail) {
        code = code || errorMap.CLOSE_FALLBACK_CODE;

        var messageFromCode = wsCloseErrorCodes[code] || 'No message available from code.';
        var message = messageFromCode + (extraDetail ? ' ' + extraDetail : '');

        OT.error('Rumor.Socket: ' + message);

        var socketError = new OT.Rumor.SocketError(code, message);

        if (connectTimeout) clearTimeout(connectTimeout);

        setState('error');

        if (_this.previousState === 'connecting' && connectCallback) {
          connectCallback(socketError, void 0);
          connectCallback = null;
        }

        if (onError) onError(socketError);
      },

      hasLostConnectivity = function hasLostConnectivity() {
        if (!lastMessageTimestamp) return false;

        return (OT.$.now() - lastMessageTimestamp) >= WEB_SOCKET_CONNECTIVITY_TIMEOUT;
      },

      sendKeepAlive = function() {
        if (!_this.is('connected')) return;

        if (hasLostConnectivity()) {
          webSocketDisconnected({code: 4001});
        } else {
          webSocket.send(OT.Rumor.Message.Ping());
          keepAliveTimer = setTimeout(sendKeepAlive, WEB_SOCKET_KEEP_ALIVE_INTERVAL);
        }
      },

      // Returns true if we think the DOM has been unloaded
      // It detects this by looking for the OT global, which
      // should always exist until the DOM is cleaned up.
      isDOMUnloaded = function isDOMUnloaded() {
        return !window.OT;
      };

  //// Private Event Handlers
  var webSocketConnected = function webSocketConnected() {
        if (connectTimeout) clearTimeout(connectTimeout);
        if (_this.isNot('connecting')) {
          OT.debug('webSocketConnected reached in state other than connecting');
          return;
        }

        // Connect to Rumor by registering our connection id and the
        // app server address to notify if we disconnect.
        //
        // We don't need to wait for a reply to this message.
        webSocket.send(OT.Rumor.Message.Connect(id, notifyDisconnectAddress));

        setState('connected');
        if (connectCallback) {
          connectCallback(void 0, id);
          connectCallback = null;
        }

        if (onOpen) onOpen(id);

        keepAliveTimer = setTimeout(function() {
          lastMessageTimestamp = OT.$.now();
          sendKeepAlive();
        }, WEB_SOCKET_KEEP_ALIVE_INTERVAL);
      },

      webSocketConnectTimedOut = function webSocketConnectTimedOut() {
        var webSocketWas = webSocket;
        raiseError(errorMap.CLOSE_TIMEOUT);
        // This will prevent a socket eventually connecting
        // But call it _after_ the error just in case any of
        // the callbacks fire synchronously, breaking the error
        // handling code.
        try {
          webSocketWas.close();
        } catch (x) {}
      },

      webSocketError = function webSocketError() {},
        // var errorMessage = 'Unknown Socket Error';
        // @fixme We MUST be able to do better than this!

        // All errors seem to result in disconnecting the socket, the close event
        // has a close reason and code which gives some error context. This,
        // combined with the fact that the errorEvent argument contains no
        // error info at all, means we'll delay triggering the error handlers
        // until the socket is closed.
        // error(errorMessage);

      webSocketDisconnected = function webSocketDisconnected(closeEvent) {
        if (connectTimeout) clearTimeout(connectTimeout);
        if (keepAliveTimer) clearTimeout(keepAliveTimer);

        if (isDOMUnloaded()) {
          // Sometimes we receive the web socket close event after
          // the DOM has already been partially or fully unloaded
          // if that's the case here then it's not really safe, or
          // desirable, to continue.
          return;
        }

        if (closeEvent.code !== 1000 && closeEvent.code !== 1001) {
          if (closeEvent.code) {
            raiseError(closeEvent.code);
          } else {
            raiseError(
              errorMap.CLOSE_FALLBACK_CODE,
              closeEvent.reason || closeEvent.message
            );
          }
        }

        if (_this.isNot('error')) setState('disconnected');
      },

      webSocketReceivedMessage = function webSocketReceivedMessage(msg) {
        lastMessageTimestamp = OT.$.now();

        if (onMessage) {
          if (msg.type !== OT.Rumor.MessageType.PONG) {
            onMessage(msg);
          }
        }
      };

  //// Public API

  this.publish = function(topics, message, headers) {
    webSocket.send(OT.Rumor.Message.Publish(topics, message, headers));
  };

  this.subscribe = function(topics) {
    webSocket.send(OT.Rumor.Message.Subscribe(topics));
  };

  this.unsubscribe = function(topics) {
    webSocket.send(OT.Rumor.Message.Unsubscribe(topics));
  };

  this.connect = function(connectionId, complete) {
    if (this.is('connecting', 'connected')) {
      complete(new OT.Rumor.SocketError(null,
          'Rumor.Socket cannot connect when it is already connecting or connected.'));
      return;
    }

    id = connectionId;
    connectCallback = complete;

    setState('connecting');

    var TheWebSocket = NativeSocket || window.WebSocket;

    var events = {
      onOpen:    webSocketConnected,
      onClose:   webSocketDisconnected,
      onError:   webSocketError,
      onMessage: webSocketReceivedMessage
    };

    try {
      if (typeof TheWebSocket !== 'undefined') {
        webSocket = new OT.Rumor.NativeSocket(TheWebSocket, messagingURL, events);
      } else {
        webSocket = new OT.Rumor.PluginSocket(messagingURL, events);
      }

      connectTimeout = setTimeout(webSocketConnectTimedOut, OT.Rumor.Socket.CONNECT_TIMEOUT);
    } catch (e) {
      OT.error(e);

      raiseError(errorMap.CLOSE_CONNECT_EXCEPTION);
    }
  };

  this.disconnect = function(drainSocketBuffer) {
    if (connectTimeout) clearTimeout(connectTimeout);
    if (keepAliveTimer) clearTimeout(keepAliveTimer);

    if (!webSocket) {
      if (this.isNot('error')) setState('disconnected');
      return;
    }

    if (webSocket.isClosed()) {
      if (this.isNot('error')) setState('disconnected');
    } else {
      if (this.is('connected')) {
        // Look! We are nice to the rumor server ;-)
        webSocket.send(OT.Rumor.Message.Disconnect());
      }

      // Wait until the socket is ready to close
      webSocket.close(drainSocketBuffer);
    }
  };

  OT.$.defineProperties(this, {
    id: {
      get: function() { return id; }
    },

    onOpen: {
      set: function(callback) {
        validateCallback('onOpen', callback);
        onOpen = callback;
      },

      get: function() { return onOpen; }
    },

    onError: {
      set: function(callback) {
        validateCallback('onError', callback);
        onError = callback;
      },

      get: function() { return onError; }
    },

    onClose: {
      set: function(callback) {
        validateCallback('onClose', callback);
        onClose = callback;
      },

      get: function() { return onClose; }
    },

    onMessage: {
      set: function(callback) {
        validateCallback('onMessage', callback);
        onMessage = callback;
      },

      get: function() { return onMessage; }
    }
  });
};

// The number of ms to wait for the websocket to connect
OT.Rumor.Socket.CONNECT_TIMEOUT = 15000;

// tb_require('../../../helpers/helpers.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

// Rumor Messaging for JS
//
// https://tbwiki.tokbox.com/index.php/Raptor_Messages_(Sent_as_a_RumorMessage_payload_in_JSON)
//
// @todo Raptor {
//     Look at disconnection cleanup: i.e. subscriber + publisher cleanup
//     Add error codes for all the error cases
//     Write unit tests for SessionInfo
//     Write unit tests for Session
//     Make use of the new DestroyedEvent
//     Remove dependency on OT.properties
//     OT.Capabilities must be part of the Raptor namespace
//     Add Dependability commands
//     Think about noConflict, or whether we should just use the OT namespace
//     Think about how to expose OT.publishers, OT.subscribers, and OT.sessions if messaging was
//        being included as a component
//     Another solution to the problem of having publishers/subscribers/etc would be to make
//        Raptor Socket a separate component from Dispatch (dispatch being more business logic)
//     Look at the coupling of OT.sessions to OT.Raptor.Socket
// }
//
// @todo Raptor Docs {
//   Document payload formats for incoming messages (what are the payloads for
//    STREAM CREATED/MODIFIED for example)
//   Document how keepalives work
//   Document all the Raptor actions and types
//   Document the session connect flow (including error cases)
// }

OT.Raptor = {
  Actions: {
    //General
    CONNECT: 100,
    CREATE: 101,
    UPDATE: 102,
    DELETE: 103,
    STATE: 104,

    //Moderation
    FORCE_DISCONNECT: 105,
    FORCE_UNPUBLISH: 106,
    SIGNAL: 107,

    //Archives
    CREATE_ARCHIVE: 108,
    CLOSE_ARCHIVE: 109,
    START_RECORDING_SESSION: 110,
    STOP_RECORDING_SESSION: 111,
    START_RECORDING_STREAM: 112,
    STOP_RECORDING_STREAM: 113,
    LOAD_ARCHIVE: 114,
    START_PLAYBACK: 115,
    STOP_PLAYBACK: 116,

    //AppState
    APPSTATE_PUT: 117,
    APPSTATE_DELETE: 118,

    // JSEP
    OFFER: 119,
    ANSWER: 120,
    PRANSWER: 121,
    CANDIDATE: 122,
    SUBSCRIBE: 123,
    UNSUBSCRIBE: 124,
    QUERY: 125,
    SDP_ANSWER: 126,

    //KeepAlive
    PONG: 127,
    REGISTER: 128, //Used for registering streams.

    QUALITY_CHANGED: 129
  },

  Types: {
    //RPC
    RPC_REQUEST: 100,
    RPC_RESPONSE: 101,

    //EVENT
    STREAM: 102,
    ARCHIVE: 103,
    CONNECTION: 104,
    APPSTATE: 105,
    CONNECTIONCOUNT: 106,
    MODERATION: 107,
    SIGNAL: 108,
    SUBSCRIBER: 110,

    //JSEP Protocol
    JSEP: 109
  }
};

// tb_require('../../../helpers/helpers.js')
// tb_require('./raptor.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

OT.Raptor.serializeMessage = function(message) {
  return JSON.stringify(message);
};

// Deserialising a Raptor message mainly means doing a JSON.parse on it.
// We do decorate the final message with a few extra helper properies though.
//
// These include:
// * typeName: A human readable version of the Raptor type. E.g. STREAM instead of 102
// * actionName: A human readable version of the Raptor action. E.g. CREATE instead of 101
// * signature: typeName and actionName combined. This is mainly for debugging. E.g. A type
//    of 102 and an action of 101 would result in a signature of "STREAM:CREATE"
//
OT.Raptor.deserializeMessage = function(msg) {
  if (msg.length === 0) return {};

  var message = JSON.parse(msg),
      bits = message.uri.substr(1).split('/');

  // Remove the Raptor protocol version
  bits.shift();
  if (bits[bits.length - 1] === '') bits.pop();

  message.params = {};
  for (var i = 0, numBits = bits.length ; i < numBits - 1; i += 2) {
    message.params[bits[i]] = bits[i + 1];
  }

  // extract the resource name. We special case 'channel' slightly, as
  // 'subscriber_channel' or 'stream_channel' is more useful for us
  // than 'channel' alone.
  if (bits.length % 2 === 0) {
    if (bits[bits.length - 2] === 'channel' && bits.length > 6) {
      message.resource = bits[bits.length - 4] + '_' + bits[bits.length - 2];
    } else {
      message.resource = bits[bits.length - 2];
    }
  } else {
    if (bits[bits.length - 1] === 'channel' && bits.length > 5) {
      message.resource = bits[bits.length - 3] + '_' + bits[bits.length - 1];
    } else {
      message.resource = bits[bits.length - 1];
    }
  }

  message.signature = message.resource + '#' + message.method;
  return message;
};

OT.Raptor.unboxFromRumorMessage = function(rumorMessage) {
  var message = OT.Raptor.deserializeMessage(rumorMessage.data);
  message.transactionId = rumorMessage.transactionId;
  message.fromAddress = rumorMessage.headers['X-TB-FROM-ADDRESS'];

  return message;
};

OT.Raptor.parseIceServers = function(message) {
  try {
    return JSON.parse(message.data).content.iceServers;
  } catch (e) {
    return [];
  }
};

OT.Raptor.Message = {};

OT.Raptor.Message.offer = function(uri, offerSdp) {
  return OT.Raptor.serializeMessage({
    method: 'offer',
    uri: uri,
    content: {
      sdp: offerSdp
    }
  });
};

OT.Raptor.Message.connections = {};

OT.Raptor.Message.connections.create = function(apiKey, sessionId, connectionId) {
  return OT.Raptor.serializeMessage({
    method: 'create',
    uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/connection/' + connectionId,
    content: {
      userAgent: OT.$.env.userAgent
    }
  });
};

OT.Raptor.Message.connections.destroy = function(apiKey, sessionId, connectionId) {
  return OT.Raptor.serializeMessage({
    method: 'delete',
    uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/connection/' + connectionId,
    content: {}
  });
};

OT.Raptor.Message.sessions = {};

OT.Raptor.Message.sessions.get = function(apiKey, sessionId) {
  return OT.Raptor.serializeMessage({
    method: 'read',
    uri: '/v2/partner/' + apiKey + '/session/' + sessionId,
    content: {}
  });
};

OT.Raptor.Message.streams = {};

OT.Raptor.Message.streams.get = function(apiKey, sessionId, streamId) {
  return OT.Raptor.serializeMessage({
    method: 'read',
    uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/stream/' + streamId,
    content: {}
  });
};

OT.Raptor.Message.streams.channelFromOTChannel = function(channel) {
  var raptorChannel = {
    id: channel.id,
    type: channel.type,
    active: channel.active
  };

  if (channel.type === 'video') {
    raptorChannel.width = channel.width;
    raptorChannel.height = channel.height;
    raptorChannel.orientation = channel.orientation;
    raptorChannel.frameRate = channel.frameRate;
    if (channel.source !== 'default') {
      raptorChannel.source = channel.source;
    }
    raptorChannel.fitMode = channel.fitMode;
  }

  return raptorChannel;
};

OT.Raptor.Message.streams.create = function(apiKey, sessionId, streamId, name,
  audioFallbackEnabled, channels, minBitrate, maxBitrate) {
  var messageContent = {
    id: streamId,
    name: name,
    audioFallbackEnabled: audioFallbackEnabled,
    channel: OT.$.map(channels, function(channel) {
      return OT.Raptor.Message.streams.channelFromOTChannel(channel);
    })
  };

  if (minBitrate) messageContent.minBitrate = minBitrate;
  if (maxBitrate) messageContent.maxBitrate = maxBitrate;

  return OT.Raptor.serializeMessage({
    method: 'create',
    uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/stream/' + streamId,
    content: messageContent
  });
};

OT.Raptor.Message.streams.destroy = function(apiKey, sessionId, streamId) {
  return OT.Raptor.serializeMessage({
    method: 'delete',
    uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/stream/' + streamId,
    content: {}
  });
};

OT.Raptor.Message.streams.answer = function(apiKey, sessionId, streamId, answerSdp) {
  return OT.Raptor.serializeMessage({
    method: 'answer',
    uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/stream/' + streamId,
    content: {
      sdp: answerSdp
    }
  });
};

OT.Raptor.Message.streams.candidate = function(apiKey, sessionId, streamId, candidate) {
  return OT.Raptor.serializeMessage({
    method: 'candidate',
    uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/stream/' + streamId,
    content: candidate
  });
};

OT.Raptor.Message.streamChannels = {};
OT.Raptor.Message.streamChannels.update =
  function(apiKey, sessionId, streamId, channelId, attributes) {
  return OT.Raptor.serializeMessage({
    method: 'update',
    uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/stream/' +
      streamId + '/channel/' + channelId,
    content: attributes
  });
};

OT.Raptor.Message.subscribers = {};

OT.Raptor.Message.subscribers.create =
  function(apiKey, sessionId, streamId, subscriberId, connectionId, channelsToSubscribeTo) {
  var content = {
    id: subscriberId,
    connection: connectionId,
    keyManagementMethod: OT.$.supportedCryptoScheme(),
    bundleSupport: OT.$.hasCapabilities('bundle'),
    rtcpMuxSupport: OT.$.hasCapabilities('RTCPMux')
  };
  if (channelsToSubscribeTo) content.channel = channelsToSubscribeTo;

  return OT.Raptor.serializeMessage({
    method: 'create',
    uri: '/v2/partner/' + apiKey + '/session/' + sessionId +
      '/stream/' + streamId + '/subscriber/' + subscriberId,
    content: content
  });
};

OT.Raptor.Message.subscribers.destroy = function(apiKey, sessionId, streamId, subscriberId) {
  return OT.Raptor.serializeMessage({
    method: 'delete',
    uri: '/v2/partner/' + apiKey + '/session/' + sessionId +
      '/stream/' + streamId + '/subscriber/' + subscriberId,
    content: {}
  });
};

OT.Raptor.Message.subscribers.update =
  function(apiKey, sessionId, streamId, subscriberId, attributes) {
  return OT.Raptor.serializeMessage({
    method: 'update',
    uri: '/v2/partner/' + apiKey + '/session/' + sessionId +
    '/stream/' + streamId + '/subscriber/' + subscriberId,
    content: attributes
  });
};

OT.Raptor.Message.subscribers.candidate =
  function(apiKey, sessionId, streamId, subscriberId, candidate) {
  return OT.Raptor.serializeMessage({
    method: 'candidate',
    uri: '/v2/partner/' + apiKey + '/session/' + sessionId +
      '/stream/' + streamId + '/subscriber/' + subscriberId,
    content: candidate
  });
};

OT.Raptor.Message.subscribers.answer =
  function(apiKey, sessionId, streamId, subscriberId, answerSdp) {
  return OT.Raptor.serializeMessage({
    method: 'answer',
    uri: '/v2/partner/' + apiKey + '/session/' + sessionId +
    '/stream/' + streamId + '/subscriber/' + subscriberId,
    content: {
      sdp: answerSdp
    }
  });
};

OT.Raptor.Message.subscriberChannels = {};

OT.Raptor.Message.subscriberChannels.update =
  function(apiKey, sessionId, streamId, subscriberId, channelId, attributes) {
  return OT.Raptor.serializeMessage({
    method: 'update',
    uri: '/v2/partner/' + apiKey + '/session/' + sessionId +
    '/stream/' + streamId + '/subscriber/' + subscriberId + '/channel/' + channelId,
    content: attributes
  });
};

OT.Raptor.Message.signals = {};

OT.Raptor.Message.signals.create = function(apiKey, sessionId, toAddress, type, data) {
  var content = {};
  if (type !== void 0) content.type = type;
  if (data !== void 0) content.data = data;

  return OT.Raptor.serializeMessage({
    method: 'signal',
    uri: '/v2/partner/' + apiKey + '/session/' + sessionId +
      (toAddress !== void 0 ? '/connection/' + toAddress : '') + '/signal/' + OT.$.uuid(),
    content: content
  });
};

// tb_require('../../../helpers/helpers.js')
// tb_require('./message.js')

!(function() {
  /* jshint globalstrict: true, strict: false, undef: true, unused: true,
            trailing: true, browser: true, smarttabs:true */
  /* global OT */

  // Connect error codes and reasons that Raptor can return.
  var connectErrorReasons;

  connectErrorReasons = {
    409: 'This P2P session already has 2 participants.',
    410: 'The session already has four participants.',
    1004: 'The token passed is invalid.'
  };

  OT.Raptor.Dispatcher = function() {
    OT.$.eventing(this, true);
    this.callbacks = {};
  };

  OT.Raptor.Dispatcher.prototype.registerCallback = function(transactionId, completion) {
    this.callbacks[transactionId] = completion;
  };

  OT.Raptor.Dispatcher.prototype.triggerCallback = function(transactionId) {
    /*, arg1, arg2, argN-1, argN*/
    if (!transactionId) return;

    var completion = this.callbacks[transactionId];

    if (completion && OT.$.isFunction(completion)) {
      var args = Array.prototype.slice.call(arguments);
      args.shift();

      completion.apply(null, args);
    }

    delete this.callbacks[transactionId];
  };

  OT.Raptor.Dispatcher.prototype.onClose = function(reason) {
    this.emit('close', reason);
  };

  OT.Raptor.Dispatcher.prototype.dispatch = function(rumorMessage) {
    // The special casing of STATUS messages is ugly. Need to think about
    // how to better integrate this.

    if (rumorMessage.type === OT.Rumor.MessageType.STATUS) {
      OT.debug('OT.Raptor.dispatch: STATUS');
      OT.debug(rumorMessage);

      var error;

      if (rumorMessage.isError) {
        error = new OT.Error(rumorMessage.status);
      }

      this.triggerCallback(rumorMessage.transactionId, error, rumorMessage);

      return;
    }

    var message = OT.Raptor.unboxFromRumorMessage(rumorMessage);
    OT.debug('OT.Raptor.dispatch ' + message.signature);
    OT.debug(rumorMessage.data);

    switch (message.resource) {
      case 'session':
        this.dispatchSession(message);
        break;

      case 'connection':
        this.dispatchConnection(message);
        break;

      case 'stream':
        this.dispatchStream(message);
        break;

      case 'stream_channel':
        this.dispatchStreamChannel(message);
        break;

      case 'subscriber':
        this.dispatchSubscriber(message);
        break;

      case 'subscriber_channel':
        this.dispatchSubscriberChannel(message);
        break;

      case 'signal':
        this.dispatchSignal(message);
        break;

      case 'archive':
        this.dispatchArchive(message);
        break;

      default:
        OT.warn('OT.Raptor.dispatch: Type ' + message.resource + ' is not currently implemented');
    }
  };

  OT.Raptor.Dispatcher.prototype.dispatchSession = function(message) {
    switch (message.method) {
      case 'read':
        this.emit('session#read', message.content, message.transactionId);
        break;

      default:
        OT.warn('OT.Raptor.dispatch: ' + message.signature + ' is not currently implemented');
    }
  };

  OT.Raptor.Dispatcher.prototype.dispatchConnection = function(message) {

    switch (message.method) {
      case 'created':
        this.emit('connection#created', message.content);
        break;

      case 'deleted':
        this.emit('connection#deleted', message.params.connection, message.reason);
        break;

      default:
        OT.warn('OT.Raptor.dispatch: ' + message.signature + ' is not currently implemented');
    }
  };

  OT.Raptor.Dispatcher.prototype.dispatchStream = function(message) {

    switch (message.method) {
      case 'created':
        this.emit('stream#created', message.content, message.transactionId);
        break;

      case 'deleted':
        this.emit('stream#deleted', message.params.stream,
          message.reason);
        break;

      case 'updated':
        this.emit('stream#updated', message.params.stream,
          message.content);
        break;

      // The JSEP process
      case 'generateoffer':
      case 'answer':
      case 'pranswer':
      case 'offer':
      case 'candidate':
        this.dispatchJsep(message.method, message);
        break;

      default:
        OT.warn('OT.Raptor.dispatch: ' + message.signature + ' is not currently implemented');
    }
  };

  OT.Raptor.Dispatcher.prototype.dispatchStreamChannel = function(message) {
    switch (message.method) {
      case 'updated':
        this.emit('streamChannel#updated', message.params.stream,
          message.params.channel, message.content);
        break;

      default:
        OT.warn('OT.Raptor.dispatch: ' + message.signature + ' is not currently implemented');
    }
  };

  // Dispatch JSEP messages
  //
  // generateoffer:
  // Request to generate a offer for another Peer (or Prism). This kicks
  // off the JSEP process.
  //
  // answer:
  // generate a response to another peers offer, this contains our constraints
  // and requirements.
  //
  // pranswer:
  // a provisional answer, i.e. not the final one.
  //
  // candidate
  //
  //
  OT.Raptor.Dispatcher.prototype.dispatchJsep = function(method, message) {
    this.emit('jsep#' + method, message.params.stream, message.fromAddress, message);
  };

  OT.Raptor.Dispatcher.prototype.dispatchSubscriberChannel = function(message) {
    switch (message.method) {
      case 'updated':
        this.emit('subscriberChannel#updated', message.params.stream,
          message.params.channel, message.content);
        break;

      case 'update': // subscriberId, streamId, content
        this.emit('subscriberChannel#update', message.params.subscriber,
          message.params.stream, message.content);
        break;

      default:
        OT.warn('OT.Raptor.dispatch: ' + message.signature + ' is not currently implemented');
    }
  };

  OT.Raptor.Dispatcher.prototype.dispatchSubscriber = function(message) {
    switch (message.method) {
      case 'created':
        this.emit('subscriber#created', message.params.stream, message.fromAddress,
          message.content.id);
        break;

      case 'deleted':
        this.dispatchJsep('unsubscribe', message);
        this.emit('subscriber#deleted', message.params.stream,
          message.fromAddress);
        break;

      // The JSEP process
      case 'generateoffer':
      case 'answer':
      case 'pranswer':
      case 'offer':
      case 'candidate':
        this.dispatchJsep(message.method, message);
        break;

      default:
        OT.warn('OT.Raptor.dispatch: ' + message.signature + ' is not currently implemented');
    }
  };

  OT.Raptor.Dispatcher.prototype.dispatchSignal = function(message) {
    if (message.method !== 'signal') {
      OT.warn('OT.Raptor.dispatch: ' + message.signature + ' is not currently implemented');
      return;
    }
    this.emit('signal', message.fromAddress, message.content.type,
      message.content.data);
  };

  OT.Raptor.Dispatcher.prototype.dispatchArchive = function(message) {
    switch (message.method) {
      case 'created':
        this.emit('archive#created', message.content);
        break;

      case 'updated':
        this.emit('archive#updated', message.params.archive, message.content);
        break;
    }
  };

}(this));

// tb_require('../../../helpers/helpers.js')
// tb_require('./message.js')
// tb_require('./dispatch.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

(function(window) {

  // @todo hide these
  OT.publishers = new OT.$.Collection('guid');          // Publishers are id'd by their guid
  OT.subscribers = new OT.$.Collection('widgetId');     // Subscribers are id'd by their widgetId
  OT.sessions = new OT.$.Collection();

  function parseStream(dict, session) {
    var channel = dict.channel.map(function(channel) {
      return new OT.StreamChannel(channel);
    });

    var connectionId = dict.connectionId ? dict.connectionId : dict.connection.id;

    return new OT.Stream(dict.id,
                         dict.name,
                         dict.creationTime,
                         session.connections.get(connectionId),
                         session,
                         channel);
  }

  function parseAndAddStreamToSession(dict, session) {
    if (session.streams.has(dict.id)) return;

    var stream = parseStream(dict, session);
    session.streams.add(stream);

    return stream;
  }

  function parseArchive(dict) {
    return new OT.Archive(dict.id,
                          dict.name,
                          dict.status);
  }

  function parseAndAddArchiveToSession(dict, session) {
    if (session.archives.has(dict.id)) return;

    var archive = parseArchive(dict);
    session.archives.add(archive);

    return archive;
  }

  var DelayedEventQueue = function DelayedEventQueue(eventDispatcher) {
    var queue = [];

    this.enqueue = function enqueue(/* arg1, arg2, ..., argN */) {
      queue.push(Array.prototype.slice.call(arguments));
    };

    this.triggerAll = function triggerAll() {
      var event;

      // Array.prototype.shift is actually pretty inefficient for longer Arrays,
      // this is because after the first element is removed it reshuffles every
      // remaining element up one (1). This involves way too many allocations and
      // deallocations as the queue size increases.
      //
      // A more efficient version could be written by keeping an index to the current
      // 'first' element in the Array and increasing that by one whenever an element
      // is removed. The elements that are infront of the index have been 'deleted'.
      // Periodically the front of the Array could be spliced off to reclaim the space.
      //
      // 1. http://www.ecma-international.org/ecma-262/5.1/#sec-15.4.4.9
      //
      //
      // TLDR: Array.prototype.shift is O(n), where n is the array length,
      // instead of the expected O(1). You can implement your own shift that runs
      // in amortised constant time.
      //
      // @todo benchmark and see if we should actually care about shift's performance
      // for our common queue sizes.
      //
      while ((event = queue.shift())) {
        eventDispatcher.trigger.apply(eventDispatcher, event);
      }
    };
  };

  var DelayedSessionEvents = function(dispatcher) {
    var eventQueues = {};

    this.enqueue = function enqueue(/* key, arg1, arg2, ..., argN */) {
      var key = arguments[0];
      var eventArgs = Array.prototype.slice.call(arguments, 1);
      if (!eventQueues[key]) {
        eventQueues[key] = new DelayedEventQueue(dispatcher);
      }
      eventQueues[key].enqueue.apply(eventQueues[key], eventArgs);
    };

    this.triggerConnectionCreated = function triggerConnectionCreated(connection) {
      if (eventQueues['connectionCreated' + connection.id]) {
        eventQueues['connectionCreated' + connection.id].triggerAll();
      }
    };

    this.triggerSessionConnected = function triggerSessionConnected(connections) {
      if (eventQueues.sessionConnected) {
        eventQueues.sessionConnected.triggerAll();
      }

      OT.$.forEach(connections, function(connection) {
        this.triggerConnectionCreated(connection);
      });
    };
  };

  var unconnectedStreams = {};

  window.OT.SessionDispatcher = function(session) {

    var dispatcher = new OT.Raptor.Dispatcher(),
        sessionStateReceived = false,
        delayedSessionEvents = new DelayedSessionEvents(dispatcher);

    dispatcher.on('close', function(reason) {

      var connection = session.connection;

      if (!connection) {
        return;
      }

      if (connection.destroyedReason()) {
        OT.debug('OT.Raptor.Socket: Socket was closed but the connection had already ' +
          'been destroyed. Reason: ' + connection.destroyedReason());
        return;
      }

      connection.destroy(reason);
    });
    
    // This method adds connections to the session both on a connection#created and
    // on a session#read. In the case of session#read sessionRead is set to true and
    // we include our own connection.
    var addConnection = function(connection, sessionRead) {
      connection = OT.Connection.fromHash(connection);
      if (sessionRead || session.connection && connection.id !== session.connection.id) {
        session.connections.add(connection);
        delayedSessionEvents.triggerConnectionCreated(connection);
      }

      OT.$.forEach(OT.$.keys(unconnectedStreams), function(streamId) {
        var stream = unconnectedStreams[streamId];
        if (stream && connection.id === stream.connection.id) {
          // dispatch streamCreated event now that the connectionCreated has been dispatched
          parseAndAddStreamToSession(stream, session);
          delete unconnectedStreams[stream.id];
          
          var payload = {
            debug: sessionRead ? 'connection came in session#read' :
              'connection came in connection#created',
            streamId: stream.id,
            connectionId: connection.id
          };
          session.logEvent('streamCreated', 'warning', payload);
        }
      });
      
      return connection;
    };

    dispatcher.on('session#read', function(content, transactionId) {

      var state = {},
          connection;

      state.streams = [];
      state.connections = [];
      state.archives = [];

      OT.$.forEach(content.connection, function(connectionParams) {
        connection = addConnection(connectionParams, true);
        state.connections.push(connection);
      });

      OT.$.forEach(content.stream, function(streamParams) {
        state.streams.push(parseAndAddStreamToSession(streamParams, session));
      });

      OT.$.forEach(content.archive || content.archives, function(archiveParams) {
        state.archives.push(parseAndAddArchiveToSession(archiveParams, session));
      });

      session._.subscriberMap = {};

      dispatcher.triggerCallback(transactionId, null, state);

      sessionStateReceived = true;
      delayedSessionEvents.triggerSessionConnected(session.connections);
    });

    dispatcher.on('connection#created', function(connection) {
      addConnection(connection);
    });

    dispatcher.on('connection#deleted', function(connection, reason) {
      connection = session.connections.get(connection);
      connection.destroy(reason);
    });

    dispatcher.on('stream#created', function(stream, transactionId) {
      var connectionId = stream.connectionId ? stream.connectionId : stream.connection.id;
      if (session.connections.has(connectionId)) {
        stream = parseAndAddStreamToSession(stream, session);
      } else {
        unconnectedStreams[stream.id] = stream;

        var payload = {
          debug: 'eventOrderError -- streamCreated event before connectionCreated',
          streamId: stream.id
        };
        session.logEvent('streamCreated', 'warning', payload);
      }

      if (stream.publisher) {
        stream.publisher.setStream(stream);
      }

      dispatcher.triggerCallback(transactionId, null, stream);
    });

    dispatcher.on('stream#deleted', function(streamId, reason) {
      var stream = session.streams.get(streamId);

      if (!stream) {
        OT.error('OT.Raptor.dispatch: A stream does not exist with the id of ' +
          streamId + ', for stream#deleted message!');
        // @todo error
        return;
      }

      stream.destroy(reason);
    });

    dispatcher.on('stream#updated', function(streamId, content) {
      var stream = session.streams.get(streamId);

      if (!stream) {
        OT.error('OT.Raptor.dispatch: A stream does not exist with the id of ' +
          streamId + ', for stream#updated message!');
        // @todo error
        return;
      }

      stream._.update(content);

    });

    dispatcher.on('streamChannel#updated', function(streamId, channelId, content) {
      var stream;
      if (!(streamId && (stream = session.streams.get(streamId)))) {
        OT.error('OT.Raptor.dispatch: Unable to determine streamId, or the stream does not ' +
          'exist, for streamChannel message!');
        // @todo error
        return;
      }
      stream._.updateChannel(channelId, content);
    });

    // Dispatch JSEP messages
    //
    // generateoffer:
    // Request to generate a offer for another Peer (or Prism). This kicks
    // off the JSEP process.
    //
    // answer:
    // generate a response to another peers offer, this contains our constraints
    // and requirements.
    //
    // pranswer:
    // a provisional answer, i.e. not the final one.
    //
    // candidate
    //
    //
    var jsepHandler = function(method, streamId, fromAddress, message) {

      var fromConnection,
          actors;

      switch (method) {
        // Messages for Subscribers
        case 'offer':
          actors = [];
          var subscriber = OT.subscribers.find({streamId: streamId});
          if (subscriber) actors.push(subscriber);
          break;

        // Messages for Publishers
        case 'answer':
        case 'pranswer':
        case 'generateoffer':
        case 'unsubscribe':
          actors = OT.publishers.where({streamId: streamId});
          break;

        // Messages for Publishers and Subscribers
        case 'candidate':
          // send to whichever of your publisher or subscribers are
          // subscribing/publishing that stream
          actors = OT.publishers.where({streamId: streamId})
            .concat(OT.subscribers.where({streamId: streamId}));
          break;

        default:
          OT.warn('OT.Raptor.dispatch: jsep#' + method +
            ' is not currently implemented');
          return;
      }

      if (actors.length === 0) return;

      // This is a bit hacky. We don't have the session in the message so we iterate
      // until we find the actor that the message relates to this stream, and then
      // we grab the session from it.
      fromConnection = actors[0].session.connections.get(fromAddress);
      if (!fromConnection && fromAddress.match(/^symphony\./)) {
        fromConnection = OT.Connection.fromHash({
          id: fromAddress,
          creationTime: Math.floor(OT.$.now())
        });

        actors[0].session.connections.add(fromConnection);
      } else if (!fromConnection) {
        OT.warn('OT.Raptor.dispatch: Messsage comes from a connection (' +
          fromAddress + ') that we do not know about. The message was ignored.');
        return;
      }

      OT.$.forEach(actors, function(actor) {
        actor.processMessage(method, fromConnection, message);
      });
    };

    dispatcher.on('jsep#offer', OT.$.bind(jsepHandler, null, 'offer'));
    dispatcher.on('jsep#answer', OT.$.bind(jsepHandler, null, 'answer'));
    dispatcher.on('jsep#pranswer', OT.$.bind(jsepHandler, null, 'pranswer'));
    dispatcher.on('jsep#generateoffer', OT.$.bind(jsepHandler, null, 'generateoffer'));
    dispatcher.on('jsep#unsubscribe', OT.$.bind(jsepHandler, null, 'unsubscribe'));
    dispatcher.on('jsep#candidate', OT.$.bind(jsepHandler, null, 'candidate'));

    dispatcher.on('subscriberChannel#updated', function(streamId, channelId, content) {

      if (!streamId || !session.streams.has(streamId)) {
        OT.error('OT.Raptor.dispatch: Unable to determine streamId, or the stream does not ' +
          'exist, for subscriberChannel#updated message!');
        // @todo error
        return;
      }

      session.streams.get(streamId)._
        .updateChannel(channelId, content);

    });

    dispatcher.on('subscriberChannel#update', function(subscriberId, streamId, content) {

      if (!streamId || !session.streams.has(streamId)) {
        OT.error('OT.Raptor.dispatch: Unable to determine streamId, or the stream does not ' +
          'exist, for subscriberChannel#update message!');
        // @todo error
        return;
      }

      // Hint to update for congestion control from the Media Server
      if (!OT.subscribers.has(subscriberId)) {
        OT.error('OT.Raptor.dispatch: Unable to determine subscriberId, or the subscriber ' +
          'does not exist, for subscriberChannel#update message!');
        // @todo error
        return;
      }

      // We assume that an update on a Subscriber channel is to disableVideo
      // we may need to be more specific in the future
      OT.subscribers.get(subscriberId).disableVideo(content.active);

    });

    dispatcher.on('subscriber#created', function(streamId, fromAddress, subscriberId) {

      var stream = streamId ? session.streams.get(streamId) : null;

      if (!stream) {
        OT.error('OT.Raptor.dispatch: Unable to determine streamId, or the stream does ' +
          'not exist, for subscriber#created message!');
        // @todo error
        return;
      }

      session._.subscriberMap[fromAddress + '_' + stream.id] = subscriberId;
    });

    dispatcher.on('subscriber#deleted', function(streamId, fromAddress) {
      var stream = streamId ? session.streams.get(streamId) : null;

      if (!stream) {
        OT.error('OT.Raptor.dispatch: Unable to determine streamId, or the stream does ' +
          'not exist, for subscriber#created message!');
        // @todo error
        return;
      }

      delete session._.subscriberMap[fromAddress + '_' + stream.id];
    });

    dispatcher.on('signal', function(fromAddress, signalType, data) {
      var fromConnection = session.connections.get(fromAddress);
      if (session.connection && fromAddress === session.connection.connectionId) {
        if (sessionStateReceived) {
          session._.dispatchSignal(fromConnection, signalType, data);
        } else {
          delayedSessionEvents.enqueue('sessionConnected',
            'signal', fromAddress, signalType, data);
        }
      } else {
        if (session.connections.get(fromAddress)) {
          session._.dispatchSignal(fromConnection, signalType, data);
        } else {
          delayedSessionEvents.enqueue('connectionCreated' + fromAddress,
            'signal', fromAddress, signalType, data);
        }
      }
    });

    dispatcher.on('archive#created', function(archive) {
      parseAndAddArchiveToSession(archive, session);
    });

    dispatcher.on('archive#updated', function(archiveId, update) {
      var archive = session.archives.get(archiveId);

      if (!archive) {
        OT.error('OT.Raptor.dispatch: An archive does not exist with the id of ' +
          archiveId + ', for archive#updated message!');
        // @todo error
        return;
      }

      archive._.update(update);
    });

    return dispatcher;

  };

})(window);

// tb_require('../../helpers/helpers.js')

/* global OT */

function httpTest(config) {

  var _httpConfig = config.httpConfig;

  function measureDownloadBandwidth(url) {

    var xhr = new XMLHttpRequest(),
      resultPromise = new OT.$.RSVP.Promise(function(resolve, reject) {

        var startTs = Date.now(), progressLoaded = 0;

        function calculate(loaded) {
          return 1000 * 8 * loaded / (Date.now() - startTs);
        }

        xhr.addEventListener('load', function(evt) {
          resolve(calculate(evt.loaded));
        });
        xhr.addEventListener('abort', function() {
          resolve(calculate(progressLoaded));
        });
        xhr.addEventListener('error', function(evt) {
          reject(evt);
        });

        xhr.addEventListener('progress', function(evt) {
          progressLoaded = evt.loaded;
        });

        xhr.open('GET', url + '?_' + Math.random());
        xhr.send();
      });

    return {
      promise: resultPromise,
      abort: function() {
        xhr.abort();
      }
    };
  }

  /**
   * Measures the bandwidth in bps.
   *
   * @param {string} url
   * @param {ArrayBuffer} payload
   * @returns {{promise: Promise, abort: function}}
   */
  function measureUploadBandwidth(url, payload) {
    var xhr = new XMLHttpRequest(),
      resultPromise = new OT.$.RSVP.Promise(function(resolve, reject) {

        var startTs,
          lastTs,
          lastLoaded;
        xhr.upload.addEventListener('progress', function(evt) {
          if (!startTs) {
            startTs = Date.now();
          }
          lastLoaded = evt.loaded;
        });
        xhr.addEventListener('load', function() {
          lastTs = Date.now();
          resolve(1000 * 8 * lastLoaded / (lastTs - startTs));
        });
        xhr.addEventListener('error', function(e) {
          reject(e);
        });
        xhr.addEventListener('abort', function() {
          reject();
        });
        xhr.open('POST', url);
        xhr.send(payload);
      });

    return {
      promise: resultPromise,
      abort: function() {
        xhr.abort();
      }
    };
  }

  function doDownload(url, maxDuration) {
    var measureResult = measureDownloadBandwidth(url);

    setTimeout(function() {
      measureResult.abort();
    }, maxDuration);

    return measureResult.promise;
  }

  function loopUpload(url, initialSize, maxDuration) {
    return new OT.$.RSVP.Promise(function(resolve) {
      var lastMeasureResult,
        lastBandwidth = 0;

      setTimeout(function() {
        lastMeasureResult.abort();
        resolve(lastBandwidth);

      }, maxDuration);

      function loop(loopSize) {
        lastMeasureResult = measureUploadBandwidth(url, new ArrayBuffer(loopSize / 8));
        lastMeasureResult.promise
          .then(function(bandwidth) {
            lastBandwidth = bandwidth;
            loop(loopSize * 2);
          });
      }

      loop(initialSize);
    });
  }

  return OT.$.RSVP.Promise
    .all([
      doDownload(_httpConfig.downloadUrl, _httpConfig.duration * 1000),
      loopUpload(_httpConfig.uploadUrl, _httpConfig.uploadSize, _httpConfig.duration * 1000)
    ])
    .then(function(results) {
      return {
        downloadBandwidth: results[0],
        uploadBandwidth: results[1]
      };
    });
}

OT.httpTest = httpTest;

// tb_require('../../helpers/helpers.js')

/* exported SDPHelpers */

var START_MEDIA_SSRC = 10000,
    START_RTX_SSRC = 20000;

// Here are the structure of the rtpmap attribute and the media line, most of the
// complex Regular Expressions in this code are matching against one of these two
// formats:
// * a=rtpmap:<payload type> <encoding name>/<clock rate> [/<encoding parameters>]
// * m=<media> <port>/<number of ports> <proto> <fmts>
//
// References:
// * https://tools.ietf.org/html/rfc4566
// * http://en.wikipedia.org/wiki/Session_Description_Protocol
//
var SDPHelpers = {};

// Search through sdpLines to find the Media Line of type +mediaType+.
SDPHelpers.getMLineIndex = function getMLineIndex(sdpLines, mediaType) {
  var targetMLine = 'm=' + mediaType;

  // Find the index of the media line for +type+
  return OT.$.findIndex(sdpLines, function(line) {
    if (line.indexOf(targetMLine) !== -1) {
      return true;
    }

    return false;
  });
};

// Grab a M line of a particular +mediaType+ from sdpLines.
SDPHelpers.getMLine = function getMLine(sdpLines, mediaType) {
  var mLineIndex = SDPHelpers.getMLineIndex(sdpLines, mediaType);
  return mLineIndex > -1 ? sdpLines[mLineIndex] : void 0;
};

SDPHelpers.hasMLinePayloadType = function hasMLinePayloadType(sdpLines, mediaType, payloadType) {
  var mLine = SDPHelpers.getMLine(sdpLines, mediaType),
    payloadTypes = SDPHelpers.getMLinePayloadTypes(mLine, mediaType);
  
  return OT.$.arrayIndexOf(payloadTypes, payloadType) > -1;
};

// Extract the payload types for a give Media Line.
//
SDPHelpers.getMLinePayloadTypes = function getMLinePayloadTypes(mediaLine, mediaType) {
  var mLineSelector = new RegExp('^m=' + mediaType +
                        ' \\d+(/\\d+)? [a-zA-Z0-9/]+(( [a-zA-Z0-9/]+)+)$', 'i');

  // Get all payload types that the line supports
  var payloadTypes = mediaLine.match(mLineSelector);
  if (!payloadTypes || payloadTypes.length < 2) {
    // Error, invalid M line?
    return [];
  }

  return OT.$.trim(payloadTypes[2]).split(' ');
};

SDPHelpers.removeTypesFromMLine = function removeTypesFromMLine(mediaLine, payloadTypes) {
  return OT.$.trim(
            mediaLine.replace(new RegExp(' ' + payloadTypes.join(' |'), 'ig'), ' ')
                     .replace(/\s+/g, ' '));
};

// Remove all references to a particular encodingName from a particular media type
//
SDPHelpers.removeMediaEncoding = function removeMediaEncoding(sdp, mediaType, encodingName) {
  var sdpLines = sdp.split('\r\n'),
      mLineIndex = SDPHelpers.getMLineIndex(sdpLines, mediaType),
      mLine = mLineIndex > -1 ? sdpLines[mLineIndex] : void 0,
      typesToRemove = [],
      payloadTypes,
      match;

  if (mLineIndex === -1) {
    // Error, missing M line
    return sdpLines.join('\r\n');
  }

  // Get all payload types that the line supports
  payloadTypes = SDPHelpers.getMLinePayloadTypes(mLine, mediaType);
  if (payloadTypes.length === 0) {
    // Error, invalid M line?
    return sdpLines.join('\r\n');
  }

  // Find the location of all the rtpmap lines that relate to +encodingName+
  // and any of the supported payload types
  var matcher = new RegExp('a=rtpmap:(' + payloadTypes.join('|') + ') ' +
                                        encodingName + '\\/\\d+', 'i');

  sdpLines = OT.$.filter(sdpLines, function(line, index) {
    match = line.match(matcher);
    if (match === null) return true;

    typesToRemove.push(match[1]);

    if (index < mLineIndex) {
      // This removal changed the index of the mline, track it
      mLineIndex--;
    }

    // remove this one
    return false;
  });

  if (typesToRemove.length > 0 && mLineIndex > -1) {
    // Remove all the payload types and we've removed from the media line
    sdpLines[mLineIndex] = SDPHelpers.removeTypesFromMLine(mLine, typesToRemove);
  }

  return sdpLines.join('\r\n');
};

// Removes all Confort Noise from +sdp+.
//
// See https://jira.tokbox.com/browse/OPENTOK-7176
//
SDPHelpers.removeComfortNoise = function removeComfortNoise(sdp) {
  return SDPHelpers.removeMediaEncoding(sdp, 'audio', 'CN');
};

SDPHelpers.removeVideoCodec = function removeVideoCodec(sdp, codec) {
  return SDPHelpers.removeMediaEncoding(sdp, 'video', codec);
};

// Used to identify whether Video media (for a given set of SDP) supports
// retransmissions.
//
// The algorithm to do could be summarised as:
//
// IF ssrc-group:FID exists AND IT HAS AT LEAST TWO IDS THEN
//    we are using RTX
// ELSE IF "a=rtpmap: (\\d+):rtxPayloadId(/\\d+)? rtx/90000"
//          AND SDPHelpers.hasMLinePayloadType(sdpLines, 'Video', rtxPayloadId)
//    we are using RTX
// ELSE
//    we are not using RTX
//
// The ELSE IF clause basically covers the case where ssrc-group:FID
// is probably malformed or missing. In that case we verify whether
// we want RTX by looking at whether it's mentioned in the video
// media line instead.
//
var isUsingRTX = function isUsingRTX(sdpLines, videoAttrs) {
  var groupFID = videoAttrs.filterByName('ssrc-group:FID'),
      missingFID = groupFID.length === 0;

  if (groupFID.length > 1) {
    // @todo Wut?
    throw new Error('WTF Simulcast?!');
  }

  if (!missingFID) groupFID = groupFID[0].value.split(' ');
  else groupFID = [];

  switch (groupFID.length) {
    case 0:
    case 1:
      // possibly no RTX, double check for the RTX payload type and that
      // the Video Media line contains that payload type
      //
      // Details: Look for a rtpmap line for rtx/90000
      //  If there is one, grab the payload ID for rtx
      //    Look to see if that payload ID is listed under the payload types for the m=Video line
      //      If it is: RTX
      //  else: No RTX for you

      var rtxAttr = videoAttrs.find(function(attr) {
        return attr.name.indexOf('rtpmap:') === 0 &&
               attr.value.indexOf('rtx/90000') > -1;
      });

      if (!rtxAttr) {
        return false;
      }

      var rtxPayloadId = rtxAttr.name.split(':')[1];
      if (rtxPayloadId.indexOf('/') > -1) rtxPayloadId = rtxPayloadId.split('/')[0];
      return SDPHelpers.hasMLinePayloadType(sdpLines, 'video', rtxPayloadId);

    default:
      // two or more: definitely RTX
      return true;
  }
};

// This returns an Array, which is decorated with several
// SDP specific helper methods.
//
SDPHelpers.getAttributesForMediaType = function getAttributesForMediaType(sdpLines, mediaType) {
  var mLineIndex = SDPHelpers.getMLineIndex(sdpLines, mediaType),
    matchOtherMLines = new RegExp('m=(^' + mediaType + ') ', 'i'),
    matchSSRCLines = new RegExp('a=ssrc:\\d+ .*', 'i'),
    matchSSRCGroup = new RegExp('a=ssrc-group:FID (\\d+).*?', 'i'),
    matchAttrLine = new RegExp('a=([a-z0-9:/-]+) (.*)', 'i'),
    ssrcStartIndex,
    ssrcEndIndex,
    attrs = [],
    regResult,
    ssrc, ssrcGroup,
    msidMatch, msid;

  for (var i = mLineIndex + 1; i < sdpLines.length; i++) {
    if (matchOtherMLines.test(sdpLines[i])) {
      break;
    }
    
    // Get the ssrc
    ssrcGroup = sdpLines[i].match(matchSSRCGroup);
    if (ssrcGroup) {
      ssrcStartIndex = i;
      ssrc = ssrcGroup[1];
    }

    // Get the msid
    msidMatch = sdpLines[i].match('a=ssrc:' + ssrc + ' msid:(.+)');
    if (msidMatch) {
      msid = msidMatch[1];
    }
    
    // find where the ssrc lines end
    var isSSRCLine = matchSSRCLines.test(sdpLines[i]);
    if (ssrcStartIndex !== undefined && ssrcEndIndex === undefined && !isSSRCLine ||
      i === sdpLines.length - 1) {
      ssrcEndIndex = i;
    }

    regResult = matchAttrLine.exec(sdpLines[i]);
    if (regResult && regResult.length === 3) {
      attrs.push({
        name: regResult[1],
        value: regResult[2]
      });
    }
  }

  /// The next section decorates the attributes array
  /// with some useful helpers.

  // Store references to the start and end indices
  // of the media section for this mediaType
  attrs.ssrcStartIndex = ssrcStartIndex;
  attrs.ssrcEndIndex = ssrcEndIndex;
  attrs.msid = msid;

  // Add filter support is
  if (!Array.prototype.filter) {
    attrs.filter = OT.$.bind(OT.$.filter, OT.$, attrs);
  }

  if (!Array.prototype.find) {
    attrs.find = OT.$.bind(OT.$.find, OT.$, attrs);
  }

  attrs.isUsingRTX = OT.$.bind(isUsingRTX, null, sdpLines, attrs);

  attrs.filterByName = function(name) {
    return this.filter(function(attr) {
      return attr.name === name;
    });
  };

  return attrs;
};

// Modifies +sdp+ to enable Simulcast for +numberOfStreams+.
//
// Ok, here's the plan:
//  - add the 'a=ssrc-group:SIM' line, it will have numberOfStreams ssrcs
//  - if RTX then add one 'a=ssrc-group:FID', we need to add numberOfStreams lines
//  - add numberOfStreams 'a=ssrc:...' lines for the media ssrc
//  - if RTX then add numberOfStreams 'a=ssrc:...' lines for the RTX ssrc
//
// Re: media and rtx ssrcs:
// We just generate these. The Mantis folk would like us to use sequential numbers
// here for ease of debugging. We can use the same starting number each time as well.
// We should confirm with Oscar/Jose that whether we need to verify that the numbers
// that we choose don't clash with any other ones in the SDP.
//
// I think we do need to check but I can't remember.
//
// Re: The format of the 'a=ssrc:' lines
// Just use the following pattern:
//   a=ssrc:<Media or RTX SSRC> cname:localCname
//   a=ssrc:<Media or RTX SSRC> msid:<MSID>
//
// It doesn't matter that they are all the same and are static.
//
//
SDPHelpers.enableSimulcast = function enableSimulcast(sdp, numberOfStreams) {
  var sdpLines = sdp.split('\r\n'),
      videoAttrs = SDPHelpers.getAttributesForMediaType(sdpLines, 'video'),
      usingRTX = videoAttrs.isUsingRTX(),
      mediaSSRC = [],
      rtxSSRC = [],
      linesToAdd,
      i;

  // generate new media (and rtx if needed) ssrcs
  for (i = 0; i < numberOfStreams; ++i) {
    mediaSSRC.push(START_MEDIA_SSRC + i);
    if (usingRTX) rtxSSRC.push(START_RTX_SSRC + i);
  }

  linesToAdd = [
    'a=ssrc-group:SIM ' + mediaSSRC.join(' ')
  ];

  if (usingRTX) {
    for (i = 0; i < numberOfStreams; ++i) {
      linesToAdd.push('a=ssrc-group:FID ' + mediaSSRC[i] + ' ' + rtxSSRC[i]);
    }
  }

  for (i = 0; i < numberOfStreams; ++i) {
    linesToAdd.push('a=ssrc:' + mediaSSRC[i] + ' cname:localCname',
    'a=ssrc:' + mediaSSRC[i] + ' msid:' + videoAttrs.msid);

  }

  if (usingRTX) {
    for (i = 0; i < numberOfStreams; ++i) {
      linesToAdd.push('a=ssrc:' + rtxSSRC[i] + ' cname:localCname',
      'a=ssrc:' + rtxSSRC[i] + ' msid:' + videoAttrs.msid);
    }
  }
  
  // Replace the previous video ssrc section with our new video ssrc section by
  // deleting the old ssrcs section and inserting the new lines
  linesToAdd.unshift(videoAttrs.ssrcStartIndex, videoAttrs.ssrcEndIndex -
    videoAttrs.ssrcStartIndex);
  sdpLines.splice.apply(sdpLines, linesToAdd);

  return sdpLines.join('\r\n');
};

// tb_require('../../helpers/helpers.js')

function isVideoStat(stat) {
  // Chrome implementation only has this property for RTP video stat
  return stat.hasOwnProperty('googFrameWidthReceived') ||
    stat.hasOwnProperty('googFrameWidthInput') ||
    stat.mediaType === 'video';
}

function isAudioStat(stat) {
  // Chrome implementation only has this property for RTP audio stat
  return stat.hasOwnProperty('audioInputLevel') ||
    stat.hasOwnProperty('audioOutputLevel') ||
    stat.mediaType === 'audio';
}

function isInboundStat(stat) {
  return stat.hasOwnProperty('bytesReceived');
}

function parseStatCategory(stat) {
  var statCategory = {
    packetsLost: 0,
    packetsReceived: 0,
    bytesReceived: 0
  };

  if (stat.hasOwnProperty('packetsReceived')) {
    statCategory.packetsReceived = parseInt(stat.packetsReceived, 10);
  }
  if (stat.hasOwnProperty('packetsLost')) {
    statCategory.packetsLost = parseInt(stat.packetsLost, 10);
  }
  if (stat.hasOwnProperty('bytesReceived')) {
    statCategory.bytesReceived += parseInt(stat.bytesReceived, 10);
  }

  return statCategory;
}

function normalizeTimestamp(timestamp) {
  if (OT.$.isObject(timestamp) && 'getTime' in timestamp) {
    // Chrome as of 39 delivers a "kind of Date" object for timestamps
    // we duck check it and get the timestamp
    return timestamp.getTime();
  } else {
    return timestamp;
  }
}

var getStatsHelpers = {};
getStatsHelpers.isVideoStat = isVideoStat;
getStatsHelpers.isAudioStat = isAudioStat;
getStatsHelpers.isInboundStat = isInboundStat;
getStatsHelpers.parseStatCategory = parseStatCategory;
getStatsHelpers.normalizeTimestamp = normalizeTimestamp;

OT.getStatsHelpers = getStatsHelpers;

// tb_require('../../helpers/helpers.js')

/**
 *
 * @returns {function(RTCPeerConnection,
 * function(DOMError, Array.<{id: string=, type: string=, timestamp: number}>))}
 */
function getStatsAdapter() {

  ///
  // Get Stats using the older API. Used by all current versions
  // of Chrome.
  //
  function getStatsOldAPI(peerConnection, completion) {

    peerConnection.getStats(function(rtcStatsReport) {

      var stats = [];
      rtcStatsReport.result().forEach(function(rtcStat) {

        var stat = {};

        rtcStat.names().forEach(function(name) {
          stat[name] = rtcStat.stat(name);
        });

        // fake the structure of the "new" RTC stat object
        stat.id = rtcStat.id;
        stat.type = rtcStat.type;
        stat.timestamp = rtcStat.timestamp;
        stats.push(stat);
      });

      completion(null, stats);
    });
  }

  ///
  // Get Stats using the newer API.
  //
  function getStatsNewAPI(peerConnection, completion) {

    peerConnection.getStats(null, function(rtcStatsReport) {

      var stats = [];
      rtcStatsReport.forEach(function(rtcStats) {
        stats.push(rtcStats);
      });

      completion(null, stats);
    }, completion);
  }

  if (OT.$.browserVersion().name === 'Firefox' || OTPlugin.isInstalled()) {
    return getStatsNewAPI;
  } else {
    return getStatsOldAPI;
  }
}

OT.getStatsAdpater = getStatsAdapter;

// tb_require('../../helpers/helpers.js')
// tb_require('../peer_connection/get_stats_adapter.js')
// tb_require('../peer_connection/get_stats_helpers.js')

/* global OT */

/**
 * @returns {Promise.<{packetLostRation: number, roundTripTime: number}>}
 */
function webrtcTest(config) {

  var _getStats = OT.getStatsAdpater();
  var _mediaConfig = config.mediaConfig;
  var _localStream = config.localStream;

  // todo copied from peer_connection.js
  // Normalise these
  var NativeRTCSessionDescription,
      NativeRTCIceCandidate;
  if (!OTPlugin.isInstalled()) {
    // order is very important: 'RTCSessionDescription' defined in Firefox Nighly but useless
    NativeRTCSessionDescription = (window.mozRTCSessionDescription ||
    window.RTCSessionDescription);
    NativeRTCIceCandidate = (window.mozRTCIceCandidate || window.RTCIceCandidate);
  } else {
    NativeRTCSessionDescription = OTPlugin.RTCSessionDescription;
    NativeRTCIceCandidate = OTPlugin.RTCIceCandidate;
  }

  function isCandidateRelay(candidate) {
    return candidate.candidate.indexOf('relay') !== -1;
  }

  /**
   * Create video a element attaches it to the body and put it visually outside the body.
   *
   * @returns {OT.VideoElement}
   */
  function createVideoElementForTest() {
    var videoElement = new OT.VideoElement({attributes: {muted: true}});
    videoElement.domElement().style.position = 'absolute';
    videoElement.domElement().style.top = '-9999%';
    videoElement.appendTo(document.body);
    return videoElement;
  }

  function createPeerConnectionForTest() {
    return new OT.$.RSVP.Promise(function(resolve, reject) {
      OT.$.createPeerConnection({
          iceServers: _mediaConfig.iceServers
        }, {},
        null,
        function(error, pc) {
          if (error) {
            reject(new OT.$.Error('createPeerConnection failed', 1600, error));
          } else {
            resolve(pc);
          }
        }
      );
    });
  }

  function createOffer(pc) {
    return new OT.$.RSVP.Promise(function(resolve, reject) {
      pc.createOffer(resolve, reject);
    });
  }

  function attachMediaStream(videoElement, webRtcStream) {
    return new OT.$.RSVP.Promise(function(resolve, reject) {
      videoElement.bindToStream(webRtcStream, function(error) {
        if (error) {
          reject(new OT.$.Error('bindToStream failed', 1600, error));
        } else {
          resolve();
        }
      });
    });
  }

  function addIceCandidate(pc, candidate) {
    return new OT.$.RSVP.Promise(function(resolve, reject) {
      pc.addIceCandidate(new NativeRTCIceCandidate({
        sdpMLineIndex: candidate.sdpMLineIndex,
        candidate: candidate.candidate
      }), resolve, reject);
    });
  }

  function setLocalDescription(pc, offer) {
    return new OT.$.RSVP.Promise(function(resolve, reject) {
      pc.setLocalDescription(offer, resolve, function(error) {
        reject(new OT.$.Error('setLocalDescription failed', 1600, error));
      });
    });
  }

  function setRemoteDescription(pc, offer) {
    return new OT.$.RSVP.Promise(function(resolve, reject) {
      pc.setRemoteDescription(offer, resolve, function(error) {
        reject(new OT.$.Error('setRemoteDescription failed', 1600, error));
      });
    });
  }

  function createAnswer(pc) {
    return new OT.$.RSVP.Promise(function(resolve, reject) {
      pc.createAnswer(resolve, function(error) {
        reject(new OT.$.Error('createAnswer failed', 1600, error));
      });
    });
  }

  function getStats(pc) {
    return new OT.$.RSVP.Promise(function(resolve, reject) {
      _getStats(pc, function(error, stats) {
        if (error) {
          reject(new OT.$.Error('geStats failed', 1600, error));
        } else {
          resolve(stats);
        }
      });
    });
  }

  function createOnIceCandidateListener(pc) {
    return function(event) {
      if (event.candidate && isCandidateRelay(event.candidate)) {
        addIceCandidate(pc, event.candidate)['catch'](function() {
          OT.warn('An error occurred while adding a ICE candidate during webrtc test');
        });
      }
    };
  }

  /**
   * @param {{videoBytesReceived: number, audioBytesReceived: number, startTs: number}} statsSamples
   * @returns {number} the bandwidth in bits per second
   */
  function calculateBandwidth(statsSamples) {
    return (((statsSamples.videoBytesReceived + statsSamples.audioBytesReceived) * 8) /
      (OT.$.now() - statsSamples.startTs)) * 1000;
  }

  /**
   * @returns {Promise.<{packetLostRation: number, roundTripTime: number}>}
   */
  function collectPeerConnectionStats(localPc, remotePc) {

    var SAMPLING_DELAY = 1000;

    return new OT.$.RSVP.Promise(function(resolve) {

      var collectionActive = true;

      var _statsSamples = {
        startTs: OT.$.now(),
        packetLostRatioSamplesCount: 0,
        packetLostRatio: 0,
        roundTripTimeSamplesCount: 0,
        roundTripTime: 0,
        videoBytesReceived: 0,
        audioBytesReceived: 0
      };

      function sample() {

        OT.$.RSVP.Promise.all([
          getStats(localPc).then(function(stats) {
            OT.$.forEach(stats, function(stat) {
              if (OT.getStatsHelpers.isVideoStat(stat)) {
                var rtt = null;

                if (stat.hasOwnProperty('googRtt')) {
                  rtt = parseInt(stat.googRtt, 10);
                } else if (stat.hasOwnProperty('mozRtt')) {
                  rtt = stat.mozRtt;
                }

                if (rtt !== null && rtt > -1) {
                  _statsSamples.roundTripTimeSamplesCount++;
                  _statsSamples.roundTripTime += rtt;
                }
              }
            });
          }),

          getStats(remotePc).then(function(stats) {
            OT.$.forEach(stats, function(stat) {
              if (OT.getStatsHelpers.isVideoStat(stat)) {
                if (stat.hasOwnProperty('packetsReceived') &&
                  stat.hasOwnProperty('packetsLost')) {

                  var packetLost = parseInt(stat.packetsLost, 10);
                  var packetsReceived = parseInt(stat.packetsReceived, 10);
                  if (packetLost >= 0 && packetsReceived > 0) {
                    _statsSamples.packetLostRatioSamplesCount++;
                    _statsSamples.packetLostRatio += packetLost * 100 / packetsReceived;
                  }
                }

                if (stat.hasOwnProperty('bytesReceived')) {
                  _statsSamples.videoBytesReceived = parseInt(stat.bytesReceived, 10);
                }
              } else if (OT.getStatsHelpers.isAudioStat(stat)) {
                if (stat.hasOwnProperty('bytesReceived')) {
                  _statsSamples.audioBytesReceived = parseInt(stat.bytesReceived, 10);
                }
              }
            });
          })
        ])
          .then(function() {
            // wait and trigger another round of collection
            setTimeout(function() {
              if (collectionActive) {
                sample();
              }
            }, SAMPLING_DELAY);
          });
      }

      // start the sampling "loop"
      sample();

      /**
       * @param {boolean} extended marks the test results as extended
       */
      function stopCollectStats(extended) {
        collectionActive = false;

        var pcStats = {
          packetLostRatio: _statsSamples.packetLostRatioSamplesCount > 0 ?
            _statsSamples.packetLostRatio /= _statsSamples.packetLostRatioSamplesCount * 100 : null,
          roundTripTime: _statsSamples.roundTripTimeSamplesCount > 0 ?
            _statsSamples.roundTripTime /= _statsSamples.roundTripTimeSamplesCount : null,
          bandwidth: calculateBandwidth(_statsSamples),
          extended: extended
        };

        resolve(pcStats);
      }

      // sample for the nominal delay
      // if the bandwidth is bellow the threshold at the end we give an extra time
      setTimeout(function() {

        if (calculateBandwidth(_statsSamples) < _mediaConfig.thresholdBitsPerSecond) {
          // give an extra delay in case it was transient bandwidth problem
          setTimeout(function() {
            stopCollectStats(true);
          }, _mediaConfig.extendedDuration * 1000);
        } else {
          stopCollectStats(false);
        }

      }, _mediaConfig.duration * 1000);
    });
  }

  return OT.$.RSVP.Promise
    .all([createPeerConnectionForTest(), createPeerConnectionForTest()])
    .then(function(pcs) {

      var localPc = pcs[0],
          remotePc = pcs[1];

      var localVideo = createVideoElementForTest(),
          remoteVideo = createVideoElementForTest();

      attachMediaStream(localVideo, _localStream);
      localPc.addStream(_localStream);

      var remoteStream;
      remotePc.onaddstream = function(evt) {
        remoteStream = evt.stream;
        attachMediaStream(remoteVideo, remoteStream);
      };

      localPc.onicecandidate = createOnIceCandidateListener(remotePc);
      remotePc.onicecandidate = createOnIceCandidateListener(localPc);

      function dispose() {
        localVideo.destroy();
        remoteVideo.destroy();
        localPc.close();
        remotePc.close();
      }

      return createOffer(localPc)
        .then(function(offer) {
          return OT.$.RSVP.Promise.all([
            setLocalDescription(localPc, offer),
            setRemoteDescription(remotePc, offer)
          ]);
        })
        .then(function() {
          return createAnswer(remotePc);
        })
        .then(function(answer) {
          return OT.$.RSVP.Promise.all([
            setLocalDescription(remotePc, answer),
            setRemoteDescription(localPc, answer)
          ]);
        })
        .then(function() {
          return collectPeerConnectionStats(localPc, remotePc);
        })
        .then(function(value) {
          dispose();
          return value;
        }, function(error) {
          dispose();
          throw error;
        });
    });
}

OT.webrtcTest = webrtcTest;

// tb_require('../../helpers/helpers.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

// Manages N Chrome elements
OT.Chrome = function(properties) {
  var _visible = false,
      _widgets = {},

      // Private helper function
      _set = function(name, widget) {
        widget.parent = this;
        widget.appendTo(properties.parent);

        _widgets[name] = widget;

        this[name] = widget;
      };

  if (!properties.parent) {
    // @todo raise an exception
    return;
  }

  OT.$.eventing(this);

  this.destroy = function() {
    this.off();
    this.hideWhileLoading();

    for (var name in _widgets) {
      _widgets[name].destroy();
    }
  };

  this.showAfterLoading = function() {
    _visible = true;

    for (var name in _widgets) {
      _widgets[name].showAfterLoading();
    }
  };

  this.hideWhileLoading = function() {
    _visible = false;

    for (var name in _widgets) {
      _widgets[name].hideWhileLoading();
    }
  };

  // Adds the widget to the chrome and to the DOM. Also creates a accessor
  // property for it on the chrome.
  //
  // @example
  //  chrome.set('foo', new FooWidget());
  //  chrome.foo.setDisplayMode('on');
  //
  // @example
  //  chrome.set({
  //      foo: new FooWidget(),
  //      bar: new BarWidget()
  //  });
  //  chrome.foo.setDisplayMode('on');
  //
  this.set = function(widgetName, widget) {
    if (typeof widgetName === 'string' && widget) {
      _set.call(this, widgetName, widget);

    } else {
      for (var name in widgetName) {
        if (widgetName.hasOwnProperty(name)) {
          _set.call(this, name, widgetName[name]);
        }
      }
    }
    return this;
  };

};

// tb_require('../../../helpers/helpers.js')
// tb_require('../chrome.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

if (!OT.Chrome.Behaviour) OT.Chrome.Behaviour = {};

// A mixin to encapsulate the basic widget behaviour. This needs a better name,
// it's not actually a widget. It's actually "Behaviour that can be applied to
// an object to make it support the basic Chrome widget workflow"...but that would
// probably been too long a name.
OT.Chrome.Behaviour.Widget = function(widget, options) {
  var _options = options || {},
      _mode,
      _previousOnMode = 'auto',
      _loadingMode;

  //
  // @param [String] mode
  //      'on', 'off', or 'auto'
  //
  widget.setDisplayMode = function(mode) {
    var newMode = mode || 'auto';
    if (_mode === newMode) return;

    OT.$.removeClass(this.domElement, 'OT_mode-' + _mode);
    OT.$.addClass(this.domElement, 'OT_mode-' + newMode);

    if (newMode === 'off') {
      _previousOnMode = _mode;
    }
    _mode = newMode;
  };

  widget.getDisplayMode = function() {
    return _mode;
  };

  widget.show = function() {
    if (_mode !== _previousOnMode) {
      this.setDisplayMode(_previousOnMode);
      if (_options.onShow) _options.onShow();
    }
    return this;
  };

  widget.showAfterLoading = function() {
    this.setDisplayMode(_loadingMode);
  };

  widget.hide = function() {
    if (_mode !== 'off') {
      this.setDisplayMode('off');
      if (_options.onHide) _options.onHide();
    }
    return this;
  };

  widget.hideWhileLoading = function() {
    _loadingMode = _mode;
    this.setDisplayMode('off');
  };

  widget.destroy = function() {
    if (_options.onDestroy) _options.onDestroy(this.domElement);
    if (this.domElement) OT.$.removeElement(this.domElement);

    return widget;
  };

  widget.appendTo = function(parent) {
    // create the element under parent
    this.domElement = OT.$.createElement(_options.nodeName || 'div',
                                        _options.htmlAttributes,
                                        _options.htmlContent);

    if (_options.onCreate) _options.onCreate(this.domElement);

    widget.setDisplayMode(_options.mode);

    if (_options.mode === 'auto') {
      // if the mode is auto we hold the "on mode" for 2 seconds
      // this will let the proper widgets nicely fade away and help discoverability
      OT.$.addClass(widget.domElement, 'OT_mode-on-hold');
      setTimeout(function() {
        OT.$.removeClass(widget.domElement, 'OT_mode-on-hold');
      }, 2000);
    }

    // add the widget to the parent
    parent.appendChild(this.domElement);

    return widget;
  };
};

// tb_require('../../helpers/helpers.js')
// tb_require('./behaviour/widget.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

OT.Chrome.VideoDisabledIndicator = function(options) {
  var videoDisabled = false,
      warning = false,
      updateClasses;

  updateClasses = OT.$.bind(function(element) {
    var shouldDisplay = ['auto', 'on'].indexOf(this.getDisplayMode()) > -1;

    OT.$.removeClass(element, 'OT_video-disabled OT_video-disabled-warning OT_active');

    if (!shouldDisplay) {
      return;
    }

    if (videoDisabled) {
      OT.$.addClass(element, 'OT_video-disabled');
    } else if (warning) {
      OT.$.addClass(element, 'OT_video-disabled-warning');
    }

    OT.$.addClass(element, 'OT_active');
  }, this);

  this.disableVideo = function(value) {
    videoDisabled = value;
    if (value === true) {
      warning = false;
    }
    updateClasses(this.domElement);
  };

  this.setWarning = function(value) {
    warning = value;
    updateClasses(this.domElement);
  };

  // Mixin common widget behaviour
  OT.Chrome.Behaviour.Widget(this, {
    mode: options.mode || 'auto',
    nodeName: 'div',
    htmlAttributes: {
      className: 'OT_video-disabled-indicator'
    }
  });

  var parentSetDisplayMode = OT.$.bind(this.setDisplayMode, this);
  this.setDisplayMode = function(mode) {
    parentSetDisplayMode(mode);
    updateClasses(this.domElement);
  };
};

// tb_require('../../helpers/helpers.js')
// tb_require('./behaviour/widget.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

// NamePanel Chrome Widget
//
// mode (String)
// Whether to display the name. Possible values are: "auto" (the name is displayed
// when the stream is first displayed and when the user mouses over the display),
// "off" (the name is not displayed), and "on" (the name is displayed).
//
// displays a name
// can be shown/hidden
// can be destroyed
OT.Chrome.NamePanel = function(options) {
  var _name = options.name;

  if (!_name || OT.$.trim(_name).length === '') {
    _name = null;

    // THere's no name, just flip the mode off
    options.mode = 'off';
  }

  this.setName = OT.$.bind(function(name) {
    if (!_name) this.setDisplayMode('auto');
    _name = name;
    this.domElement.innerHTML = _name;
  });

  // Mixin common widget behaviour
  OT.Chrome.Behaviour.Widget(this, {
    mode: options.mode,
    nodeName: 'h1',
    htmlContent: _name,
    htmlAttributes: {
      className: 'OT_name OT_edge-bar-item'
    }
  });
};

// tb_require('../../helpers/helpers.js')
// tb_require('./behaviour/widget.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

OT.Chrome.MuteButton = function(options) {
  var _onClickCb,
      _muted = options.muted || false,
      updateClasses,
      attachEvents,
      detachEvents,
      onClick;

  updateClasses = OT.$.bind(function() {
    if (_muted) {
      OT.$.addClass(this.domElement, 'OT_active');
    } else {
      OT.$.removeClass(this.domElement, 'OT_active ');
    }
  }, this);

  // Private Event Callbacks
  attachEvents = function(elem) {
    _onClickCb = OT.$.bind(onClick, this);
    OT.$.on(elem, 'click', _onClickCb);
  };

  detachEvents = function(elem) {
    _onClickCb = null;
    OT.$.off(elem, 'click', _onClickCb);
  };

  onClick = function() {
    _muted = !_muted;

    updateClasses();

    if (_muted) {
      this.parent.trigger('muted', this);
    } else {
      this.parent.trigger('unmuted', this);
    }

    return false;
  };

  OT.$.defineProperties(this, {
    muted: {
      get: function() { return _muted; },
      set: function(muted) {
        _muted = muted;
        updateClasses();
      }
    }
  });

  // Mixin common widget behaviour
  var classNames = _muted ? 'OT_edge-bar-item OT_mute OT_active' : 'OT_edge-bar-item OT_mute';
  OT.Chrome.Behaviour.Widget(this, {
    mode: options.mode,
    nodeName: 'button',
    htmlContent: 'Mute',
    htmlAttributes: {
      className: classNames
    },
    onCreate: OT.$.bind(attachEvents, this),
    onDestroy: OT.$.bind(detachEvents, this)
  });
};

// tb_require('../../helpers/helpers.js')
// tb_require('./behaviour/widget.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

// BackingBar Chrome Widget
//
// nameMode (String)
// Whether or not the name panel is being displayed
// Possible values are: "auto" (the name is displayed
// when the stream is first displayed and when the user mouses over the display),
// "off" (the name is not displayed), and "on" (the name is displayed).
//
// muteMode (String)
// Whether or not the mute button is being displayed
// Possible values are: "auto" (the mute button is displayed
// when the stream is first displayed and when the user mouses over the display),
// "off" (the mute button is not displayed), and "on" (the mute button is displayed).
//
// displays a backing bar
// can be shown/hidden
// can be destroyed
OT.Chrome.BackingBar = function(options) {
  var _nameMode = options.nameMode,
      _muteMode = options.muteMode;

  function getDisplayMode() {
    if (_nameMode === 'on' || _muteMode === 'on') {
      return 'on';
    } else if (_nameMode === 'mini' || _muteMode === 'mini') {
      return 'mini';
    } else if (_nameMode === 'mini-auto' || _muteMode === 'mini-auto') {
      return 'mini-auto';
    } else if (_nameMode === 'auto' || _muteMode === 'auto') {
      return 'auto';
    } else {
      return 'off';
    }
  }

  // Mixin common widget behaviour
  OT.Chrome.Behaviour.Widget(this, {
    mode: getDisplayMode(),
    nodeName: 'div',
    htmlContent: '',
    htmlAttributes: {
      className: 'OT_bar OT_edge-bar-item'
    }
  });

  this.setNameMode = function(nameMode) {
    _nameMode = nameMode;
    this.setDisplayMode(getDisplayMode());
  };

  this.setMuteMode = function(muteMode) {
    _muteMode = muteMode;
    this.setDisplayMode(getDisplayMode());
  };

};

// tb_require('../../helpers/helpers.js')
// tb_require('./behaviour/widget.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
 trailing: true, browser: true, smarttabs:true */
/* global OT */

OT.Chrome.AudioLevelMeter = function(options) {

  var widget = this,
      _meterBarElement,
      _voiceOnlyIconElement,
      _meterValueElement,
      _value,
      _maxValue = options.maxValue || 1,
      _minValue = options.minValue || 0;

  function onCreate() {
    _meterBarElement = OT.$.createElement('div', {
      className: 'OT_audio-level-meter__bar'
    }, '');
    _meterValueElement = OT.$.createElement('div', {
      className: 'OT_audio-level-meter__value'
    }, '');
    _voiceOnlyIconElement = OT.$.createElement('div', {
      className: 'OT_audio-level-meter__audio-only-img'
    }, '');

    widget.domElement.appendChild(_meterBarElement);
    widget.domElement.appendChild(_voiceOnlyIconElement);
    widget.domElement.appendChild(_meterValueElement);
  }

  function updateView() {
    var percentSize = _value * 100 / (_maxValue - _minValue);
    _meterValueElement.style.width = _meterValueElement.style.height = 2 * percentSize + '%';
    _meterValueElement.style.top = _meterValueElement.style.right = -percentSize + '%';
  }

  // Mixin common widget behaviour
  var widgetOptions = {
    mode: options ? options.mode : 'auto',
    nodeName: 'div',
    htmlAttributes: {
      className: 'OT_audio-level-meter'
    },
    onCreate: onCreate
  };

  OT.Chrome.Behaviour.Widget(this, widgetOptions);

  // override
  var _setDisplayMode = OT.$.bind(widget.setDisplayMode, widget);
  widget.setDisplayMode = function(mode) {
    _setDisplayMode(mode);
    if (mode === 'off') {
      if (options.onPassivate) options.onPassivate();
    } else {
      if (options.onActivate) options.onActivate();
    }
  };

  widget.setValue = function(value) {
    _value = value;
    updateView();
  };
};

// tb_require('../../helpers/helpers.js')
// tb_require('./behaviour/widget.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

// Archving Chrome Widget
//
// mode (String)
// Whether to display the archving widget. Possible values are: "on" (the status is displayed
// when archiving and briefly when archving ends) and "off" (the status is not displayed)

// Whether to display the archving widget. Possible values are: "auto" (the name is displayed
// when the status is first displayed and when the user mouses over the display),
// "off" (the name is not displayed), and "on" (the name is displayed).
//
// displays a name
// can be shown/hidden
// can be destroyed
OT.Chrome.Archiving = function(options) {
  var _archiving = options.archiving,
      _archivingStarted = options.archivingStarted || 'Archiving on',
      _archivingEnded = options.archivingEnded || 'Archiving off',
      _initialState = true,
      _lightBox,
      _light,
      _text,
      _textNode,
      renderStageDelayedAction,
      renderText,
      renderStage;

  renderText = function(text) {
    _textNode.nodeValue = text;
    _lightBox.setAttribute('title', text);
  };

  renderStage = OT.$.bind(function() {
    if (renderStageDelayedAction) {
      clearTimeout(renderStageDelayedAction);
      renderStageDelayedAction = null;
    }

    if (_archiving) {
      OT.$.addClass(_light, 'OT_active');
    } else {
      OT.$.removeClass(_light, 'OT_active');
    }

    OT.$.removeClass(this.domElement, 'OT_archiving-' + (!_archiving ? 'on' : 'off'));
    OT.$.addClass(this.domElement, 'OT_archiving-' + (_archiving ? 'on' : 'off'));
    if (options.show && _archiving) {
      renderText(_archivingStarted);
      OT.$.addClass(_text, 'OT_mode-on');
      OT.$.removeClass(_text, 'OT_mode-auto');
      this.setDisplayMode('on');
      renderStageDelayedAction = setTimeout(function() {
        OT.$.addClass(_text, 'OT_mode-auto');
        OT.$.removeClass(_text, 'OT_mode-on');
      }, 5000);
    } else if (options.show && !_initialState) {
      OT.$.addClass(_text, 'OT_mode-on');
      OT.$.removeClass(_text, 'OT_mode-auto');
      this.setDisplayMode('on');
      renderText(_archivingEnded);
      renderStageDelayedAction = setTimeout(OT.$.bind(function() {
        this.setDisplayMode('off');
      }, this), 5000);
    } else {
      this.setDisplayMode('off');
    }
  }, this);

  // Mixin common widget behaviour
  OT.Chrome.Behaviour.Widget(this, {
    mode: _archiving && options.show && 'on' || 'off',
    nodeName: 'h1',
    htmlAttributes: {className: 'OT_archiving OT_edge-bar-item OT_edge-bottom'},
    onCreate: OT.$.bind(function() {
      _lightBox = OT.$.createElement('div', {
        className: 'OT_archiving-light-box'
      }, '');
      _light = OT.$.createElement('div', {
        className: 'OT_archiving-light'
      }, '');
      _lightBox.appendChild(_light);
      _text = OT.$.createElement('div', {
        className: 'OT_archiving-status OT_mode-on OT_edge-bar-item OT_edge-bottom'
      }, '');
      _textNode = document.createTextNode('');
      _text.appendChild(_textNode);
      this.domElement.appendChild(_lightBox);
      this.domElement.appendChild(_text);
      renderStage();
    }, this)
  });

  this.setShowArchiveStatus = OT.$.bind(function(show) {
    options.show = show;
    if (this.domElement) {
      renderStage.call(this);
    }
  }, this);

  this.setArchiving = OT.$.bind(function(status) {
    _archiving = status;
    _initialState = false;
    if (this.domElement) {
      renderStage.call(this);
    }
  }, this);

};

// tb_require('../helpers.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

// Web OT Helpers
!(function(window) {
  // guard for Node.js
  if (window && typeof navigator !== 'undefined') {
    var NativeRTCPeerConnection = (window.webkitRTCPeerConnection ||
                                   window.mozRTCPeerConnection);

    if (navigator.webkitGetUserMedia) {
      /*global webkitMediaStream, webkitRTCPeerConnection*/
      // Stub for getVideoTracks for Chrome < 26
      if (!webkitMediaStream.prototype.getVideoTracks) {
        webkitMediaStream.prototype.getVideoTracks = function() {
          return this.videoTracks;
        };
      }

      // Stubs for getAudioTracks for Chrome < 26
      if (!webkitMediaStream.prototype.getAudioTracks) {
        webkitMediaStream.prototype.getAudioTracks = function() {
          return this.audioTracks;
        };
      }

      if (!webkitRTCPeerConnection.prototype.getLocalStreams) {
        webkitRTCPeerConnection.prototype.getLocalStreams = function() {
          return this.localStreams;
        };
      }

      if (!webkitRTCPeerConnection.prototype.getRemoteStreams) {
        webkitRTCPeerConnection.prototype.getRemoteStreams = function() {
          return this.remoteStreams;
        };
      }

    } else if (navigator.mozGetUserMedia) {
      // Firefox < 23 doesn't support get Video/Audio tracks, we'll just stub them out for now.
      /* global MediaStream */
      if (!MediaStream.prototype.getVideoTracks) {
        MediaStream.prototype.getVideoTracks = function() {
          return [];
        };
      }

      if (!MediaStream.prototype.getAudioTracks) {
        MediaStream.prototype.getAudioTracks = function() {
          return [];
        };
      }

      // This won't work as mozRTCPeerConnection is a weird internal Firefox
      // object (a wrapped native object I think).
      // if (!window.mozRTCPeerConnection.prototype.getLocalStreams) {
      //     window.mozRTCPeerConnection.prototype.getLocalStreams = function() {
      //         return this.localStreams;
      //     };
      // }

      // This won't work as mozRTCPeerConnection is a weird internal Firefox
      // object (a wrapped native object I think).
      // if (!window.mozRTCPeerConnection.prototype.getRemoteStreams) {
      //     window.mozRTCPeerConnection.prototype.getRemoteStreams = function() {
      //         return this.remoteStreams;
      //     };
      // }
    }

    // The setEnabled method on MediaStreamTracks is a OTPlugin
    // construct. In this particular instance it's easier to bring
    // all the good browsers down to IE's level than bootstrap it up.
    if (typeof window.MediaStreamTrack !== 'undefined') {
      if (!window.MediaStreamTrack.prototype.setEnabled) {
        window.MediaStreamTrack.prototype.setEnabled = function(enabled) {
          this.enabled = OT.$.castToBoolean(enabled);
        };
      }
    }

    if (!window.URL && window.webkitURL) {
      window.URL = window.webkitURL;
    }

    OT.$.createPeerConnection = function(config, options, publishersWebRtcStream, completion) {
      if (OTPlugin.isInstalled()) {
        OTPlugin.initPeerConnection(config, options,
                                    publishersWebRtcStream, completion);
      } else {
        var pc;

        try {
          pc = new NativeRTCPeerConnection(config, options);
        } catch (e) {
          completion(e.message);
          return;
        }

        completion(null, pc);
      }
    };
  }

  // Returns a String representing the supported WebRTC crypto scheme. The possible
  // values are SDES_SRTP, DTLS_SRTP, and NONE;
  //
  // Broadly:
  // * Firefox only supports DTLS
  // * Older versions of Chrome (<= 24) only support SDES
  // * Newer versions of Chrome (>= 25) support DTLS and SDES
  //
  OT.$.supportedCryptoScheme = function() {
    return OT.$.env.name === 'Chrome' && OT.$.env.version < 25 ? 'SDES_SRTP' : 'DTLS_SRTP';
  };

})(window);

// tb_require('../../helpers/helpers.js')
// tb_require('../../helpers/lib/web_rtc.js')

/* exported subscribeProcessor */
/* global SDPHelpers */

// Attempt to completely process a subscribe message. This will:
// * create an Offer
// * set the new offer as the location description
//
// If there are no issues, the +success+ callback will be executed on completion.
// Errors during any step will result in the +failure+ callback being executed.
//
var subscribeProcessor = function(peerConnection, numberOfSimulcastStreams, success, failure) {
  var generateErrorCallback,
      setLocalDescription;

  generateErrorCallback = function(message, prefix) {
    return function(errorReason) {
      OT.error(message);
      OT.error(errorReason);

      if (failure) failure(message, errorReason, prefix);
    };
  };

  setLocalDescription = function(offer) {
    offer.sdp = SDPHelpers.removeComfortNoise(offer.sdp);
    offer.sdp = SDPHelpers.removeVideoCodec(offer.sdp, 'ulpfec');
    offer.sdp = SDPHelpers.removeVideoCodec(offer.sdp, 'red');
    if (numberOfSimulcastStreams > 1) {
      offer.sdp = SDPHelpers.enableSimulcast(offer.sdp, numberOfSimulcastStreams);
    }

    peerConnection.setLocalDescription(
      offer,

      // Success
      function() {
        success(offer);
      },

      // Failure
      generateErrorCallback('Error while setting LocalDescription', 'SetLocalDescription')
    );
  };

  peerConnection.createOffer(
    // Success
    setLocalDescription,

    // Failure
    generateErrorCallback('Error while creating Offer', 'CreateOffer'),

    // Constraints
    {}
  );
};

// tb_require('../../helpers/helpers.js')
// tb_require('../../helpers/lib/web_rtc.js')

/* exported offerProcessor */
/* global SDPHelpers */

// Attempt to completely process +offer+. This will:
// * set the offer as the remote description
// * create an answer and
// * set the new answer as the location description
//
// If there are no issues, the +success+ callback will be executed on completion.
// Errors during any step will result in the +failure+ callback being executed.
//
var offerProcessor = function(peerConnection, offer, success, failure) {
  var generateErrorCallback,
      setLocalDescription,
      createAnswer;

  generateErrorCallback = function(message, prefix) {
    return function(errorReason) {
      OT.error(message);
      OT.error(errorReason);

      if (failure) failure(message, errorReason, prefix);
    };
  };

  setLocalDescription = function(answer) {
    answer.sdp = SDPHelpers.removeComfortNoise(answer.sdp);
    answer.sdp = SDPHelpers.removeVideoCodec(answer.sdp, 'ulpfec');
    answer.sdp = SDPHelpers.removeVideoCodec(answer.sdp, 'red');

    peerConnection.setLocalDescription(
      answer,

      // Success
      function() {
        success(answer);
      },

      // Failure
      generateErrorCallback('Error while setting LocalDescription', 'SetLocalDescription')
    );
  };

  createAnswer = function() {
    peerConnection.createAnswer(
      // Success
      setLocalDescription,

      // Failure
      generateErrorCallback('Error while setting createAnswer', 'CreateAnswer'),

      null, // MediaConstraints
      false // createProvisionalAnswer
    );
  };

  if (offer.sdp.indexOf('a=rtcp-fb') === -1) {
    var rtcpFbLine = 'a=rtcp-fb:* ccm fir\r\na=rtcp-fb:* nack ';

    offer.sdp = offer.sdp.replace(/^m=video(.*)$/gmi, 'm=video$1\r\n' + rtcpFbLine);
  }

  peerConnection.setRemoteDescription(
    offer,

    // Success
    createAnswer,

    // Failure
    generateErrorCallback('Error while setting RemoteDescription', 'SetRemoteDescription')
  );

};

// tb_require('../../helpers/helpers.js')
// tb_require('../../helpers/lib/web_rtc.js')

/* exported IceCandidateProcessor */

// Normalise these
var NativeRTCIceCandidate;

if (!OTPlugin.isInstalled()) {
  NativeRTCIceCandidate = (window.mozRTCIceCandidate || window.RTCIceCandidate);
} else {
  NativeRTCIceCandidate = OTPlugin.RTCIceCandidate;
}

// Process incoming Ice Candidates from a remote connection (which have been
// forwarded via iceCandidateForwarder). The Ice Candidates cannot be processed
// until a PeerConnection is available. Once a PeerConnection becomes available
// the pending PeerConnections can be processed by calling processPending.
//
// @example
//
//  var iceProcessor = new IceCandidateProcessor();
//  iceProcessor.process(iceMessage1);
//  iceProcessor.process(iceMessage2);
//  iceProcessor.process(iceMessage3);
//
//  iceProcessor.setPeerConnection(peerConnection);
//  iceProcessor.processPending();
//
var IceCandidateProcessor = function() {
  var _pendingIceCandidates = [],
      _peerConnection = null;

  this.setPeerConnection = function(peerConnection) {
    _peerConnection = peerConnection;
  };

  this.process = function(message) {
    var iceCandidate = new NativeRTCIceCandidate(message.content);

    if (_peerConnection) {
      _peerConnection.addIceCandidate(iceCandidate);
    } else {
      _pendingIceCandidates.push(iceCandidate);
    }
  };

  this.processPending = function() {
    while (_pendingIceCandidates.length) {
      _peerConnection.addIceCandidate(_pendingIceCandidates.shift());
    }
  };
};

// tb_require('../../helpers/helpers.js')
// tb_require('../../helpers/lib/web_rtc.js')

/* exported DataChannel */

// Wraps up a native RTCDataChannelEvent object for the message event. This is
// so we never accidentally leak the native DataChannel.
//
// @constructor
// @private
//
var DataChannelMessageEvent = function DataChanneMessageEvent(event) {
  this.data = event.data;
  this.source = event.source;
  this.lastEventId = event.lastEventId;
  this.origin = event.origin;
  this.timeStamp = event.timeStamp;
  this.type = event.type;
  this.ports = event.ports;
  this.path = event.path;
};

// DataChannel is a wrapper of the native browser RTCDataChannel object.
//
// It exposes an interface that is very similar to the native one except
// for the following differences:
//  * eventing is handled in a way that is consistent with the OpenTok API
//  * calls to the send method that occur before the channel is open will be
//    buffered until the channel is open (as opposed to throwing an exception)
//
// By design, there is (almost) no direct access to the native object. This is to ensure
// that we can control the underlying implementation as needed.
//
// NOTE: IT TURNS OUT THAT FF HASN'T IMPLEMENT THE LATEST PUBLISHED DATACHANNELS
// SPEC YET, SO THE INFO ABOUT maxRetransmitTime AND maxRetransmits IS WRONG. INSTEAD
// THERE IS A SINGLE PROPERTY YOU PROVIDE WHEN CREATING THE CHANNEL WHICH CONTROLS WHETHER
// THE CHANNEL IS RELAIBLE OF NOT.
//
// Two properties that will have a large bearing on channel behaviour are maxRetransmitTime
// and maxRetransmits. Descriptions of those properties are below. They are set when creating
// the initial native RTCDataChannel.
//
// maxRetransmitTime of type unsigned short, readonly , nullable
//   The RTCDataChannel.maxRetransmitTime attribute returns the length of the time window
//   (in milliseconds) during which retransmissions may occur in unreliable mode, or null if
//   unset. The attribute MUST return the value to which it was set when the RTCDataChannel was
//   created.
//
// maxRetransmits of type unsigned short, readonly , nullable
//   The RTCDataChannel.maxRetransmits attribute returns the maximum number of retransmissions
//   that are attempted in unreliable mode, or null if unset. The attribute MUST return the value
//   to which it was set when the RTCDataChannel was created.
//
// @reference http://www.w3.org/TR/webrtc/#peer-to-peer-data-api
//
// @param [RTCDataChannel] dataChannel A native data channel.
//
//
// @constructor
// @private
//
var DataChannel = function DataChannel(dataChannel) {
  var api = {};

  /// Private Data

  var bufferedMessages = [];

  var qosData = {
    sentMessages: 0,
    recvMessages: 0
  };

  /// Private API

  var bufferMessage = function bufferMessage(data) {
        bufferedMessages.push(data);
        return api;
      },

      sendMessage = function sendMessage(data) {
        dataChannel.send(data);
        qosData.sentMessages++;

        return api;
      },

      flushBufferedMessages = function flushBufferedMessages() {
        var data;

        while ((data = bufferedMessages.shift())) {
          api.send(data);
        }
      },

      onOpen = function onOpen() {
        api.send = sendMessage;
        flushBufferedMessages();
      },

      onClose = function onClose(event) {
        api.send = bufferMessage;
        api.trigger('close', event);
      },

      onError = function onError(event) {
        OT.error('Data Channel Error:', event);
      },

      onMessage = function onMessage(domEvent) {
        var event = new DataChannelMessageEvent(domEvent);
        qosData.recvMessages++;

        api.trigger('message', event);
      };

  /// Public API

  OT.$.eventing(api, true);

  api.label = dataChannel.label;
  api.id = dataChannel.id;
  // api.maxRetransmitTime = dataChannel.maxRetransmitTime;
  // api.maxRetransmits = dataChannel.maxRetransmits;
  api.reliable = dataChannel.reliable;
  api.negotiated = dataChannel.negotiated;
  api.ordered = dataChannel.ordered;
  api.protocol = dataChannel.protocol;
  api._channel = dataChannel;

  api.close = function() {
    dataChannel.close();
  };

  api.equals = function(label, options) {
    if (api.label !== label) return false;

    for (var key in options) {
      if (options.hasOwnProperty(key)) {
        if (api[key] !== options[key]) {
          return false;
        }
      }
    }

    return true;
  };

  api.getQosData = function() {
    return qosData;
  };

  // Send initially just buffers all messages until
  // the channel is open.
  api.send = bufferMessage;

  /// Init
  dataChannel.addEventListener('open', onOpen, false);
  dataChannel.addEventListener('close', onClose, false);
  dataChannel.addEventListener('error', onError, false);
  dataChannel.addEventListener('message', onMessage, false);

  return api;
};

// tb_require('../../helpers/helpers.js')
// tb_require('../../helpers/lib/web_rtc.js')
// tb_require('./data_channel.js')

/* exported PeerConnectionChannels */
/* global DataChannel */

// Contains a collection of DataChannels for a particular RTCPeerConnection
//
// @param [RTCPeerConnection] pc A native peer connection object
//
// @constructor
// @private
//
var PeerConnectionChannels = function PeerConnectionChannels(pc) {
  /// Private Data
  var channels = [],
      api = {};

  var lastQos = {
    sentMessages: 0,
    recvMessages: 0
  };

  /// Private API

  var remove = function remove(channel) {
    OT.$.filter(channels, function(c) {
      return channel !== c;
    });
  };

  var add = function add(nativeChannel) {
    var channel = new DataChannel(nativeChannel);
    channels.push(channel);

    channel.on('close', function() {
      remove(channel);
    });

    return channel;
  };

  /// Public API

  api.add = function(label, options) {
    return add(pc.createDataChannel(label, options));
  };

  api.addMany = function(newChannels) {
    for (var label in newChannels) {
      if (newChannels.hasOwnProperty(label)) {
        api.add(label, newChannels[label]);
      }
    }
  };

  api.get = function(label, options) {
    return OT.$.find(channels, function(channel) {
      return channel.equals(label, options);
    });
  };

  api.getOrAdd = function(label, options) {
    var channel = api.get(label, options);
    if (!channel) {
      channel = api.add(label, options);
    }

    return channel;
  };

  api.getQosData = function() {
    var qosData = {
      sentMessages: 0,
      recvMessages: 0
    };

    OT.$.forEach(channels, function(channel) {
      qosData.sentMessages += channel.getQosData().sentMessages;
      qosData.recvMessages += channel.getQosData().recvMessages;
    });

    return qosData;
  };

  api.sampleQos = function() {
    var newQos = api.getQosData();

    var sampleQos = {
      sentMessages: newQos.sentMessages - lastQos.sentMessages,
      recvMessages: newQos.recvMessages - lastQos.recvMessages
    };

    lastQos = newQos;

    return sampleQos;
  };

  api.destroy = function() {
    OT.$.forEach(channels, function(channel) {
      channel.close();
    });

    channels = [];
  };

  /// Init

  pc.addEventListener('datachannel', function(event) {
    add(event.channel);
  }, false);

  return api;
};

// tb_require('../../helpers/helpers.js')
// tb_require('../../helpers/lib/web_rtc.js')

/* exported connectionStateLogger */

// @meta: ping Mike/Eric to let them know that this data is coming and what format it will be
// @meta: what reports would I like around this, what question am I trying to answer?
//
// log the sequence of iceconnectionstates, icegatheringstates, signalingstates
// log until we reach a terminal iceconnectionstate or signalingstate
// send a client event once we have a full sequence
//
// Format of the states:
//  [
//    {delta: 1234, iceConnection: 'new', signalingstate: 'stable', iceGathering: 'new'},
//    {delta: 1234, iceConnection: 'new', signalingstate: 'stable', iceGathering: 'new'},
//    {delta: 1234, iceConnection: 'new', signalingstate: 'stable', iceGathering: 'new'},
//  ]
//
// Format of the logged event:
//  {
//    startTime: 1234,
//    finishTime: 5678,
//    states: [
//      {delta: 1234, iceConnection: 'new', signalingstate: 'stable', iceGathering: 'new'},
//      {delta: 1234, iceConnection: 'new', signalingstate: 'stable', iceGathering: 'new'},
//      {delta: 1234, iceConnection: 'new', signalingstate: 'stable', iceGathering: 'new'},
//    ]
//  }
//
var connectionStateLogger = function(pc) {
  var startTime = OT.$.now(),
      finishTime,
      suceeded,
      states = [];

  var trackState = function() {
    var now = OT.$.now(),
        lastState = states[states.length - 1],
        state = {delta: finishTime ? now - finishTime : 0};

    if (!lastState || lastState.iceConnection !== pc.iceConnectionState) {
      state.iceConnectionState = pc.iceConnectionState;
    }

    if (!lastState || lastState.signalingState !== pc.signalingState) {
      state.signalingState = pc.signalingState;
    }

    if (!lastState || lastState.iceGatheringState !== pc.iceGatheringState) {
      state.iceGathering = pc.iceGatheringState;
    }
    OT.debug(state);
    states.push(state);
    finishTime = now;
  };

  pc.addEventListener('iceconnectionstatechange', trackState, false);
  pc.addEventListener('signalingstatechange', trackState, false);
  pc.addEventListener('icegatheringstatechange', trackState, false);

  return {
    stop: function() {
      pc.removeEventListener('iceconnectionstatechange', trackState, false);
      pc.removeEventListener('signalingstatechange', trackState, false);
      pc.removeEventListener('icegatheringstatechange', trackState, false);

      // @todo The client logging of these states is not currently used, so it's left todo.

      // @todo analyse final state and decide whether the connection was successful
      suceeded = true;

      var payload = {
        type: 'PeerConnectionWorkflow',
        success: suceeded,
        startTime: startTime,
        finishTime: finishTime,
        states: states
      };

      // @todo send client event
      OT.debug(payload);
    }
  };
};

// tb_require('../../helpers/helpers.js')
// tb_require('../../helpers/lib/web_rtc.js')
// tb_require('./connection_state_logger.js')
// tb_require('./ice_candidate_processor.js')
// tb_require('./subscribe_processor.js')
// tb_require('./offer_processor.js')
// tb_require('./get_stats_adapter.js')
// tb_require('./peer_connection_channels.js')

/* global offerProcessor, subscribeProcessor, connectionStateLogger,
          IceCandidateProcessor, PeerConnectionChannels */

// Normalise these
var NativeRTCSessionDescription;

if (!OTPlugin.isInstalled()) {
  // order is very important: 'RTCSessionDescription' defined in Firefox Nighly but useless
  NativeRTCSessionDescription = (window.mozRTCSessionDescription ||
                                 window.RTCSessionDescription);
} else {
  NativeRTCSessionDescription = OTPlugin.RTCSessionDescription;
}

// Helper function to forward Ice Candidates via +messageDelegate+
var iceCandidateForwarder = function(messageDelegate) {
  return function(event) {
    if (event.candidate) {
      messageDelegate(OT.Raptor.Actions.CANDIDATE, {
        candidate: event.candidate.candidate,
        sdpMid: event.candidate.sdpMid || '',
        sdpMLineIndex: event.candidate.sdpMLineIndex || 0
      });
    } else {
      OT.debug('IceCandidateForwarder: No more ICE candidates.');
    }
  };
};

/*
 * Negotiates a WebRTC PeerConnection.
 *
 * Responsible for:
 * * offer-answer exchange
 * * iceCandidates
 * * notification of remote streams being added/removed
 *
 */
OT.PeerConnection = function(config) {
  var _peerConnection,
      _peerConnectionCompletionHandlers = [],
      _channels,
      _iceProcessor = new IceCandidateProcessor(),
      _getStatsAdapter = OT.getStatsAdpater(),
      _stateLogger,
      _offer,
      _answer,
      _state = 'new',
      _messageDelegates = [],
      api = {};

  OT.$.eventing(api);

  // if ice servers doesn't exist Firefox will throw an exception. Chrome
  // interprets this as 'Use my default STUN servers' whereas FF reads it
  // as 'Don't use STUN at all'. *Grumble*
  if (!config.iceServers) config.iceServers = [];

  // Private methods
  var delegateMessage = function(type, messagePayload, uri) {
        if (_messageDelegates.length) {
          // We actually only ever send to the first delegate. This is because
          // each delegate actually represents a Publisher/Subscriber that
          // shares a single PeerConnection. If we sent to all delegates it
          // would result in each message being processed multiple times by
          // each PeerConnection.
          _messageDelegates[0](type, messagePayload, uri);
        }
      },

      // Create and initialise the PeerConnection object. This deals with
      // any differences between the various browser implementations and
      // our own OTPlugin version.
      //
      // +completion+ is the function is call once we've either successfully
      // created the PeerConnection or on failure.
      //
      // +localWebRtcStream+ will be null unless the callee is representing
      // a publisher. This is an unfortunate implementation limitation
      // of OTPlugin, it's not used for vanilla WebRTC. Hopefully this can
      // be tidied up later.
      //
      createPeerConnection = function(completion, localWebRtcStream) {
        if (_peerConnection) {
          completion.call(null, null, _peerConnection);
          return;
        }

        _peerConnectionCompletionHandlers.push(completion);

        if (_peerConnectionCompletionHandlers.length > 1) {
          // The PeerConnection is already being setup, just wait for
          // it to be ready.
          return;
        }

        var pcConstraints = {
          optional: [
            // This should be unnecessary, but the plugin has issues if we remove it. This needs
            // to be investigated.
            {DtlsSrtpKeyAgreement: true},

            // https://jira.tokbox.com/browse/OPENTOK-21989
            {googIPv6: false}
          ]
        };

        OT.debug('Creating peer connection config "' + JSON.stringify(config) + '".');

        if (!config.iceServers || config.iceServers.length === 0) {
          // This should never happen unless something is misconfigured
          OT.error('No ice servers present');
          OT.analytics.logEvent({
            action: 'Error',
            variation: 'noIceServers'
          });
        }

        OT.$.createPeerConnection(config, pcConstraints, localWebRtcStream,
                                        attachEventsToPeerConnection);
      },

      // An auxiliary function to createPeerConnection. This binds the various event callbacks
      // once the peer connection is created.
      //
      // +err+ will be non-null if an err occured while creating the PeerConnection
      // +pc+ will be the PeerConnection object itself.
      //
      attachEventsToPeerConnection = function(err, pc) {
        if (err) {
          triggerError('Failed to create PeerConnection, exception: ' +
              err.toString(), 'NewPeerConnection');

          _peerConnectionCompletionHandlers = [];
          return;
        }

        OT.debug('OT attachEventsToPeerConnection');
        _peerConnection = pc;
        _stateLogger = connectionStateLogger(_peerConnection);
        _channels = new PeerConnectionChannels(_peerConnection);
        if (config.channels) _channels.addMany(config.channels);

        _peerConnection.addEventListener('icecandidate',
                                    iceCandidateForwarder(delegateMessage), false);
        _peerConnection.addEventListener('addstream', onRemoteStreamAdded, false);
        _peerConnection.addEventListener('removestream', onRemoteStreamRemoved, false);
        _peerConnection.addEventListener('signalingstatechange', routeStateChanged, false);

        if (_peerConnection.oniceconnectionstatechange !== void 0) {
          var failedStateTimer;
          _peerConnection.addEventListener('iceconnectionstatechange', function(event) {
            if (event.target.iceConnectionState === 'failed') {
              if (failedStateTimer) {
                clearTimeout(failedStateTimer);
              }

              // We wait 5 seconds and make sure that it's still in the failed state
              // before we trigger the error. This is because we sometimes see
              // 'failed' and then 'connected' afterwards.
              failedStateTimer = setTimeout(function() {
                if (event.target.iceConnectionState === 'failed') {
                  triggerError('The stream was unable to connect due to a network error.' +
                   ' Make sure your connection isn\'t blocked by a firewall.', 'ICEWorkflow');
                }
              }, 5000);
            } else if (event.target.iceConnectionState === 'disconnected') {
              OT.analytics.logEvent({
                action: 'attachEventsToPeerConnection',
                variation: 'iceconnectionstatechange',
                payload: 'disconnected'
              });
            }
          }, false);
        }

        triggerPeerConnectionCompletion(null);
      },

      triggerPeerConnectionCompletion = function() {
        while (_peerConnectionCompletionHandlers.length) {
          _peerConnectionCompletionHandlers.shift().call(null);
        }
      },

      // Clean up the Peer Connection and trigger the close event.
      // This function can be called safely multiple times, it will
      // only trigger the close event once (per PeerConnection object)
      tearDownPeerConnection = function() {
        // Our connection is dead, stop processing ICE candidates
        if (_iceProcessor) _iceProcessor.setPeerConnection(null);
        if (_stateLogger) _stateLogger.stop();

        qos.stopCollecting();

        if (_peerConnection !== null) {
          if (_peerConnection.destroy) {
            // OTPlugin defines a destroy method on PCs. This allows
            // the plugin to release any resources that it's holding.
            _peerConnection.destroy();
          }

          _peerConnection = null;
          api.trigger('close');
        }
      },

      routeStateChanged = function() {
        var newState = _peerConnection.signalingState;

        if (newState && newState !== _state) {
          _state = newState;
          OT.debug('PeerConnection.stateChange: ' + _state);

          switch (_state) {
            case 'closed':
              tearDownPeerConnection();
              break;
          }
        }
      },

      qosCallback = function(parsedStats) {
        parsedStats.dataChannels = _channels.sampleQos();
        api.trigger('qos', parsedStats);
      },

      getRemoteStreams = function() {
        var streams;

        if (_peerConnection.getRemoteStreams) {
          streams = _peerConnection.getRemoteStreams();
        } else if (_peerConnection.remoteStreams) {
          streams = _peerConnection.remoteStreams;
        } else {
          throw new Error('Invalid Peer Connection object implements no ' +
            'method for retrieving remote streams');
        }

        // Force streams to be an Array, rather than a 'Sequence' object,
        // which is browser dependent and does not behaviour like an Array
        // in every case.
        return Array.prototype.slice.call(streams);
      },

      /// PeerConnection signaling
      onRemoteStreamAdded = function(event) {
        api.trigger('streamAdded', event.stream);
      },

      onRemoteStreamRemoved = function(event) {
        api.trigger('streamRemoved', event.stream);
      },

      // ICE Negotiation messages

      // Relays a SDP payload (+sdp+), that is part of a message of type +messageType+
      // via the registered message delegators
      relaySDP = function(messageType, sdp, uri) {
        delegateMessage(messageType, sdp, uri);
      },

      // Process an offer that
      processOffer = function(message) {
        var offer = new NativeRTCSessionDescription({type: 'offer', sdp: message.content.sdp}),

            // Relays +answer+ Answer
            relayAnswer = function(answer) {
              _iceProcessor.setPeerConnection(_peerConnection);
              _iceProcessor.processPending();
              relaySDP(OT.Raptor.Actions.ANSWER, answer);

              qos.startCollecting(_peerConnection);
            },

            reportError = function(message, errorReason, prefix) {
              triggerError('PeerConnection.offerProcessor ' + message + ': ' +
                errorReason, prefix);
            };

        createPeerConnection(function() {
          offerProcessor(
            _peerConnection,
            offer,
            relayAnswer,
            reportError
          );
        });
      },

      processAnswer = function(message) {
        if (!message.content.sdp) {
          OT.error('PeerConnection.processMessage: Weird answer message, no SDP.');
          return;
        }

        _answer = new NativeRTCSessionDescription({type: 'answer', sdp: message.content.sdp});

        _peerConnection.setRemoteDescription(_answer,
            function() {
              OT.debug('setRemoteDescription Success');
            }, function(errorReason) {
              triggerError('Error while setting RemoteDescription ' + errorReason,
                'SetRemoteDescription');
            });

        _iceProcessor.setPeerConnection(_peerConnection);
        _iceProcessor.processPending();

        qos.startCollecting(_peerConnection);
      },

      processSubscribe = function(message) {
        OT.debug('PeerConnection.processSubscribe: Sending offer to subscriber.');

        if (!_peerConnection) {
          // TODO(rolly) I need to examine whether this can
          // actually happen. If it does happen in the short
          // term, I want it to be noisy.
          throw new Error('PeerConnection broke!');
        }

        createPeerConnection(function() {
          subscribeProcessor(
            _peerConnection,
            config.numberOfSimulcastStreams,

            // Success: Relay Offer
            function(offer) {
              _offer = offer;
              relaySDP(OT.Raptor.Actions.OFFER, _offer, message.uri);
            },

            // Failure
            function(message, errorReason, prefix) {
              triggerError('PeerConnection.subscribeProcessor ' + message + ': ' +
                errorReason, prefix);
            }
          );
        });
      },

      triggerError = function(errorReason, prefix) {
        OT.error(errorReason);
        api.trigger('error', errorReason, prefix);
      };

  api.addLocalStream = function(webRTCStream) {
    createPeerConnection(function() {
      _peerConnection.addStream(webRTCStream);
    }, webRTCStream);
  };

  api.getSenders = function() {
    return _peerConnection.getSenders();
  };

  api.disconnect = function() {
    _iceProcessor = null;

    if (_peerConnection &&
        _peerConnection.signalingState &&
        _peerConnection.signalingState.toLowerCase() !== 'closed') {

      _peerConnection.close();

      if (OT.$.env.name === 'Firefox') {
        // FF seems to never go into the closed signalingState when the close
        // method is called on a PeerConnection. This means that we need to call
        // our cleanup code manually.
        //
        // * https://bugzilla.mozilla.org/show_bug.cgi?id=989936
        //
        OT.$.callAsync(tearDownPeerConnection);
      }
    }

    api.off();
  };

  api.processMessage = function(type, message) {
    OT.debug('PeerConnection.processMessage: Received ' +
      type + ' from ' + message.fromAddress);

    OT.debug(message);

    switch (type) {
      case 'generateoffer':
        processSubscribe(message);
        break;

      case 'offer':
        processOffer(message);
        break;

      case 'answer':
      case 'pranswer':
        processAnswer(message);
        break;

      case 'candidate':
        _iceProcessor.process(message);
        break;

      default:
        OT.debug('PeerConnection.processMessage: Received an unexpected message of type ' +
          type + ' from ' + message.fromAddress + ': ' + JSON.stringify(message));
    }

    return api;
  };

  api.setIceServers = function(iceServers) {
    if (iceServers) {
      config.iceServers = iceServers;
    }
  };

  api.registerMessageDelegate = function(delegateFn) {
    return _messageDelegates.push(delegateFn);
  };

  api.unregisterMessageDelegate = function(delegateFn) {
    var index = OT.$.arrayIndexOf(_messageDelegates, delegateFn);

    if (index !== -1) {
      _messageDelegates.splice(index, 1);
    }
    return _messageDelegates.length;
  };

  api.remoteStreams = function() {
    return _peerConnection ? getRemoteStreams() : [];
  };

  api.getStats = function(callback) {
    if (!_peerConnection) {
      callback(new OT.$.Error('Cannot call getStats before there is a connection.', 1015));
      return;
    }
    _getStatsAdapter(_peerConnection, callback);
  };

  var waitForChannel = function waitForChannel(timesToWait, label, options, completion) {
    var channel = _channels.get(label, options),
        err;

    if (!channel) {
      if (timesToWait > 0) {
        setTimeout(OT.$.bind(waitForChannel, null, timesToWait - 1, label, options, completion),
          200);
        return;
      }

      err = new OT.$.Error('A channel with that label and options could not be found. ' +
                            'Label:' + label + '. Options: ' + JSON.stringify(options));
    }

    completion(err, channel);
  };

  api.getDataChannel = function(label, options, completion) {
    if (!_peerConnection) {
      completion(new OT.$.Error('Cannot create a DataChannel before there is a connection.'));
      return;
    }
    // Wait up to 20 sec for the channel to appear, then fail
    waitForChannel(100, label, options, completion);
  };

  var qos = new OT.PeerConnection.QOS(qosCallback);

  return api;
};

// tb_require('../../helpers/helpers.js')
// tb_require('./peer_connection.js')

//
// There are three implementations of stats parsing in this file.
// 1. For Chrome: Chrome is currently using an older version of the API
// 2. For OTPlugin: The plugin is using a newer version of the API that
//    exists in the latest WebRTC codebase
// 3. For Firefox: FF is using a version that looks a lot closer to the
//    current spec.
//
// I've attempted to keep the three implementations from sharing any code,
// accordingly you'll notice a bunch of duplication between the three.
//
// This is acceptable as the goal is to be able to remove each implementation
// as it's no longer needed without any risk of affecting the others. If there
// was shared code between them then each removal would require an audit of
// all the others.
//
//
!(function() {

  ///
  // Get Stats using the older API. Used by all current versions
  // of Chrome.
  //
  var parseStatsOldAPI = function parseStatsOldAPI(peerConnection,
                                                   prevStats,
                                                   currentStats,
                                                   completion) {

    /* this parses a result if there it contains the video bitrate */
    var parseVideoStats = function(result) {
          if (result.stat('googFrameRateSent')) {
            currentStats.videoSentBytes = Number(result.stat('bytesSent'));
            currentStats.videoSentPackets = Number(result.stat('packetsSent'));
            currentStats.videoSentPacketsLost = Number(result.stat('packetsLost'));
            currentStats.videoRtt = Number(result.stat('googRtt'));
            currentStats.videoFrameRate = Number(result.stat('googFrameRateInput'));
            currentStats.videoWidth = Number(result.stat('googFrameWidthSent'));
            currentStats.videoHeight = Number(result.stat('googFrameHeightSent'));
            currentStats.videoFrameRateSent = Number(result.stat('googFrameRateSent'));
            currentStats.videoWidthInput = Number(result.stat('googFrameWidthInput'));
            currentStats.videoHeightInput = Number(result.stat('googFrameHeightInput'));
            currentStats.videoCodec = result.stat('googCodecName');
          } else if (result.stat('googFrameRateReceived')) {
            currentStats.videoRecvBytes = Number(result.stat('bytesReceived'));
            currentStats.videoRecvPackets = Number(result.stat('packetsReceived'));
            currentStats.videoRecvPacketsLost = Number(result.stat('packetsLost'));
            currentStats.videoFrameRate = Number(result.stat('googFrameRateOutput'));
            currentStats.videoFrameRateReceived = Number(result.stat('googFrameRateReceived'));
            currentStats.videoFrameRateDecoded = Number(result.stat('googFrameRateDecoded'));
            currentStats.videoWidth = Number(result.stat('googFrameWidthReceived'));
            currentStats.videoHeight = Number(result.stat('googFrameHeightReceived'));
            currentStats.videoCodec = result.stat('googCodecName');
          }

          return null;
        },

        parseAudioStats = function(result) {
          if (result.stat('audioInputLevel')) {
            currentStats.audioSentPackets = Number(result.stat('packetsSent'));
            currentStats.audioSentPacketsLost = Number(result.stat('packetsLost'));
            currentStats.audioSentBytes =  Number(result.stat('bytesSent'));
            currentStats.audioCodec = result.stat('googCodecName');
            currentStats.audioRtt = Number(result.stat('googRtt'));
          } else if (result.stat('audioOutputLevel')) {
            currentStats.audioRecvPackets = Number(result.stat('packetsReceived'));
            currentStats.audioRecvPacketsLost = Number(result.stat('packetsLost'));
            currentStats.audioRecvBytes =  Number(result.stat('bytesReceived'));
            currentStats.audioCodec = result.stat('googCodecName');
          }
        },

        parseStatsReports = function(stats) {
          if (stats.result) {
            var resultList = stats.result();
            for (var resultIndex = 0; resultIndex < resultList.length; resultIndex++) {
              var result = resultList[resultIndex];

              if (result.stat) {
                if (result.stat('googActiveConnection') === 'true') {
                  currentStats.localCandidateType = result.stat('googLocalCandidateType');
                  currentStats.remoteCandidateType = result.stat('googRemoteCandidateType');
                  currentStats.transportType = result.stat('googTransportType');
                }

                parseAudioStats(result);
                parseVideoStats(result);
              }
            }
          }

          completion(null, currentStats);
        };

    peerConnection.getStats(parseStatsReports);
  };

  ///
  // Get Stats for the OT Plugin, newer than Chromes version, but
  // still not in sync with the spec.
  //
  var parseStatsOTPlugin = function parseStatsOTPlugin(peerConnection,
                                                    prevStats,
                                                    currentStats,
                                                    completion) {

    var onStatsError = function onStatsError(error) {
          completion(error);
        },

        ///
        // From the Audio Tracks
        // * avgAudioBitrate
        // * audioBytesTransferred
        //
        parseAudioStats = function(statsReport) {
          var lastBytesSent = prevStats.audioBytesTransferred || 0,
              transferDelta;

          if (statsReport.audioInputLevel) {
            currentStats.audioSentBytes = Number(statsReport.bytesSent);
            currentStats.audioSentPackets = Number(statsReport.packetsSent);
            currentStats.audioSentPacketsLost = Number(statsReport.packetsLost);
            currentStats.audioRtt = Number(statsReport.googRtt);
            currentStats.audioCodec = statsReport.googCodecName;
          } else if (statsReport.audioOutputLevel) {
            currentStats.audioBytesTransferred = Number(statsReport.bytesReceived);
            currentStats.audioCodec = statsReport.googCodecName;
          }

          if (currentStats.audioBytesTransferred) {
            transferDelta = currentStats.audioBytesTransferred - lastBytesSent;
            currentStats.avgAudioBitrate = Math.round(transferDelta * 8 /
              (currentStats.period / 1000));
          }
        },

        ///
        // From the Video Tracks
        // * frameRate
        // * avgVideoBitrate
        // * videoBytesTransferred
        //
        parseVideoStats = function(statsReport) {

          var lastBytesSent = prevStats.videoBytesTransferred || 0,
              transferDelta;

          if (statsReport.googFrameHeightSent) {
            currentStats.videoSentBytes = Number(statsReport.bytesSent);
            currentStats.videoSentPackets = Number(statsReport.packetsSent);
            currentStats.videoSentPacketsLost = Number(statsReport.packetsLost);
            currentStats.videoRtt = Number(statsReport.googRtt);
            currentStats.videoCodec = statsReport.googCodecName;
            currentStats.videoWidth = Number(statsReport.googFrameWidthSent);
            currentStats.videoHeight = Number(statsReport.googFrameHeightSent);
            currentStats.videoFrameRateSent = Number(statsReport.googFrameRateSent);
            currentStats.videoWidthInput = Number(statsReport.googFrameWidthInput);
            currentStats.videoHeightInput = Number(statsReport.googFrameHeightInput);
          } else if (statsReport.googFrameHeightReceived) {
            currentStats.videoRecvBytes =   Number(statsReport.bytesReceived);
            currentStats.videoRecvPackets = Number(statsReport.packetsReceived);
            currentStats.videoRecvPacketsLost = Number(statsReport.packetsLost);
            currentStats.videoRtt = Number(statsReport.googRtt);
            currentStats.videoCodec = statsReport.googCodecName;
            currentStats.videoFrameRateReceived = Number(statsReport.googFrameRateReceived);
            currentStats.videoFrameRateDecoded = Number(statsReport.googFrameRateDecoded);
            currentStats.videoWidth = Number(statsReport.googFrameWidthReceived);
            currentStats.videoHeight = Number(statsReport.googFrameHeightReceived);
          }

          if (currentStats.videoBytesTransferred) {
            transferDelta = currentStats.videoBytesTransferred - lastBytesSent;
            currentStats.avgVideoBitrate = Math.round(transferDelta * 8 /
             (currentStats.period / 1000));
          }

          if (statsReport.googFrameRateInput) {
            currentStats.videoFrameRate = Number(statsReport.googFrameRateInput);
          } else if (statsReport.googFrameRateOutput) {
            currentStats.videoFrameRate = Number(statsReport.googFrameRateOutput);
          }
        },

        isStatsForVideoTrack = function(statsReport) {
          return statsReport.googFrameHeightSent !== void 0 ||
                  statsReport.googFrameHeightReceived !== void 0 ||
                  currentStats.videoBytesTransferred !== void 0 ||
                  statsReport.googFrameRateSent !== void 0;
        },

        isStatsForIceCandidate = function(statsReport) {
          return statsReport.googActiveConnection === 'true';
        };

    peerConnection.getStats(null, function(statsReports) {
      statsReports.forEach(function(statsReport) {
        if (isStatsForIceCandidate(statsReport)) {
          currentStats.localCandidateType = statsReport.googLocalCandidateType;
          currentStats.remoteCandidateType = statsReport.googRemoteCandidateType;
          currentStats.transportType = statsReport.googTransportType;
        } else if (isStatsForVideoTrack(statsReport)) {
          parseVideoStats(statsReport);
        } else {
          parseAudioStats(statsReport);
        }
      });

      completion(null, currentStats);
    }, onStatsError);
  };

  ///
  // Get Stats using the newer API.
  //
  var parseStatsNewAPI = function parseStatsNewAPI(peerConnection,
                                                   prevStats,
                                                   currentStats,
                                                   completion) {

    var onStatsError = function onStatsError(error) {
          completion(error);
        },

        parseAudioStats = function(result) {
          if (result.type === 'outboundrtp') {
            currentStats.audioSentPackets = result.packetsSent;
            currentStats.audioSentPacketsLost = result.packetsLost;
            currentStats.audioSentBytes =  result.bytesSent;
          } else if (result.type === 'inboundrtp') {
            currentStats.audioRecvPackets = result.packetsReceived;
            currentStats.audioRecvPacketsLost = result.packetsLost;
            currentStats.audioRecvBytes =  result.bytesReceived;
          }
        },

        parseVideoStats = function(result) {
          if (result.type === 'outboundrtp') {
            currentStats.videoSentPackets = result.packetsSent;
            currentStats.videoSentPacketsLost = result.packetsLost;
            currentStats.videoSentBytes =  result.bytesSent;
          } else if (result.type === 'inboundrtp') {
            currentStats.videoRecvPackets = result.packetsReceived;
            currentStats.videoRecvPacketsLost = result.packetsLost;
            currentStats.videoRecvBytes =  result.bytesReceived;
          }
        };

    peerConnection.getStats(null, function(stats) {

      for (var key in stats) {
        if (stats.hasOwnProperty(key) &&
          (stats[key].type === 'outboundrtp' || stats[key].type === 'inboundrtp')) {
          var res = stats[key];

          if (res.id.indexOf('rtp') !== -1) {
            if (res.id.indexOf('audio') !== -1) {
              parseAudioStats(res);
            } else if (res.id.indexOf('video') !== -1) {
              parseVideoStats(res);
            }
          }
        }
      }

      completion(null, currentStats);
    }, onStatsError);
  };

  var parseQOS = function(peerConnection, prevStats, currentStats, completion) {
    if (OTPlugin.isInstalled()) {
      parseQOS = parseStatsOTPlugin;
      return parseStatsOTPlugin(peerConnection, prevStats, currentStats, completion);
    } else if (OT.$.env.name === 'Firefox' && OT.$.env.version >= 27) {
      parseQOS = parseStatsNewAPI;
      return parseStatsNewAPI(peerConnection, prevStats, currentStats, completion);
    } else {
      parseQOS = parseStatsOldAPI;
      return parseStatsOldAPI(peerConnection, prevStats, currentStats, completion);
    }
  };

  OT.PeerConnection.QOS = function(qosCallback) {
    var _creationTime = OT.$.now(),
        _peerConnection;

    var calculateQOS = OT.$.bind(function calculateQOS(prevStats, interval) {
      if (!_peerConnection) {
        // We don't have a PeerConnection yet, or we did and
        // it's been closed. Either way we're done.
        return;
      }

      var now = OT.$.now();

      var currentStats = {
        timeStamp: now,
        duration: Math.round(now - _creationTime),
        period: Math.round(now - prevStats.timeStamp)
      };

      var onParsedStats = function(err, parsedStats) {
        if (err) {
          OT.error('Failed to Parse QOS Stats: ' + JSON.stringify(err));
          return;
        }

        qosCallback(parsedStats, prevStats);

        var nextInterval = interval === OT.PeerConnection.QOS.INITIAL_INTERVAL ?
          OT.PeerConnection.QOS.INTERVAL - OT.PeerConnection.QOS.INITIAL_INTERVAL :
          OT.PeerConnection.QOS.INTERVAL;

        // Recalculate the stats
        setTimeout(OT.$.bind(calculateQOS, null, parsedStats, nextInterval),
          interval);
      };

      parseQOS(_peerConnection, prevStats, currentStats, onParsedStats);
    }, this);

    this.startCollecting = function(peerConnection) {
      if (!peerConnection || !peerConnection.getStats) {
        // It looks like this browser doesn't support getStats
        // Bail.
        return;
      }

      _peerConnection = peerConnection;

      calculateQOS({timeStamp: OT.$.now()}, OT.PeerConnection.QOS.INITIAL_INTERVAL);
    };

    this.stopCollecting = function() {
      _peerConnection = null;
    };
  };

  // Send stats after 1 sec
  OT.PeerConnection.QOS.INITIAL_INTERVAL = 1000;
  // Recalculate the stats every 30 sec
  OT.PeerConnection.QOS.INTERVAL = 30000;
})();

// tb_require('../../helpers/helpers.js')
// tb_require('./peer_connection.js')

OT.PeerConnections = (function() {
  var _peerConnections = {};

  return {
    add: function(remoteConnection, streamId, config) {
      var key = remoteConnection.id + '_' + streamId,
          ref = _peerConnections[key];

      if (!ref) {
        ref = _peerConnections[key] = {
          count: 0,
          pc: new OT.PeerConnection(config)
        };
      }

      // increase the PCs ref count by 1
      ref.count += 1;

      return ref.pc;
    },

    remove: function(remoteConnection, streamId) {
      var key = remoteConnection.id + '_' + streamId,
          ref = _peerConnections[key];

      if (ref) {
        ref.count -= 1;

        if (ref.count === 0) {
          ref.pc.disconnect();
          delete _peerConnections[key];
        }
      }
    }
  };
})();

// tb_require('../../helpers/helpers.js')
// tb_require('../messaging/raptor/raptor.js')
// tb_require('./peer_connections.js')
// tb_require('./set_certificates.js')

/* global _setCertificates */

/*
 * Abstracts PeerConnection related stuff away from OT.Subscriber.
 *
 * Responsible for:
 * * setting up the underlying PeerConnection (delegates to OT.PeerConnections)
 * * triggering a connected event when the Peer connection is opened
 * * triggering a disconnected event when the Peer connection is closed
 * * creating a video element when a stream is added
 * * responding to stream removed intelligently
 * * providing a destroy method
 * * providing a processMessage method
 *
 * Once the PeerConnection is connected and the video element playing it
 * triggers the connected event
 *
 * Triggers the following events
 * * connected
 * * disconnected
 * * remoteStreamAdded
 * * remoteStreamRemoved
 * * error
 *
 */

OT.SubscriberPeerConnection = function(remoteConnection, session, stream,
  subscriber, properties) {
  var _subscriberPeerConnection = this,
      _peerConnection,
      _destroyed = false,
      _hasRelayCandidates = false,
      _onPeerClosed,
      _onRemoteStreamAdded,
      _onRemoteStreamRemoved,
      _onPeerError,
      _relayMessageToPeer,
      _setEnabledOnStreamTracksCurry,
      _onQOS;

  // Private
  _onPeerClosed = function() {
    this.destroy();
    this.trigger('disconnected', this);
  };

  _onRemoteStreamAdded = function(remoteRTCStream) {
    this.trigger('remoteStreamAdded', remoteRTCStream, this);
  };

  _onRemoteStreamRemoved = function(remoteRTCStream) {
    this.trigger('remoteStreamRemoved', remoteRTCStream, this);
  };

  // Note: All Peer errors are fatal right now.
  _onPeerError = function(errorReason, prefix) {
    this.trigger('error', null, errorReason, this, prefix);
  };

  _relayMessageToPeer = OT.$.bind(function(type, payload) {
    if (!_hasRelayCandidates) {
      var extractCandidates = type === OT.Raptor.Actions.CANDIDATE ||
                              type === OT.Raptor.Actions.OFFER ||
                              type === OT.Raptor.Actions.ANSWER ||
                              type === OT.Raptor.Actions.PRANSWER ;

      if (extractCandidates) {
        var message = (type === OT.Raptor.Actions.CANDIDATE) ? payload.candidate : payload.sdp;
        _hasRelayCandidates = message.indexOf('typ relay') !== -1;
      }
    }

    switch (type) {
      case OT.Raptor.Actions.ANSWER:
      case OT.Raptor.Actions.PRANSWER:
        this.trigger('connected');

        session._.jsepAnswerP2p(stream.id, subscriber.widgetId, payload.sdp);
        break;

      case OT.Raptor.Actions.OFFER:
        session._.jsepOfferP2p(stream.id, subscriber.widgetId, payload.sdp);
        break;

      case OT.Raptor.Actions.CANDIDATE:
        session._.jsepCandidateP2p(stream.id, subscriber.widgetId, payload);
        break;
    }
  }, this);

  // Helper method used by subscribeToAudio/subscribeToVideo
  _setEnabledOnStreamTracksCurry = function(isVideo) {
    var method = 'get' + (isVideo ? 'Video' : 'Audio') + 'Tracks';

    return function(enabled) {
      var remoteStreams = _peerConnection.remoteStreams(),
          tracks,
          stream;

      if (remoteStreams.length === 0 || !remoteStreams[0][method]) {
        // either there is no remote stream or we are in a browser that doesn't
        // expose the media tracks (Firefox)
        return;
      }

      for (var i = 0, num = remoteStreams.length; i < num; ++i) {
        stream = remoteStreams[i];
        tracks = stream[method]();

        for (var k = 0, numTracks = tracks.length; k < numTracks; ++k) {
          // Only change the enabled property if it's different
          // otherwise we get flickering of the video
          if (tracks[k].enabled !== enabled) tracks[k].setEnabled(enabled);
        }
      }
    };
  };

  _onQOS = OT.$.bind(function _onQOS(parsedStats, prevStats) {
    this.trigger('qos', parsedStats, prevStats);
  }, this);

  OT.$.eventing(this);

  // Public
  this.destroy = function() {
    if (_destroyed) return;
    _destroyed = true;

    if (_peerConnection) {
      var numDelegates = _peerConnection.unregisterMessageDelegate(_relayMessageToPeer);

      // Only clean up the PeerConnection if there isn't another Subscriber using it
      if (numDelegates === 0) {
        // Unsubscribe us from the stream, if it hasn't already been destroyed
        if (session && session.isConnected() && stream && !stream.destroyed) {
          // Notify the server components
          session._.subscriberDestroy(stream, subscriber);
        }

        // Ref: OPENTOK-2458 disable all audio tracks before removing it.
        this.subscribeToAudio(false);
      }
      OT.PeerConnections.remove(remoteConnection, stream.id);
    }
    _peerConnection = null;
    this.off();
  };

  this.getDataChannel = function(label, options, completion) {
    _peerConnection.getDataChannel(label, options, completion);
  };

  this.processMessage = function(type, message) {
    _peerConnection.processMessage(type, message);
  };

  this.getStats = function(callback) {
    if (_peerConnection) {
      _peerConnection.getStats(callback);
    } else {
      callback(new OT.$.Error('Subscriber is not connected cannot getStats', 1015));
    }
  };

  this.subscribeToAudio = _setEnabledOnStreamTracksCurry(false);
  this.subscribeToVideo = _setEnabledOnStreamTracksCurry(true);

  this.hasRelayCandidates = function() {
    return _hasRelayCandidates;
  };

  // Init
  this.init = function(completion) {
    var pcConfig = {};

    _setCertificates(pcConfig, function(err, pcConfigWithCerts) {
      if (err) {
        completion(err);
        return;
      }

      _peerConnection = OT.PeerConnections.add(
        remoteConnection,
        stream.streamId,
        pcConfigWithCerts
      );

      _peerConnection.on({
        close: _onPeerClosed,
        streamAdded: _onRemoteStreamAdded,
        streamRemoved: _onRemoteStreamRemoved,
        error: _onPeerError,
        qos: _onQOS
      }, _subscriberPeerConnection);

      var numDelegates = _peerConnection.registerMessageDelegate(_relayMessageToPeer);

      // If there are already remoteStreams, add them immediately
      if (_peerConnection.remoteStreams().length > 0) {
        OT.$.forEach(
          _peerConnection.remoteStreams(),
          _onRemoteStreamAdded,
          _subscriberPeerConnection
        );
      } else if (numDelegates === 1) {
        // We only bother with the PeerConnection negotiation if we don't already
        // have a remote stream.

        var channelsToSubscribeTo;

        if (properties.subscribeToVideo || properties.subscribeToAudio) {
          var audio = stream.getChannelsOfType('audio'),
              video = stream.getChannelsOfType('video');

          channelsToSubscribeTo = OT.$.map(audio, function(channel) {
            return {
              id: channel.id,
              type: channel.type,
              active: properties.subscribeToAudio
            };
          }).concat(OT.$.map(video, function(channel) {
            var props = {
              id: channel.id,
              type: channel.type,
              active: properties.subscribeToVideo,
              restrictFrameRate: properties.restrictFrameRate !== void 0 ?
                properties.restrictFrameRate : false
            };

            if (properties.maxFrameRate !== void 0) {
              props.maxFrameRate = parseFloat(properties.maxFrameRate);
            }

            if (properties.maxHeight !== void 0) {
              props.maxHeight = parseInt(properties.maxHeight, 10);
            }

            if (properties.maxWidth !== void 0) {
              props.maxWidth = parseInt(properties.maxWidth, 10);
            }

            return props;
          }));
        }

        session._.subscriberCreate(
          stream,
          subscriber,
          channelsToSubscribeTo,
            function(err, message) {
            if (err) {
              _subscriberPeerConnection.trigger(
                'error',
                err.message,
                _subscriberPeerConnection,
                'Subscribe'
              );
            }
            if (_peerConnection) {
              _peerConnection.setIceServers(OT.Raptor.parseIceServers(message));
            }
          }
        );

        completion(undefined, _subscriberPeerConnection);
      }
    });
  };
};

// tb_require('../../helpers/helpers.js')
// tb_require('../messaging/raptor/raptor.js')
// tb_require('./peer_connections.js')
// tb_require('./set_certificates.js')

/* global _setCertificates */

/*
 * Abstracts PeerConnection related stuff away from OT.Publisher.
 *
 * Responsible for:
 * * setting up the underlying PeerConnection (delegates to OT.PeerConnections)
 * * triggering a connected event when the Peer connection is opened
 * * triggering a disconnected event when the Peer connection is closed
 * * providing a destroy method
 * * providing a processMessage method
 *
 * Once the PeerConnection is connected and the video element playing it triggers
 * the connected event
 *
 * Triggers the following events
 * * connected
 * * disconnected
 */
OT.PublisherPeerConnection = function(remoteConnection, session, streamId, webRTCStream, channels,
    numberOfSimulcastStreams) {
  var _publisherPeerConnection = this,
      _peerConnection,
      _hasRelayCandidates = false,
      _subscriberId = session._.subscriberMap[remoteConnection.id + '_' + streamId],
      _onPeerClosed,
      _onPeerError,
      _relayMessageToPeer,
      _onQOS;

  // Private
  _onPeerClosed = function() {
    this.destroy();
    this.trigger('disconnected', this);
  };

  // Note: All Peer errors are fatal right now.
  _onPeerError = function(errorReason, prefix) {
    this.trigger('error', null, errorReason, this, prefix);
    this.destroy();
  };

  _relayMessageToPeer = OT.$.bind(function(type, payload, uri) {
    if (!_hasRelayCandidates) {
      var extractCandidates = type === OT.Raptor.Actions.CANDIDATE ||
                              type === OT.Raptor.Actions.OFFER ||
                              type === OT.Raptor.Actions.ANSWER ||
                              type === OT.Raptor.Actions.PRANSWER ;

      if (extractCandidates) {
        var message = (type === OT.Raptor.Actions.CANDIDATE) ? payload.candidate : payload.sdp;
        _hasRelayCandidates = message.indexOf('typ relay') !== -1;
      }
    }

    switch (type) {
      case OT.Raptor.Actions.ANSWER:
      case OT.Raptor.Actions.PRANSWER:
        if (session.sessionInfo.p2pEnabled) {
          session._.jsepAnswerP2p(streamId, _subscriberId, payload.sdp);
        } else {
          session._.jsepAnswer(streamId, payload.sdp);
        }

        break;

      case OT.Raptor.Actions.OFFER:
        this.trigger('connected');
        session._.jsepOffer(uri, payload.sdp);

        break;

      case OT.Raptor.Actions.CANDIDATE:
        if (session.sessionInfo.p2pEnabled) {
          session._.jsepCandidateP2p(streamId, _subscriberId, payload);

        } else {
          session._.jsepCandidate(streamId, payload);
        }
    }
  }, this);

  _onQOS = OT.$.bind(function _onQOS(parsedStats, prevStats) {
    this.trigger('qos', remoteConnection, parsedStats, prevStats);
  }, this);

  OT.$.eventing(this);

  /// Public API

  this.getDataChannel = function(label, options, completion) {
    _peerConnection.getDataChannel(label, options, completion);
  };

  this.destroy = function() {
    // Clean up our PeerConnection
    if (_peerConnection) {
      _peerConnection.off();
      OT.PeerConnections.remove(remoteConnection, streamId);
    }

    _peerConnection = null;
  };

  this.processMessage = function(type, message) {
    _peerConnection.processMessage(type, message);
  };

  // Init
  this.init = function(iceServers, completion) {
    var pcConfig = {
      iceServers: iceServers,
      channels: channels,
      numberOfSimulcastStreams: numberOfSimulcastStreams
    };

    _setCertificates(pcConfig, function(err, pcConfigWithCerts) {
      if (err) {
        completion(err);
        return;
      }

      _peerConnection = OT.PeerConnections.add(remoteConnection, streamId, pcConfigWithCerts);

      _peerConnection.on({
        close: _onPeerClosed,
        error: _onPeerError,
        qos: _onQOS
      }, _publisherPeerConnection);

      _peerConnection.registerMessageDelegate(_relayMessageToPeer);
      _peerConnection.addLocalStream(webRTCStream);

      completion(undefined, _publisherPeerConnection);
    });

    this.remoteConnection = function() {
      return remoteConnection;
    };

    this.hasRelayCandidates = function() {
      return _hasRelayCandidates;
    };
  };

  this.getSenders = function() {
    return _peerConnection.getSenders();
  };
};

// tb_require('../helpers.js')
// tb_require('./web_rtc.js')
// tb_require('./videoContentResizesMixin.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT, OTPlugin, videoContentResizesMixin */

(function(window) {

  var VideoOrientationTransforms = {
    0: 'rotate(0deg)',
    270: 'rotate(90deg)',
    90: 'rotate(-90deg)',
    180: 'rotate(180deg)'
  };

  OT.VideoOrientation = {
    ROTATED_NORMAL: 0,
    ROTATED_LEFT: 270,
    ROTATED_RIGHT: 90,
    ROTATED_UPSIDE_DOWN: 180
  };

  var DefaultAudioVolume = 50;

  var DEGREE_TO_RADIANS = Math.PI * 2 / 360;

  //
  //
  //   var _videoElement = new OT.VideoElement({
  //     fallbackText: 'blah'
  //   }, errorHandler);
  //
  //   _videoElement.bindToStream(webRtcStream, completion);      // => VideoElement
  //   _videoElement.appendTo(DOMElement)                         // => VideoElement
  //
  //   _videoElement.domElement                       // => DomNode
  //
  //   _videoElement.imgData                          // => PNG Data string
  //
  //   _videoElement.orientation = OT.VideoOrientation.ROTATED_LEFT;
  //
  //   _videoElement.unbindStream();
  //   _videoElement.destroy()                        // => Completely cleans up and
  //                                                        removes the video element
  //
  //
  OT.VideoElement = function(/* optional */ options/*, optional errorHandler*/) {
    var _options = OT.$.defaults(options && !OT.$.isFunction(options) ? options : {}, {
        fallbackText: 'Sorry, Web RTC is not available in your browser'
      }),

      errorHandler = OT.$.isFunction(arguments[arguments.length - 1]) ?
                                    arguments[arguments.length - 1] : void 0,

      orientationHandler = OT.$.bind(function(orientation) {
        this.trigger('orientationChanged', orientation);
      }, this),

      _videoElement = OTPlugin.isInstalled() ?
                            new PluginVideoElement(_options, errorHandler, orientationHandler) :
                            new NativeDOMVideoElement(_options, errorHandler, orientationHandler),
      _streamBound = false,
      _stream,
      _preInitialisedVolume;

    OT.$.eventing(this);

    _videoElement.on('videoDimensionsChanged', OT.$.bind(function(oldValue, newValue) {
      this.trigger('videoDimensionsChanged', oldValue, newValue);
    }, this));

    _videoElement.on('mediaStopped', OT.$.bind(function() {
      this.trigger('mediaStopped');
    }, this));

    // Public Properties
    OT.$.defineProperties(this, {

      domElement: {
        get: function() {
          return _videoElement.domElement();
        }
      },

      videoWidth: {
        get: function() {
          return _videoElement['video' + (this.isRotated() ? 'Height' : 'Width')]();
        }
      },

      videoHeight: {
        get: function() {
          return _videoElement['video' + (this.isRotated() ? 'Width' : 'Height')]();
        }
      },

      aspectRatio: {
        get: function() {
          return (this.videoWidth() + 0.0) / this.videoHeight();
        }
      },

      isRotated: {
        get: function() {
          return _videoElement.isRotated();
        }
      },

      orientation: {
        get: function() {
          return _videoElement.orientation();
        },
        set: function(orientation) {
          _videoElement.orientation(orientation);
        }
      },

      audioChannelType: {
        get: function() {
          return _videoElement.audioChannelType();
        },
        set: function(type) {
          _videoElement.audioChannelType(type);
        }
      }
    });

    // Public Methods

    this.imgData = function() {
      return _videoElement.imgData();
    };

    this.appendTo = function(parentDomElement) {
      _videoElement.appendTo(parentDomElement);
      return this;
    };

    this.bindToStream = function(webRtcStream, completion) {
      _streamBound = false;
      _stream = webRtcStream;

      _videoElement.bindToStream(webRtcStream, OT.$.bind(function(err) {
        if (err) {
          completion(err);
          return;
        }

        _streamBound = true;

        if (typeof _preInitialisedVolume !== 'undefined') {
          this.setAudioVolume(_preInitialisedVolume);
          _preInitialisedVolume = undefined;
        }

        _videoElement.on('aspectRatioAvailable',
          OT.$.bind(this.trigger, this, 'aspectRatioAvailable'));

        completion(null);
      }, this));

      return this;
    };

    this.unbindStream = function() {
      if (!_stream) return this;

      _stream.onended = null;
      _stream = null;
      _videoElement.unbindStream();
      return this;
    };

    this.setAudioVolume = function(value) {
      if (_streamBound) _videoElement.setAudioVolume(OT.$.roundFloat(value / 100, 2));
      else _preInitialisedVolume = parseFloat(value);

      return this;
    };

    this.getAudioVolume = function() {
      if (_streamBound) {
        return parseInt(_videoElement.getAudioVolume() * 100, 10);
      }

      if (typeof _preInitialisedVolume !== 'undefined') {
        return _preInitialisedVolume;
      }

      return 50;
    };

    this.whenTimeIncrements = function(callback, context) {
      _videoElement.whenTimeIncrements(callback, context);
      return this;
    };

    this.onRatioAvailable = function(callabck) {
      _videoElement.onRatioAvailable(callabck) ;
      return this;
    };

    this.destroy = function() {
      // unbind all events so they don't fire after the object is dead
      this.off();

      _videoElement.destroy();
      return void 0;
    };
  };

  var PluginVideoElement = function PluginVideoElement(options,
                                                       errorHandler,
                                                       orientationChangedHandler) {
    var _videoProxy,
        _parentDomElement,
        _ratioAvailable = false,
        _ratioAvailableListeners = [];

    OT.$.eventing(this);

    canBeOrientatedMixin(this,
                          function() { return _videoProxy.domElement; },
                          orientationChangedHandler);

    /// Public methods

    this.domElement = function() {
      return _videoProxy ? _videoProxy.domElement : void 0;
    };

    this.videoWidth = function() {
      return _videoProxy ? _videoProxy.videoWidth() : void 0;
    };

    this.videoHeight = function() {
      return _videoProxy ? _videoProxy.videoHeight() : void 0;
    };

    this.imgData = function() {
      return _videoProxy ? _videoProxy.getImgData() : null;
    };

    // Append the Video DOM element to a parent node
    this.appendTo = function(parentDomElement) {
      _parentDomElement = parentDomElement;
      return this;
    };

    // Bind a stream to the video element.
    this.bindToStream = function(webRtcStream, completion) {
      if (!_parentDomElement) {
        completion('The VideoElement must attached to a DOM node before a stream can be bound');
        return;
      }

      _videoProxy = webRtcStream._.render();
      _videoProxy.appendTo(_parentDomElement);
      _videoProxy.show(function(error) {

        if (!error) {
          _ratioAvailable = true;
          var listener;
          while ((listener = _ratioAvailableListeners.shift())) {
            listener();
          }
        }

        completion(error);
      });

      return this;
    };

    // Unbind the currently bound stream from the video element.
    this.unbindStream = function() {
      // TODO: some way to tell OTPlugin to release that stream and controller

      if (_videoProxy) {
        _videoProxy.destroy();
        _parentDomElement = null;
        _videoProxy = null;
      }

      return this;
    };

    this.setAudioVolume = function(value) {
      if (_videoProxy) _videoProxy.volume(value);
    };

    this.getAudioVolume = function() {
      // Return the actual volume of the DOM element
      if (_videoProxy) return _videoProxy.volume();
      return DefaultAudioVolume;
    };

    // see https://wiki.mozilla.org/WebAPI/AudioChannels
    // The audioChannelType is not currently supported in the plugin.
    this.audioChannelType = function(/* type */) {
      return 'unknown';
    };

    this.whenTimeIncrements = function(callback, context) {
      // exists for compatibility with NativeVideoElement
      OT.$.callAsync(OT.$.bind(callback, context));
    };

    this.onRatioAvailable = function(callback) {
      if (_ratioAvailable) {
        callback();
      } else {
        _ratioAvailableListeners.push(callback);
      }
    };

    this.destroy = function() {
      this.unbindStream();

      return void 0;
    };
  };

  var NativeDOMVideoElement = function NativeDOMVideoElement(options,
                                                             errorHandler,
                                                             orientationChangedHandler) {
    var _domElement,
        _videoElementMovedWarning = false;

    OT.$.eventing(this);

    /// Private API
    var _onVideoError = OT.$.bind(function(event) {
          var reason = 'There was an unexpected problem with the Video Stream: ' +
                        videoElementErrorCodeToStr(event.target.error.code);
          errorHandler(reason, this, 'VideoElement');
        }, this),

        // The video element pauses itself when it's reparented, this is
        // unfortunate. This function plays the video again and is triggered
        // on the pause event.
        _playVideoOnPause = function() {
          if (!_videoElementMovedWarning) {
            OT.warn('Video element paused, auto-resuming. If you intended to do this, ' +
                      'use publishVideo(false) or subscribeToVideo(false) instead.');

            _videoElementMovedWarning = true;
          }

          _domElement.play();
        };

    _domElement = createNativeVideoElement(options.fallbackText, options.attributes);

    // dirty but it is the only way right now to get the aspect ratio in FF
    // any other event is triggered too early
    var ratioAvailable = false;
    var ratioAvailableListeners = [];
    _domElement.addEventListener('timeupdate', function timeupdateHandler() {
      if (!_domElement) {
        event.target.removeEventListener('timeupdate', timeupdateHandler);
        return;
      }
      var aspectRatio = _domElement.videoWidth / _domElement.videoHeight;
      if (!isNaN(aspectRatio)) {
        _domElement.removeEventListener('timeupdate', timeupdateHandler);
        ratioAvailable = true;
        var listener;
        while ((listener = ratioAvailableListeners.shift())) {
          listener();
        }
      }
    });

    _domElement.addEventListener('pause', _playVideoOnPause);

    videoContentResizesMixin(this, _domElement);

    canBeOrientatedMixin(this, function() { return _domElement; }, orientationChangedHandler);

    /// Public methods

    this.domElement = function() {
      return _domElement;
    };

    this.videoWidth = function() {
      return _domElement ? _domElement.videoWidth : 0;
    };

    this.videoHeight = function() {
      return _domElement ? _domElement.videoHeight : 0;
    };

    this.imgData = function() {
      var canvas = OT.$.createElement('canvas', {
        width: _domElement.videoWidth,
        height: _domElement.videoHeight,
        style: { display: 'none' }
      });

      document.body.appendChild(canvas);
      try {
        canvas.getContext('2d').drawImage(_domElement, 0, 0, canvas.width, canvas.height);
      } catch (err) {
        OT.warn('Cannot get image data yet');
        return null;
      }
      var imgData = canvas.toDataURL('image/png');

      OT.$.removeElement(canvas);

      return OT.$.trim(imgData.replace('data:image/png;base64,', ''));
    };

    // Append the Video DOM element to a parent node
    this.appendTo = function(parentDomElement) {
      parentDomElement.appendChild(_domElement);
      return this;
    };

    // Bind a stream to the video element.
    this.bindToStream = function(webRtcStream, completion) {
      var _this = this;

      bindStreamToNativeVideoElement(_domElement, webRtcStream, function(err) {
        if (err) {
          completion(err);
          return;
        }

        if (!_domElement) {
          completion('Can\'t bind because _domElement no longer exists');
          return;
        }

        _this.startObservingSize();

        function handleEnded() {
          webRtcStream.onended = null;

          if (_domElement) {
            _domElement.onended = null;
          }

          _this.trigger('mediaStopped', this);
        }

        // OPENTOK-22428: Firefox doesn't emit the ended event on the webRtcStream when the user
        // stops sharing their camera, but we do get the ended event on the video element.
        webRtcStream.onended = handleEnded;
        _domElement.onended = handleEnded;

        _domElement.addEventListener('error', _onVideoError, false);
        completion(null);
      });

      return this;
    };

    // Unbind the currently bound stream from the video element.
    this.unbindStream = function() {
      if (_domElement) {
        unbindNativeStream(_domElement);
      }

      this.stopObservingSize();

      return this;
    };

    this.setAudioVolume = function(value) {
      if (_domElement) _domElement.volume = value;
    };

    this.getAudioVolume = function() {
      // Return the actual volume of the DOM element
      if (_domElement) return _domElement.volume;
      return DefaultAudioVolume;
    };

    // see https://wiki.mozilla.org/WebAPI/AudioChannels
    // The audioChannelType is currently only available in Firefox. This property returns
    // "unknown" in other browser. The related HTML tag attribute is "mozaudiochannel"
    this.audioChannelType = function(type) {
      if (type !== void 0) {
        _domElement.mozAudioChannelType = type;
      }

      if ('mozAudioChannelType' in _domElement) {
        return _domElement.mozAudioChannelType;
      } else {
        return 'unknown';
      }
    };

    this.whenTimeIncrements = function(callback, context) {
      if (_domElement) {
        var lastTime, handler;
        handler = OT.$.bind(function() {
          if (_domElement) {
            if (!lastTime || lastTime >= _domElement.currentTime) {
              lastTime = _domElement.currentTime;
            } else {
              _domElement.removeEventListener('timeupdate', handler, false);
              callback.call(context, this);
            }
          }
        }, this);
        _domElement.addEventListener('timeupdate', handler, false);
      }
    };

    this.destroy = function() {
      this.unbindStream();

      if (_domElement) {
        // Unbind this first, otherwise it will trigger when the
        // video element is removed from the DOM.
        _domElement.removeEventListener('pause', _playVideoOnPause);

        OT.$.removeElement(_domElement);
        _domElement = null;
      }

      return void 0;
    };

    this.onRatioAvailable = function(callback) {
      if (ratioAvailable) {
        callback();
      } else {
        ratioAvailableListeners.push(callback);
      }
    };
  };

  /// Private Helper functions

  // A mixin to create the orientation API implementation on +self+
  // +getDomElementCallback+ is a function that the mixin will call when it wants to
  // get the native Dom element for +self+.
  //
  // +initialOrientation+ sets the initial orientation (shockingly), it's currently unused
  // so the initial value is actually undefined.
  //
  var canBeOrientatedMixin = function canBeOrientatedMixin(self,
                                                           getDomElementCallback,
                                                           orientationChangedHandler,
                                                           initialOrientation) {
    var _orientation = initialOrientation;

    OT.$.defineProperties(self, {
      isRotated: {
        get: function() {
          return this.orientation() &&
                    (this.orientation().videoOrientation === 270 ||
                     this.orientation().videoOrientation === 90);
        }
      },

      orientation: {
        get: function() { return _orientation; },
        set: function(orientation) {
          _orientation = orientation;

          var transform = VideoOrientationTransforms[orientation.videoOrientation] ||
                          VideoOrientationTransforms.ROTATED_NORMAL;

          switch (OT.$.env.name) {
            case 'Chrome':
            case 'Safari':
              getDomElementCallback().style.webkitTransform = transform;
              break;

            case 'IE':
              if (OT.$.env.version >= 9) {
                getDomElementCallback().style.msTransform = transform;
              } else {
                // So this basically defines matrix that represents a rotation
                // of a single vector in a 2d basis.
                //
                //    R =  [cos(Theta) -sin(Theta)]
                //         [sin(Theta)  cos(Theta)]
                //
                // Where Theta is the number of radians to rotate by
                //
                // Then to rotate the vector v:
                //    v' = Rv
                //
                // We then use IE8 Matrix filter property, which takes
                // a 2x2 rotation matrix, to rotate our DOM element.
                //
                var radians = orientation.videoOrientation * DEGREE_TO_RADIANS,
                    element = getDomElementCallback(),
                    costheta = Math.cos(radians),
                    sintheta = Math.sin(radians);

                // element.filters.item(0).M11 = costheta;
                // element.filters.item(0).M12 = -sintheta;
                // element.filters.item(0).M21 = sintheta;
                // element.filters.item(0).M22 = costheta;

                element.style.filter = 'progid:DXImageTransform.Microsoft.Matrix(' +
                                          'M11=' + costheta + ',' +
                                          'M12=' + (-sintheta) + ',' +
                                          'M21=' + sintheta + ',' +
                                          'M22=' + costheta + ',SizingMethod=\'auto expand\')';
              }

              break;

            default:
              // The standard version, just Firefox, Opera, and IE > 9
              getDomElementCallback().style.transform = transform;
          }

          orientationChangedHandler(_orientation);

        }
      },

      // see https://wiki.mozilla.org/WebAPI/AudioChannels
      // The audioChannelType is currently only available in Firefox. This property returns
      // "unknown" in other browser. The related HTML tag attribute is "mozaudiochannel"
      audioChannelType: {
        get: function() {
          if ('mozAudioChannelType' in this.domElement) {
            return this.domElement.mozAudioChannelType;
          } else {
            return 'unknown';
          }
        },
        set: function(type) {
          if ('mozAudioChannelType' in this.domElement) {
            this.domElement.mozAudioChannelType = type;
          }
        }
      }
    });
  };

  function createNativeVideoElement(fallbackText, attributes) {
    var videoElement = document.createElement('video');
    videoElement.setAttribute('autoplay', '');
    videoElement.innerHTML = fallbackText;

    if (attributes) {
      if (attributes.muted === true) {
        delete attributes.muted;
        videoElement.muted = 'true';
      }

      for (var key in attributes) {
        if (!attributes.hasOwnProperty(key)) {
          continue;
        }
        videoElement.setAttribute(key, attributes[key]);
      }
    }

    return videoElement;
  }

  // See http://www.w3.org/TR/2010/WD-html5-20101019/video.html#error-codes
  var _videoErrorCodes = {};

  // Checking for window.MediaError for IE compatibility, just so we don't throw
  // exceptions when the script is included
  if (window.MediaError) {
    _videoErrorCodes[window.MediaError.MEDIA_ERR_ABORTED] = 'The fetching process for the media ' +
      'resource was aborted by the user agent at the user\'s request.';
    _videoErrorCodes[window.MediaError.MEDIA_ERR_NETWORK] = 'A network error of some description ' +
      'caused the user agent to stop fetching the media resource, after the resource was ' +
      'established to be usable.';
    _videoErrorCodes[window.MediaError.MEDIA_ERR_DECODE] = 'An error of some description ' +
      'occurred while decoding the media resource, after the resource was established to be ' +
      ' usable.';
    _videoErrorCodes[window.MediaError.MEDIA_ERR_SRC_NOT_SUPPORTED] = 'The media resource ' +
      'indicated by the src attribute was not suitable.';
  }

  function videoElementErrorCodeToStr(errorCode) {
    return _videoErrorCodes[parseInt(errorCode, 10)] || 'An unknown error occurred.';
  }

  function bindStreamToNativeVideoElement(videoElement, webRtcStream, completion) {
    var timeout,
        minVideoTracksForTimeUpdate = OT.$.env.name === 'Chrome' ? 1 : 0,
        loadedEvent = webRtcStream.getVideoTracks().length > minVideoTracksForTimeUpdate ?
          'timeupdate' : 'loadedmetadata';

    var cleanup = function cleanup() {
          clearTimeout(timeout);
          videoElement.removeEventListener(loadedEvent, onLoad, false);
          videoElement.removeEventListener('error', onError, false);
          webRtcStream.onended = null;
          videoElement.onended = null;
        },

        onLoad = function onLoad() {
          cleanup();
          completion(null);
        },

        onError = function onError(event) {
          cleanup();
          unbindNativeStream(videoElement);
          completion('There was an unexpected problem with the Video Stream: ' +
            videoElementErrorCodeToStr(event.target.error.code));
        },

        onStoppedLoading = function onStoppedLoading() {
          // The stream ended before we fully bound it. Maybe the other end called
          // stop on it or something else went wrong.
          cleanup();
          unbindNativeStream(videoElement);
          completion('Stream ended while trying to bind it to a video element.');
        };

    // The official spec way is 'srcObject', we are slowly converging there.
    if (videoElement.srcObject !== void 0) {
      videoElement.srcObject = webRtcStream;
    } else if (videoElement.mozSrcObject !== void 0) {
      videoElement.mozSrcObject = webRtcStream;
    } else {
      videoElement.src = window.URL.createObjectURL(webRtcStream);
    }

    videoElement.addEventListener(loadedEvent, onLoad, false);
    videoElement.addEventListener('error', onError, false);
    webRtcStream.onended = onStoppedLoading;
    videoElement.onended = onStoppedLoading;
  }

  function unbindNativeStream(videoElement) {
    videoElement.onended = null;

    if (videoElement.srcObject !== void 0) {
      videoElement.srcObject = null;
    } else if (videoElement.mozSrcObject !== void 0) {
      videoElement.mozSrcObject = null;
    } else {
      window.URL.revokeObjectURL(videoElement.src);
    }
  }

})(window);

// tb_require('../helpers.js')
// tb_require('./video_element.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

!(function() {
  /*global OT:true */

  var defaultAspectRatio = 4.0 / 3.0,
      miniWidth = 128,
      miniHeight = 128,
      microWidth = 64,
      microHeight = 64;

  /**
   * Sets the video element size so by preserving the intrinsic aspect ratio of the element content
   * but altering the width and height so that the video completely covers the container.
   *
   * @param {Element} element the container of the video element
   * @param {number} containerWidth
   * @param {number} containerHeight
   * @param {number} intrinsicRatio the aspect ratio of the video media
   * @param {boolean} rotated
   */
  function fixFitModeCover(element, containerWidth, containerHeight, intrinsicRatio, rotated) {

    var $video = OT.$('.OT_video-element', element);

    if ($video.length > 0) {

      var cssProps = {left: '', top: ''};

      if (OTPlugin.isInstalled()) {
        cssProps.width = '100%';
        cssProps.height = '100%';
      } else {
        intrinsicRatio = intrinsicRatio || defaultAspectRatio;
        intrinsicRatio = rotated ? 1 / intrinsicRatio : intrinsicRatio;

        var containerRatio = containerWidth / containerHeight;

        var enforcedVideoWidth,
            enforcedVideoHeight;

        if (rotated) {
          // in case of rotation same code works for both kind of ration
          enforcedVideoHeight = containerWidth;
          enforcedVideoWidth = enforcedVideoHeight * intrinsicRatio;

          cssProps.width = enforcedVideoWidth + 'px';
          cssProps.height = enforcedVideoHeight + 'px';
          cssProps.top = (enforcedVideoWidth + containerHeight) / 2 + 'px';
        } else {
          if (intrinsicRatio < containerRatio) {
            // the container is wider than the video -> we will crop the height of the video
            enforcedVideoWidth = containerWidth;
            enforcedVideoHeight = enforcedVideoWidth / intrinsicRatio;

            cssProps.width = enforcedVideoWidth + 'px';
            cssProps.height = enforcedVideoHeight + 'px';
            cssProps.top = (-enforcedVideoHeight + containerHeight) / 2 + 'px';
          } else {
            enforcedVideoHeight = containerHeight;
            enforcedVideoWidth = enforcedVideoHeight * intrinsicRatio;

            cssProps.width = enforcedVideoWidth + 'px';
            cssProps.height = enforcedVideoHeight + 'px';
            cssProps.left = (-enforcedVideoWidth + containerWidth) / 2 + 'px';
          }
        }
      }

      $video.css(cssProps);
    }
  }

  /**
   * Sets the video element size so that the video is entirely visible inside the container.
   *
   * @param {Element} element the container of the video element
   * @param {number} containerWidth
   * @param {number} containerHeight
   * @param {number} intrinsicRatio the aspect ratio of the video media
   * @param {boolean} rotated
   */
  function fixFitModeContain(element, containerWidth, containerHeight, intrinsicRatio, rotated) {

    var $video = OT.$('.OT_video-element', element);

    if ($video.length > 0) {

      var cssProps = {left: '', top: ''};

      if (OTPlugin.isInstalled()) {
        intrinsicRatio = intrinsicRatio || defaultAspectRatio;

        var containerRatio = containerWidth / containerHeight;

        var enforcedVideoWidth,
            enforcedVideoHeight;

        if (intrinsicRatio < containerRatio) {
          enforcedVideoHeight = containerHeight;
          enforcedVideoWidth = containerHeight * intrinsicRatio;

          cssProps.width = enforcedVideoWidth + 'px';
          cssProps.height = enforcedVideoHeight + 'px';
          cssProps.left = (containerWidth - enforcedVideoWidth) / 2 + 'px';
        } else {
          enforcedVideoWidth = containerWidth;
          enforcedVideoHeight = enforcedVideoWidth / intrinsicRatio;

          cssProps.width = enforcedVideoWidth + 'px';
          cssProps.height = enforcedVideoHeight + 'px';
          cssProps.top = (containerHeight - enforcedVideoHeight) / 2 + 'px';
        }
      } else {
        if (rotated) {
          cssProps.width = containerHeight + 'px';
          cssProps.height = containerWidth + 'px';
          cssProps.top = containerHeight + 'px';
        } else {
          cssProps.width = '100%';
          cssProps.height = '100%';
        }
      }

      $video.css(cssProps);
    }
  }

  function fixMini(container, width, height) {
    var w = parseInt(width, 10),
        h = parseInt(height, 10);

    if (w < microWidth || h < microHeight) {
      OT.$.addClass(container, 'OT_micro');
    } else {
      OT.$.removeClass(container, 'OT_micro');
    }
    if (w < miniWidth || h < miniHeight) {
      OT.$.addClass(container, 'OT_mini');
    } else {
      OT.$.removeClass(container, 'OT_mini');
    }
  }

  var getOrCreateContainer = function getOrCreateContainer(elementOrDomId, insertMode) {
    var container,
        domId;

    if (elementOrDomId && elementOrDomId.nodeName) {
      // It looks like we were given a DOM element. Grab the id or generate
      // one if it doesn't have one.
      container = elementOrDomId;
      if (!container.getAttribute('id') || container.getAttribute('id').length === 0) {
        container.setAttribute('id', 'OT_' + OT.$.uuid());
      }

      domId = container.getAttribute('id');
    } else if (elementOrDomId) {
      // We may have got an id, try and get it's DOM element.
      container = OT.$('#' + elementOrDomId).get(0);
      if (container) domId = elementOrDomId;
    }

    if (!domId) {
      domId = 'OT_' + OT.$.uuid().replace(/-/g, '_');
    }

    if (!container) {
      container = OT.$.createElement('div', {id: domId});
      container.style.backgroundColor = '#000000';
      document.body.appendChild(container);
    } else {
      if (!(insertMode == null || insertMode === 'replace')) {
        var placeholder = document.createElement('div');
        placeholder.id = ('OT_' + OT.$.uuid());
        if (insertMode === 'append') {
          container.appendChild(placeholder);
          container = placeholder;
        } else if (insertMode === 'before') {
          container.parentNode.insertBefore(placeholder, container);
          container = placeholder;
        } else if (insertMode === 'after') {
          container.parentNode.insertBefore(placeholder, container.nextSibling);
          container = placeholder;
        }
      } else {
        OT.$.emptyElement(container);
      }
    }

    return container;
  };

  // Creates the standard container that the Subscriber and Publisher use to hold
  // their video element and other chrome.
  OT.WidgetView = function(targetElement, properties) {

    var widgetView = {};

    var container = getOrCreateContainer(targetElement, properties && properties.insertMode),
        widgetContainer = document.createElement('div'),
        oldContainerStyles = {},
        sizeObserver,
        videoElement,
        videoObserver,
        posterContainer,
        loadingContainer,
        width,
        height,
        loading = true,
        audioOnly = false,
        fitMode = 'cover',
        fixFitMode = fixFitModeCover;

    OT.$.eventing(widgetView);

    if (properties) {
      width = properties.width;
      height = properties.height;

      if (width) {
        if (typeof width === 'number') {
          width = width + 'px';
        }
      }

      if (height) {
        if (typeof height === 'number') {
          height = height + 'px';
        }
      }

      container.style.width = width ? width : '264px';
      container.style.height = height ? height : '198px';
      container.style.overflow = 'hidden';
      fixMini(container, width || '264px', height || '198px');

      if (properties.mirror === undefined || properties.mirror) {
        OT.$.addClass(container, 'OT_mirrored');
      }

      if (properties.fitMode === 'contain') {
        fitMode = 'contain';
        fixFitMode = fixFitModeContain;
      } else if (properties.fitMode !== 'cover') {
        OT.warn('Invalid fit value "' + properties.fitMode + '" passed. ' +
        'Only "contain" and "cover" can be used.');
      }
    }

    if (properties.classNames) OT.$.addClass(container, properties.classNames);

    OT.$(container).addClass('OT_loading OT_fit-mode-' + fitMode);

    OT.$.addClass(widgetContainer, 'OT_widget-container');
    widgetContainer.style.width = '100%'; //container.style.width;
    widgetContainer.style.height = '100%'; // container.style.height;
    container.appendChild(widgetContainer);

    loadingContainer = document.createElement('div');
    OT.$.addClass(loadingContainer, 'OT_video-loading');
    widgetContainer.appendChild(loadingContainer);

    posterContainer = document.createElement('div');
    OT.$.addClass(posterContainer, 'OT_video-poster');
    widgetContainer.appendChild(posterContainer);

    oldContainerStyles.width = container.offsetWidth;
    oldContainerStyles.height = container.offsetHeight;

    if (!OTPlugin.isInstalled()) {
      // Observe changes to the width and height and update the aspect ratio
      sizeObserver = OT.$(container).observeSize(
        function(size) {
          var width = size.width,
              height = size.height;

          fixMini(container, width, height);

          if (videoElement) {
            fixFitMode(widgetContainer, width, height, videoElement.aspectRatio(),
              videoElement.isRotated());
          }
        })[0];

      // @todo observe if the video container or the video element get removed
      // if they do we should do some cleanup
      videoObserver = OT.$.observeNodeOrChildNodeRemoval(container, function(removedNodes) {
        if (!videoElement) return;

        // This assumes a video element being removed is the main video element. This may
        // not be the case.
        var videoRemoved = OT.$.some(removedNodes, function(node) {
          return node === widgetContainer || node.nodeName === 'VIDEO';
        });

        if (videoRemoved) {
          videoElement.destroy();
          videoElement = null;
        }

        if (widgetContainer) {
          OT.$.removeElement(widgetContainer);
          widgetContainer = null;
        }

        if (sizeObserver) {
          sizeObserver.disconnect();
          sizeObserver = null;
        }

        if (videoObserver) {
          videoObserver.disconnect();
          videoObserver = null;
        }
      });
    }

    var fixFitModePartial = function() {
      if (!videoElement) return;

      fixFitMode(widgetContainer, container.offsetWidth, container.offsetHeight,
        videoElement.aspectRatio(), videoElement.isRotated());
    };

    widgetView.destroy = function() {
      if (sizeObserver) {
        sizeObserver.disconnect();
        sizeObserver = null;
      }

      if (videoObserver) {
        videoObserver.disconnect();
        videoObserver = null;
      }

      if (videoElement) {
        videoElement.destroy();
        videoElement = null;
      }

      if (container) {
        OT.$.removeElement(container);
        container = null;
      }
    };

    widgetView.setBackgroundImageURI = function(bgImgURI) {
      if (bgImgURI.substr(0, 5) !== 'http:' && bgImgURI.substr(0, 6) !== 'https:') {
        if (bgImgURI.substr(0, 22) !== 'data:image/png;base64,') {
          bgImgURI = 'data:image/png;base64,' + bgImgURI;
        }
      }
      OT.$.css(posterContainer, 'backgroundImage', 'url(' + bgImgURI + ')');
      OT.$.css(posterContainer, 'backgroundSize', 'contain');
      OT.$.css(posterContainer, 'opacity', '1.0');
    };

    if (properties && properties.style && properties.style.backgroundImageURI) {
      widgetView.setBackgroundImageURI(properties.style.backgroundImageURI);
    }

    widgetView.bindVideo = function(webRTCStream, options, completion) {
      // remove the old video element if it exists
      // @todo this might not be safe, publishers/subscribers use this as well...

      if (typeof options.audioVolume !== 'undefined') {
        options.audioVolume = parseFloat(options.audioVolume);
      }

      if (videoElement) {
        videoElement.destroy();
        videoElement = null;
      }

      var onError = options && options.error ? options.error : void 0;
      delete options.error;

      var video = new OT.VideoElement({attributes: options}, onError);

      // Initialize the audio volume
      if (typeof options.audioVolume !== 'undefined') {
        video.setAudioVolume(options.audioVolume);
      }

      // makes the incoming audio streams take priority (will impact only FF OS for now)
      video.audioChannelType('telephony');

      video.appendTo(widgetContainer).bindToStream(webRTCStream, function(err) {
        if (err) {
          video.destroy();
          completion(err);
          return;
        }

        videoElement = video;

        // clear inline height value we used to init plugin rendering
        if (video.domElement()) {
          OT.$.css(video.domElement(), 'height', '');
        }

        video.on({
          orientationChanged: fixFitModePartial,
          videoDimensionsChanged: function(oldValue, newValue) {
            fixFitModePartial();
            widgetView.trigger('videoDimensionsChanged', oldValue, newValue);
          },
          mediaStopped: function() {
            widgetView.trigger('mediaStopped');
          }
        });

        video.onRatioAvailable(fixFitModePartial);

        completion(null, video);
      });

      OT.$.addClass(video.domElement(), 'OT_video-element');

      // plugin needs a minimum of height to be rendered and kicked off
      // we will reset that once the video is bound to the stream
      OT.$.css(video.domElement(), 'height', '1px');

      return video;
    };

    OT.$.defineProperties(widgetView, {

      video: {
        get: function() {
          return videoElement;
        }
      },

      showPoster: {
        get: function() {
          return !OT.$.isDisplayNone(posterContainer);
        },
        set: function(newValue) {
          if (newValue) {
            OT.$.show(posterContainer);
          } else {
            OT.$.hide(posterContainer);
          }
        }
      },

      poster: {
        get: function() {
          return OT.$.css(posterContainer, 'backgroundImage');
        },
        set: function(src) {
          OT.$.css(posterContainer, 'backgroundImage', 'url(' + src + ')');
        }
      },

      loading: {
        get: function() { return loading; },
        set: function(l) {
          loading = l;

          if (loading) {
            OT.$.addClass(container, 'OT_loading');
          } else {
            OT.$.removeClass(container, 'OT_loading');
          }
        }
      },

      audioOnly: {
        get: function() { return audioOnly; },
        set: function(a) {
          audioOnly = a;

          if (audioOnly) {
            OT.$.addClass(container, 'OT_audio-only');
          } else {
            OT.$.removeClass(container, 'OT_audio-only');
          }

          if (OTPlugin.isInstalled()) {
            // to keep OTPlugin happy
            setTimeout(fixFitModePartial, 0);
          }
        }
      },

      domId: {
        get: function() { return container.getAttribute('id'); }
      }

    });

    widgetView.domElement = container;

    widgetView.addError = function(errorMsg, helpMsg, classNames) {
      container.innerHTML = '<p>' + errorMsg +
        (helpMsg ? ' <span class="ot-help-message">' + helpMsg + '</span>' : '') +
        '</p>';
      OT.$.addClass(container, classNames || 'OT_subscriber_error');
      if (container.querySelector('p').offsetHeight > container.offsetHeight) {
        container.querySelector('span').style.display = 'none';
      }
    };

    return widgetView;
  };

})(window);

// tb_require('../helpers.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

if (!OT.properties) {
  throw new Error('OT.properties does not exist, please ensure that you include a valid ' +
    'properties file.');
}

// Consumes and overwrites OT.properties. Makes it better and stronger!
OT.properties = (function(properties) {
  var props = OT.$.clone(properties);

  props.debug = properties.debug === 'true' || properties.debug === true;
  props.supportSSL = properties.supportSSL === 'true' || properties.supportSSL === true;

  if (window.OTProperties) {
    // Allow window.OTProperties to override cdnURL, configURL, assetURL and cssURL
    if (window.OTProperties.cdnURL) props.cdnURL = window.OTProperties.cdnURL;
    if (window.OTProperties.cdnURLSSL) props.cdnURLSSL = window.OTProperties.cdnURLSSL;
    if (window.OTProperties.configURL) props.configURL = window.OTProperties.configURL;
    if (window.OTProperties.assetURL) props.assetURL = window.OTProperties.assetURL;
    if (window.OTProperties.cssURL) props.cssURL = window.OTProperties.cssURL;
  }

  if (!props.assetURL) {
    if (OT.properties.supportSSL) {
      props.assetURL = props.cdnURLSSL + '/webrtc/' + props.version;
    } else {
      props.assetURL = props.cdnURL + '/webrtc/' + props.version;
    }
  }

  var isIE89 = OT.$.env.name === 'IE' && OT.$.env.version <= 9;
  if (!(isIE89 && window.location.protocol.indexOf('https') < 0)) {
    props.apiURL = props.apiURLSSL;
    props.loggingURL = props.loggingURLSSL;
  }

  if (!props.configURL) props.configURL = props.assetURL + '/js/dynamic_config.min.js';
  if (!props.cssURL) props.cssURL = props.assetURL + '/css/TB.min.css';

  return props;
})(OT.properties);

// tb_require('../helpers.js')

!(function() {
  /* jshint globalstrict: true, strict: false, undef: true, unused: true,
            trailing: true, browser: true, smarttabs:true */
  /* global OT */

  var currentGuidStorage,
      currentGuid;

  var isInvalidStorage = function isInvalidStorage(storageInterface) {
    return !(OT.$.isFunction(storageInterface.get) && OT.$.isFunction(storageInterface.set));
  };

  var getClientGuid = function getClientGuid(completion) {
    if (currentGuid) {
      completion(null, currentGuid);
      return;
    }

    // It's the first time that getClientGuid has been called
    // in this page lifetime. Attempt to load any existing Guid
    // from the storage
    currentGuidStorage.get(completion);
  };

  /*
  * Sets the methods for storing and retrieving client GUIDs persistently
  * across sessions. By default, OpenTok.js attempts to use browser cookies to
  * store GUIDs.
  * <p>
  * Pass in an object that has a <code>get()</code> method and
  * a <code>set()</code> method.
  * <p>
  * The <code>get()</code> method must take one parameter: the callback
  * method to invoke. The callback method is passed two parameters &mdash;
  * the first parameter is an error object or null if the call is successful;
  * and the second parameter is the GUID (a string) if successful.
  * <p>
  * The <code>set()</code> method must include two parameters: the GUID to set
  * (a string) and the callback method to invoke. The callback method is
  * passed an error object on error, or it is passed no parameter if the call is
  * successful.
  * <p>
  * Here is an example:
  * <p>
  * <pre>
  * var ComplexStorage = function() {
  *   this.set = function(guid, completion) {
  *     AwesomeBackendService.set(guid, function(response) {
  *       completion(response.error || null);
  *     });
  *   };
  *   this.get = function(completion) {
  *     AwesomeBackendService.get(function(response, guid) {
  *       completion(response.error || null, guid);
  *     });
  *   };
  * };
  *
  * OT.overrideGuidStorage(new ComplexStorage());
  * </pre>
  */
  OT.overrideGuidStorage = function(storageInterface) {
    if (isInvalidStorage(storageInterface)) {
      throw new Error('The storageInterface argument does not seem to be valid, ' +
                                        'it must implement get and set methods');
    }

    if (currentGuidStorage === storageInterface) {
      return;
    }

    currentGuidStorage = storageInterface;

    // If a client Guid has already been assigned to this client then
    // let the new storage know about it so that it's in sync.
    if (currentGuid) {
      currentGuidStorage.set(currentGuid, function(error) {
        if (error) {
          OT.error('Failed to send initial Guid value (' + currentGuid +
                                ') to the newly assigned Guid Storage. The error was: ' + error);
          // @todo error
        }
      });
    }
  };

  if (!OT._) OT._ = {};
  OT._.getClientGuid = function(completion) {
    getClientGuid(function(error, guid) {
      if (error) {
        completion(error);
        return;
      }

      if (!guid) {
        // Nothing came back, this client is entirely new.
        // generate a new Guid and persist it
        guid = OT.$.uuid();
        currentGuidStorage.set(guid, function(error) {
          if (error) {
            completion(error);
            return;
          }

          currentGuid = guid;
        });
      } else if (!currentGuid) {
        currentGuid = guid;
      }

      completion(null, currentGuid);
    });
  };

  // Implement our default storage mechanism, which sets/gets a cookie
  // called 'opentok_client_id'
  OT.overrideGuidStorage({
    get: function(completion) {
      completion(null, OT.$.getCookie('opentok_client_id'));
    },

    set: function(guid, completion) {
      OT.$.setCookie('opentok_client_id', guid);
      completion(null);
    }
  });

})(window);

// tb_require('../helpers.js')
// tb_require('./web_rtc.js')

// Web OT Helpers
!(function() {
  /* jshint globalstrict: true, strict: false, undef: true, unused: true,
            trailing: true, browser: true, smarttabs:true */
  /* global OT */

  var nativeGetUserMedia,
      vendorToW3CErrors,
      gumNamesToMessages,
      mapVendorErrorName,
      parseErrorEvent,
      areInvalidConstraints;

  // Handy cross-browser getUserMedia shim. Inspired by some code from Adam Barth
  nativeGetUserMedia = (function() {
    if (navigator.getUserMedia) {
      return OT.$.bind(navigator.getUserMedia, navigator);
    } else if (navigator.mozGetUserMedia) {
      return OT.$.bind(navigator.mozGetUserMedia, navigator);
    } else if (navigator.webkitGetUserMedia) {
      return OT.$.bind(navigator.webkitGetUserMedia, navigator);
    } else if (OTPlugin.isInstalled()) {
      return OT.$.bind(OTPlugin.getUserMedia, OTPlugin);
    }
  })();

  // Mozilla error strings and the equivalent W3C names. NOT_SUPPORTED_ERROR does not
  // exist in the spec right now, so we'll include Mozilla's error description.
  // Chrome TrackStartError is triggered when the camera is already used by another app (Windows)
  vendorToW3CErrors = {
    PERMISSION_DENIED: 'PermissionDeniedError',
    NOT_SUPPORTED_ERROR: 'NotSupportedError',
    MANDATORY_UNSATISFIED_ERROR: ' ConstraintNotSatisfiedError',
    NO_DEVICES_FOUND: 'NoDevicesFoundError',
    HARDWARE_UNAVAILABLE: 'HardwareUnavailableError',
    TrackStartError: 'HardwareUnavailableError'
  };

  gumNamesToMessages = {
    PermissionDeniedError: 'End-user denied permission to hardware devices',
    PermissionDismissedError: 'End-user dismissed permission to hardware devices',
    NotSupportedError: 'A constraint specified is not supported by the browser.',
    ConstraintNotSatisfiedError: 'It\'s not possible to satisfy one or more constraints ' +
      'passed into the getUserMedia function',
    OverconstrainedError: 'Due to changes in the environment, one or more mandatory ' +
      'constraints can no longer be satisfied.',
    NoDevicesFoundError: 'No voice or video input devices are available on this machine.',
    HardwareUnavailableError: 'The selected voice or video devices are unavailable. Verify ' +
      'that the chosen devices are not in use by another application.'
  };

  // Map vendor error strings to names and messages if possible
  mapVendorErrorName = function mapVendorErrorName(vendorErrorName, vendorErrors) {
    var errorName, errorMessage;

    if (vendorErrors.hasOwnProperty(vendorErrorName)) {
      errorName = vendorErrors[vendorErrorName];
    } else {
      // This doesn't map to a known error from the Media Capture spec, it's
      // probably a custom vendor error message.
      errorName = vendorErrorName;
    }

    if (gumNamesToMessages.hasOwnProperty(errorName)) {
      errorMessage = gumNamesToMessages[errorName];
    } else {
      errorMessage = 'Unknown Error while getting user media';
    }

    return {
      name: errorName,
      message: errorMessage
    };
  };

  // Parse and normalise a getUserMedia error event from Chrome or Mozilla
  // @ref http://dev.w3.org/2011/webrtc/editor/getusermedia.html#idl-def-NavigatorUserMediaError
  parseErrorEvent = function parseErrorObject(event) {
    var error;

    if (OT.$.isObject(event) && event.name) {
      error = mapVendorErrorName(event.name, vendorToW3CErrors);
      error.constraintName = event.constraintName;
    } else if (typeof event === 'string') {
      error = mapVendorErrorName(event, vendorToW3CErrors);
    } else {
      error = {
        message: 'Unknown Error type while getting media'
      };
    }

    return error;
  };

  // Validates a Hash of getUserMedia constraints. Currently we only
  // check to see if there is at least one non-false constraint.
  areInvalidConstraints = function(constraints) {
    if (!constraints || !OT.$.isObject(constraints)) return true;

    for (var key in constraints) {
      if (!constraints.hasOwnProperty(key)) {
        continue;
      }
      if (constraints[key]) return false;
    }

    return true;
  };

  // A wrapper for the builtin navigator.getUserMedia. In addition to the usual
  // getUserMedia behaviour, this helper method also accepts a accessDialogOpened
  // and accessDialogClosed callback.
  //
  // @memberof OT.$
  // @private
  //
  // @param {Object} constraints
  //      A dictionary of constraints to pass to getUserMedia. See
  //      http://dev.w3.org/2011/webrtc/editor/getusermedia.html#idl-def-MediaStreamConstraints
  //      in the Media Capture and Streams spec for more info.
  //
  // @param {function} success
  //      Called when getUserMedia completes successfully. The callback will be passed a WebRTC
  //      Stream object.
  //
  // @param {function} failure
  //      Called when getUserMedia fails to access a user stream. It will be passed an object
  //      with a code property representing the error that occurred.
  //
  // @param {function} accessDialogOpened
  //      Called when the access allow/deny dialog is opened.
  //
  // @param {function} accessDialogClosed
  //      Called when the access allow/deny dialog is closed.
  //
  // @param {function} accessDenied
  //      Called when access is denied to the camera/mic. This will be either because
  //      the user has clicked deny or because a particular origin is permanently denied.
  //
  OT.$.getUserMedia = function(constraints, success, failure, accessDialogOpened,
    accessDialogClosed, accessDenied, customGetUserMedia) {

    var getUserMedia = nativeGetUserMedia;

    if (OT.$.isFunction(customGetUserMedia)) {
      getUserMedia = customGetUserMedia;
    }

    // All constraints are false, we don't allow this. This may be valid later
    // depending on how/if we integrate data channels.
    if (areInvalidConstraints(constraints)) {
      OT.error('Couldn\'t get UserMedia: All constraints were false');
      // Using a ugly dummy-code for now.
      failure.call(null, {
        name: 'NO_VALID_CONSTRAINTS',
        message: 'Video and Audio was disabled, you need to enabled at least one'
      });

      return;
    }

    var triggerOpenedTimer = null,
        displayedPermissionDialog = false,

        finaliseAccessDialog = function() {
          if (triggerOpenedTimer) {
            clearTimeout(triggerOpenedTimer);
          }

          if (displayedPermissionDialog && accessDialogClosed) accessDialogClosed();
        },

        triggerOpened = function() {
          triggerOpenedTimer = null;
          displayedPermissionDialog = true;

          if (accessDialogOpened) accessDialogOpened();
        },

        onStream = function(stream) {
          finaliseAccessDialog();
          success.call(null, stream);
        },

        onError = function(event) {
          finaliseAccessDialog();
          var error = parseErrorEvent(event);

          // The error name 'PERMISSION_DENIED' is from an earlier version of the spec
          if (error.name === 'PermissionDeniedError' || error.name === 'PermissionDismissedError') {
            accessDenied.call(null, error);
          } else {
            failure.call(null, error);
          }
        };

    try {
      getUserMedia(constraints, onStream, onError);
    } catch (e) {
      OT.error('Couldn\'t get UserMedia: ' + e.toString());
      onError();
      return;
    }

    // The 'remember me' functionality of WebRTC only functions over HTTPS, if
    // we aren't on HTTPS then we should definitely be displaying the access
    // dialog.
    //
    // If we are on HTTPS, we'll wait 500ms to see if we get a stream
    // immediately. If we do then the user had clicked 'remember me'. Otherwise
    // we assume that the accessAllowed dialog is visible.
    //
    // @todo benchmark and see if 500ms is a reasonable number. It seems like
    // we should know a lot quicker.
    //
    if (location.protocol.indexOf('https') === -1) {
      // Execute after, this gives the client a chance to bind to the
      // accessDialogOpened event.
      triggerOpenedTimer = setTimeout(triggerOpened, 100);

    } else {
      // wait a second and then trigger accessDialogOpened
      triggerOpenedTimer = setTimeout(triggerOpened, 500);
    }
  };

})();

// tb_require('../helpers.js')
// tb_require('./web_rtc.js')

// Web OT Helpers
!(function(window) {

  /* jshint globalstrict: true, strict: false, undef: true, unused: true,
            trailing: true, browser: true, smarttabs:true */
  /* global OT */

  ///
  // Device Helpers
  //
  // Support functions to enumerating and guerying device info
  //

  // Map all the various formats for device kinds
  // to the ones that our API exposes. Also includes
  // an identity map.
  var deviceKindsMap = {
    audio: 'audioInput',
    video: 'videoInput',
    audioinput: 'audioInput',
    videoinput: 'videoInput',
    audioInput: 'audioInput',
    videoInput: 'videoInput'
  };

  var getNativeEnumerateDevices = function() {
    if (navigator.mediaDevices) {
      return navigator.mediaDevices.enumerateDevices;
    } else if (OT.$.hasCapabilities('otplugin')) {
      return OTPlugin.mediaDevices.enumerateDevices;
    } else if (window.MediaStreamTrack && window.MediaStreamTrack.getSources) {
      return window.MediaStreamTrack.getSources;
    }
  };

  var enumerateDevices = function(completion) {
    var fn = getNativeEnumerateDevices();

    if (navigator.mediaDevices && OT.$.isFunction(fn)) {
      // De-promisify the newer style APIs. We aren't ready for Promises yet...
      fn = function(completion) {
        navigator.mediaDevices.enumerateDevices().then(function(devices) {
          completion(void 0, devices);
        })['catch'](function(err) {
          completion(err);
        });
      };
    } else if (window.MediaStreamTrack && window.MediaStreamTrack.getSources) {
      fn = function(completion) {
        window.MediaStreamTrack.getSources(function(devices) {
          completion(void 0, devices.map(function(device) {
            return OT.$.clone(device);
          }));
        });
      };
    }

    return fn(completion);
  };

  var fakeShouldAskForDevices = function fakeShouldAskForDevices(callback) {
    setTimeout(OT.$.bind(callback, null, { video: true, audio: true }));
  };

  // Indicates whether this browser supports the enumerateDevices (getSources) API.
  //
  OT.$.registerCapability('enumerateDevices', function() {
    return OT.$.isFunction(getNativeEnumerateDevices());
  });

  OT.$.getMediaDevices = function(completion, customGetDevices) {
    if (!OT.$.hasCapabilities('enumerateDevices')) {
      completion(new Error('This browser does not support enumerateDevices APIs'));
      return;
    }

    var getDevices = OT.$.isFunction(customGetDevices) ? customGetDevices
                                                       : enumerateDevices;

    getDevices(function(err, devices) {
      if (err) {
        completion(err);
        return;
      }

      // Normalise the device kinds
      var filteredDevices = OT.$(devices).map(function(device) {
        return {
          deviceId: device.deviceId || device.id,
          label: device.label,
          kind: deviceKindsMap[device.kind]
        };
      }).filter(function(device) {
        return device.kind === 'audioInput' || device.kind === 'videoInput';
      });

      completion(void 0, filteredDevices.get());
    });
  };

  OT.$.shouldAskForDevices = function(callback) {
    if (OT.$.hasCapabilities('enumerateDevices')) {
      OT.$.getMediaDevices(function(err, devices) {
        if (err) {
          // There was an error. It may be temporally. Just assume
          // all devices exist for now.
          fakeShouldAskForDevices(callback);
          return;
        }

        var hasAudio = OT.$.some(devices, function(device) {
          return device.kind === 'audioInput';
        });

        var hasVideo = OT.$.some(devices, function(device) {
          return device.kind === 'videoInput';
        });

        callback.call(null, { video: hasVideo, audio: hasAudio });
      });

    } else {
      // This environment can't enumerate devices anyway, so we'll memorise this result.
      OT.$.shouldAskForDevices = fakeShouldAskForDevices;
      OT.$.shouldAskForDevices(callback);
    }
  };
})(window);

// tb_require('../helpers.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

!(function() {

  var adjustModal = function(callback) {
    return function setFullHeightDocument(window, document) {
      // required in IE8
      document.querySelector('html').style.height = document.body.style.height = '100%';
      callback(window, document);
    };
  };

  var addCss = function(document, url, callback) {
    var head = document.head || document.getElementsByTagName('head')[0];
    var cssTag = OT.$.createElement('link', {
      type: 'text/css',
      media: 'screen',
      rel: 'stylesheet',
      href: url
    });
    head.appendChild(cssTag);
    OT.$.on(cssTag, 'error', function(error) {
      OT.error('Could not load CSS for dialog', url, error && error.message || error);
    });
    OT.$.on(cssTag, 'load', callback);
  };

  var addDialogCSS = function(document, urls, callback) {
    var allURLs = [
      '//fonts.googleapis.com/css?family=Didact+Gothic',
      OT.properties.cssURL
    ].concat(urls);
    var remainingStylesheets = allURLs.length;
    OT.$.forEach(allURLs, function(stylesheetUrl) {
      addCss(document, stylesheetUrl, function() {
        if (--remainingStylesheets <= 0) {
          callback();
        }
      });
    });

  };

  var templateElement = function(classes, children, tagName) {
    var el = OT.$.createElement(tagName || 'div', { 'class': classes }, children, this);
    el.on = OT.$.bind(OT.$.on, OT.$, el);
    el.off = OT.$.bind(OT.$.off, OT.$, el);
    return el;
  };

  var checkBoxElement = function(classes, nameAndId, onChange) {
    var checkbox = templateElement.call(this, '', null, 'input');
    checkbox = OT.$(checkbox).on('change', onChange);

    if (OT.$.env.name === 'IE' && OT.$.env.version <= 8) {
      // Fix for IE8 not triggering the change event
      checkbox.on('click', function() {
        checkbox.first.blur();
        checkbox.first.focus();
      });
    }

    checkbox.attr({
      name: nameAndId,
      id: nameAndId,
      type: 'checkbox'
    });

    return checkbox.first;
  };

  var linkElement = function(children, href, classes) {
    var link = templateElement.call(this, classes || '', children, 'a');
    link.setAttribute('href', href);
    return link;
  };

  OT.Dialogs = {};

  OT.Dialogs.Plugin = {};

  OT.Dialogs.Plugin.promptToInstall = function() {
    var modal = new OT.$.Modal(adjustModal(function(window, document) {

      var el = OT.$.bind(templateElement, document),
          btn = function(children, size) {
            var classes = 'OT_dialog-button ' +
                          (size ? 'OT_dialog-button-' + size : 'OT_dialog-button-large'),
                b = el(classes, children);

            b.enable = function() {
              OT.$.removeClass(this, 'OT_dialog-button-disabled');
              return this;
            };

            b.disable = function() {
              OT.$.addClass(this, 'OT_dialog-button-disabled');
              return this;
            };

            return b;
          },
          downloadButton = btn('Download plugin'),
          cancelButton = btn('cancel', 'small'),
          refreshButton = btn('Refresh browser'),
          acceptEULA,
          checkbox,
          close,
          root;

      OT.$.addClass(cancelButton, 'OT_dialog-no-natural-margin OT_dialog-button-block');
      OT.$.addClass(refreshButton, 'OT_dialog-no-natural-margin');

      function onDownload() {
        modal.trigger('download');
        setTimeout(function() {
          root.querySelector('.OT_dialog-messages-main').innerHTML =
                                              'Plugin installation successful';
          var sections = root.querySelectorAll('.OT_dialog-section');
          OT.$.addClass(sections[0], 'OT_dialog-hidden');
          OT.$.removeClass(sections[1], 'OT_dialog-hidden');
        }, 3000);
      }

      function onRefresh() {
        modal.trigger('refresh');
      }

      function onToggleEULA() {
        if (checkbox.checked) {
          enableButtons();
        } else {
          disableButtons();
        }
      }

      function enableButtons() {
        downloadButton.enable();
        downloadButton.on('click', onDownload);

        refreshButton.enable();
        refreshButton.on('click', onRefresh);
      }

      function disableButtons() {
        downloadButton.disable();
        downloadButton.off('click', onDownload);

        refreshButton.disable();
        refreshButton.off('click', onRefresh);
      }

      downloadButton.disable();
      refreshButton.disable();

      cancelButton.on('click', function() {
        modal.trigger('cancelButtonClicked');
        modal.close();
      });

      close = el('OT_closeButton', '&times;')
        .on('click', function() {
          modal.trigger('closeButtonClicked');
          modal.close();
        }).first;

      var protocol = (window.location.protocol.indexOf('https') >= 0 ? 'https' : 'http');
      acceptEULA = linkElement.call(document,
                                    'end-user license agreement',
                                    protocol + '://tokbox.com/support/ie-eula');

      checkbox = checkBoxElement.call(document, null, 'acceptEULA', onToggleEULA);

      root = el('OT_dialog-centering', [
        el('OT_dialog-centering-child', [
          el('OT_root OT_dialog OT_dialog-plugin-prompt', [
            close,
            el('OT_dialog-messages', [
              el('OT_dialog-messages-main', 'This app requires real-time communication')
            ]),
            el('OT_dialog-section', [
              el('OT_dialog-single-button-with-title', [
                el('OT_dialog-button-title', [
                  checkbox,
                  (function() {
                    var x = el('', 'accept', 'label');
                    x.setAttribute('for', checkbox.id);
                    x.style.margin = '0 5px';
                    return x;
                  })(),
                  acceptEULA
                ]),
                el('OT_dialog-actions-card', [
                  downloadButton,
                  cancelButton
                ])
              ])
            ]),
            el('OT_dialog-section OT_dialog-hidden', [
              el('OT_dialog-button-title', [
                'You can now enjoy webRTC enabled video via Internet Explorer.'
              ]),
              refreshButton
            ])
          ])
        ])
      ]);

      addDialogCSS(document, [], function() {
        document.body.appendChild(root);
      });

    }));
    return modal;
  };

  OT.Dialogs.Plugin.promptToReinstall = function() {
    var modal = new OT.$.Modal(adjustModal(function(window, document) {

      var el = OT.$.bind(templateElement, document),
          close,
          okayButton,
          root;

      close = el('OT_closeButton', '&times;')
                .on('click', function() {
                  modal.trigger('closeButtonClicked');
                  modal.close();
                }).first;

      okayButton =
        el('OT_dialog-button OT_dialog-button-large OT_dialog-no-natural-margin', 'Okay')
          .on('click', function() {
            modal.trigger('okay');
          }).first;

      root = el('OT_dialog-centering', [
        el('OT_dialog-centering-child', [
          el('OT_ROOT OT_dialog OT_dialog-plugin-reinstall', [
            close,
            el('OT_dialog-messages', [
              el('OT_dialog-messages-main', 'Reinstall Opentok Plugin'),
              el('OT_dialog-messages-minor', 'Uh oh! Try reinstalling the OpenTok plugin again ' +
                'to enable real-time video communication for Internet Explorer.')
            ]),
            el('OT_dialog-section', [
              el('OT_dialog-single-button', okayButton)
            ])
          ])
        ])
      ]);

      addDialogCSS(document, [], function() {
        document.body.appendChild(root);
      });

    }));

    return modal;
  };

  OT.Dialogs.Plugin.updateInProgress = function() {

    var progressBar,
        progressText,
        progressValue = 0;

    var modal = new OT.$.Modal(adjustModal(function(window, document) {

      var el = OT.$.bind(templateElement, document),
          root;

      progressText = el('OT_dialog-plugin-upgrade-percentage', '0%', 'strong');

      progressBar = el('OT_dialog-progress-bar-fill');

      root = el('OT_dialog-centering', [
        el('OT_dialog-centering-child', [
          el('OT_ROOT OT_dialog OT_dialog-plugin-upgrading', [
            el('OT_dialog-messages', [
              el('OT_dialog-messages-main', [
                'One moment please... ',
                progressText
              ]),
              el('OT_dialog-progress-bar', progressBar),
              el('OT_dialog-messages-minor OT_dialog-no-natural-margin',
                'Please wait while the OpenTok plugin is updated')
            ])
          ])
        ])
      ]);

      addDialogCSS(document, [], function() {
        document.body.appendChild(root);
        if (progressValue != null) {
          modal.setUpdateProgress(progressValue);
        }
      });
    }));

    modal.setUpdateProgress = function(newProgress) {
      if (progressBar && progressText) {
        if (newProgress > 99) {
          OT.$.css(progressBar, 'width', '');
          progressText.innerHTML = '100%';
        } else if (newProgress < 1) {
          OT.$.css(progressBar, 'width', '0%');
          progressText.innerHTML = '0%';
        } else {
          OT.$.css(progressBar, 'width', newProgress + '%');
          progressText.innerHTML = newProgress + '%';
        }
      } else {
        progressValue = newProgress;
      }
    };

    return modal;
  };

  OT.Dialogs.Plugin.updateComplete = function(error) {
    var modal = new OT.$.Modal(adjustModal(function(window, document) {
      var el = OT.$.bind(templateElement, document),
          reloadButton,
          root;

      reloadButton =
        el('OT_dialog-button OT_dialog-button-large OT_dialog-no-natural-margin', 'Reload')
          .on('click', function() {
            modal.trigger('reload');
          }).first;

      var msgs;

      if (error) {
        msgs = ['Update Failed.', error + '' || 'NO ERROR'];
      } else {
        msgs = ['Update Complete.',
          'The OpenTok plugin has been succesfully updated. ' +
          'Please reload your browser.'];
      }

      root = el('OT_dialog-centering', [
        el('OT_dialog-centering-child', [
          el('OT_root OT_dialog OT_dialog-plugin-upgraded', [
            el('OT_dialog-messages', [
              el('OT_dialog-messages-main', msgs[0]),
              el('OT_dialog-messages-minor', msgs[1])
            ]),
            el('OT_dialog-single-button', reloadButton)
          ])
        ])
      ]);

      addDialogCSS(document, [], function() {
        document.body.appendChild(root);
      });

    }));

    return modal;

  };

})();

// tb_require('../helpers.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* exported loadCSS */

var loadCSS = function loadCSS(cssURL) {
  var style = document.createElement('link');
  style.type = 'text/css';
  style.media = 'screen';
  style.rel = 'stylesheet';
  style.href = cssURL;
  var head = document.head || document.getElementsByTagName('head')[0];
  head.appendChild(style);
};

// tb_require('../helpers.js')
// tb_require('./properties.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

//--------------------------------------
// JS Dynamic Config
//--------------------------------------

OT.Config = (function() {
  var _loaded = false,
      _global = {},
      _partners = {},
      _script,
      _head = document.head || document.getElementsByTagName('head')[0],
      _loadTimer,

      _clearTimeout = function() {
        if (_loadTimer) {
          clearTimeout(_loadTimer);
          _loadTimer = null;
        }
      },

      _cleanup = function() {
        _clearTimeout();

        if (_script) {
          _script.onload = _script.onreadystatechange = null;

          if (_head && _script.parentNode) {
            _head.removeChild(_script);
          }

          _script = undefined;
        }
      },

      _onLoad = function() {
        // Only IE and Opera actually support readyState on Script elements.
        if (_script.readyState && !/loaded|complete/.test(_script.readyState)) {
          // Yeah, we're not ready yet...
          return;
        }

        _clearTimeout();

        if (!_loaded) {
          // Our config script is loaded but there is not config (as
          // replaceWith wasn't called). Something went wrong. Possibly
          // the file we loaded wasn't actually a valid config file.
          _this._onLoadTimeout();
        }
      },

      _onLoadError = function(/* event */) {
        _cleanup();

        OT.warn('TB DynamicConfig failed to load due to an error');
        this.trigger('dynamicConfigLoadFailed');
      },

      _getModule = function(moduleName, apiKey) {
        if (apiKey && _partners[apiKey] && _partners[apiKey][moduleName]) {
          return _partners[apiKey][moduleName];
        }

        return _global[moduleName];
      },

      _this;

  _this = {
    // In ms
    loadTimeout: 4000,

    _onLoadTimeout: function() {
      _cleanup();

      OT.warn('TB DynamicConfig failed to load in ' + _this.loadTimeout + ' ms');
      this.trigger('dynamicConfigLoadFailed');
    },

    load: function(configUrl) {
      if (!configUrl) throw new Error('You must pass a valid configUrl to Config.load');

      _loaded = false;

      setTimeout(function() {
        _script = document.createElement('script');
        _script.async = 'async';
        _script.src = configUrl;
        _script.onload = _script.onreadystatechange = OT.$.bind(_onLoad, _this);
        _script.onerror = OT.$.bind(_onLoadError, _this);
        _head.appendChild(_script);
      }, 1);

      _loadTimer = setTimeout(function() {
        _this._onLoadTimeout();
      }, this.loadTimeout);
    },

    isLoaded: function() {
      return _loaded;
    },

    reset: function() {
      this.off();
      _cleanup();
      _loaded = false;
      _global = {};
      _partners = {};
    },

    // This is public so that the dynamic config file can load itself.
    // Using it for other purposes is discouraged, but not forbidden.
    replaceWith: function(config) {
      _cleanup();

      if (!config) config = {};

      _global = config.global || {};
      _partners = config.partners || {};

      if (!_loaded) _loaded = true;
      this.trigger('dynamicConfigChanged');
    },

    // @example Get the value that indicates whether exceptionLogging is enabled
    //  OT.Config.get('exceptionLogging', 'enabled');
    //
    // @example Get a key for a specific partner, fallback to the default if there is
    // no key for that partner
    //  OT.Config.get('exceptionLogging', 'enabled', 'apiKey');
    //
    get: function(moduleName, key, apiKey) {
      var module = _getModule(moduleName, apiKey);
      return module ? module[key] : null;
    }
  };

  OT.$.eventing(_this);

  return _this;
})();

// tb_require('../helpers.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OTPlugin, OT */

///
// Capabilities
//
// Support functions to query browser/client Media capabilities.
//

// Indicates whether this client supports the getUserMedia
// API.
//
OT.$.registerCapability('getUserMedia', function() {
  if (OT.$.env === 'Node') return false;
  return !!(navigator.webkitGetUserMedia ||
            navigator.mozGetUserMedia ||
            OTPlugin.isInstalled());
});

// TODO Remove all PeerConnection stuff, that belongs to the messaging layer not the Media layer.
// Indicates whether this client supports the PeerConnection
// API.
//
// Chrome Issues:
// * The explicit prototype.addStream check is because webkitRTCPeerConnection was
// partially implemented, but not functional, in Chrome 22.
//
// Firefox Issues:
// * No real support before Firefox 19
// * Firefox 19 has issues with generating Offers.
// * Firefox 20 doesn't interoperate with Chrome.
//
OT.$.registerCapability('PeerConnection', function() {
  if (OT.$.env === 'Node') {
    return false;
  } else if (typeof window.webkitRTCPeerConnection === 'function' &&
                    !!window.webkitRTCPeerConnection.prototype.addStream) {
    return true;
  } else if (typeof window.mozRTCPeerConnection === 'function' && OT.$.env.version > 20.0) {
    return true;
  } else {
    return OTPlugin.isInstalled();
  }
});

// Indicates whether this client supports WebRTC
//
// This is defined as: getUserMedia + PeerConnection + exceeds min browser version
//
OT.$.registerCapability('webrtc', function() {
  if (OT.properties) {
    var minimumVersions = OT.properties.minimumVersion || {},
        minimumVersion = minimumVersions[OT.$.env.name.toLowerCase()];

    if (minimumVersion && OT.$.env.versionGreaterThan(minimumVersion)) {
      OT.debug('Support for', OT.$.env.name, 'is disabled because we require',
        minimumVersion, 'but this is', OT.$.env.version);
      return false;
    }
  }

  if (OT.$.env === 'Node') {
    // Node works, even though it doesn't have getUserMedia
    return true;
  }

  return OT.$.hasCapabilities('getUserMedia', 'PeerConnection');
});

// TODO Remove all transport stuff, that belongs to the messaging layer not the Media layer.
// Indicates if the browser supports bundle
//
// Broadly:
// * Firefox doesn't support bundle
// * Chrome support bundle
// * OT Plugin supports bundle
// * We assume NodeJs supports bundle (e.g. 'you're on your own' mode)
//
OT.$.registerCapability('bundle', function() {
  return OT.$.hasCapabilities('webrtc') &&
            (OT.$.env.name === 'Chrome' ||
              OT.$.env.name === 'Node' ||
              OTPlugin.isInstalled());
});

// Indicates if the browser supports RTCP Mux
//
// Broadly:
// * Older versions of Firefox (<= 25) don't support RTCP Mux
// * Older versions of Firefox (>= 26) support RTCP Mux (not tested yet)
// * Chrome support RTCP Mux
// * OT Plugin supports RTCP Mux
// * We assume NodeJs supports RTCP Mux (e.g. 'you're on your own' mode)
//
OT.$.registerCapability('RTCPMux', function() {
  return OT.$.hasCapabilities('webrtc') &&
              (OT.$.env.name === 'Chrome' ||
                OT.$.env.name === 'Node' ||
                OTPlugin.isInstalled());
});

OT.$.registerCapability('audioOutputLevelStat', function() {
  return OT.$.env.name === 'Chrome' || OT.$.env.name === 'IE' || OT.$.env.name === 'Opera';
});

OT.$.registerCapability('webAudioCapableRemoteStream', function() {
  return OT.$.env.name === 'Firefox';
});

OT.$.registerCapability('webAudio', function() {
  return 'AudioContext' in window;
});

// tb_require('../helpers.js')
// tb_require('./config.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

OT.Analytics = function(loggingUrl) {

  var LOG_VERSION = '1';
  var _analytics = new OT.$.Analytics(loggingUrl, OT.debug, OT._.getClientGuid);

  this.logError = function(code, type, message, details, options) {
    if (!options) options = {};
    var partnerId = options.partnerId;

    if (OT.Config.get('exceptionLogging', 'enabled', partnerId) !== true) {
      return;
    }

    OT._.getClientGuid(function(error, guid) {
      if (error) {
        // @todo
        return;
      }
      var data = OT.$.extend({
        // TODO: OT.properties.version only gives '2.2', not '2.2.9.3'.
        clientVersion: 'js-' + OT.properties.version.replace('v', ''),
        guid: guid,
        partnerId: partnerId,
        source: window.location.href,
        logVersion: LOG_VERSION,
        clientSystemTime: new Date().getTime()
      }, options);
      _analytics.logError(code, type, message, details, data);
    });

  };

  this.logEvent = function(options, throttle, completionHandler) {
    var partnerId = options.partnerId;

    if (!options) options = {};

    OT._.getClientGuid(function(error, guid) {
      if (error) {
        // @todo
        return;
      }

      // Set a bunch of defaults
      var data = OT.$.extend({
        // TODO: OT.properties.version only gives '2.2', not '2.2.9.3'.
        clientVersion: 'js-' + OT.properties.version.replace('v', ''),
        guid: guid,
        partnerId: partnerId,
        source: window.location.href,
        logVersion: LOG_VERSION,
        clientSystemTime: new Date().getTime()
      }, options);
      _analytics.logEvent(data, false, throttle, completionHandler);
    });
  };

  this.logQOS = function(options) {
    var partnerId = options.partnerId;

    if (!options) options = {};

    OT._.getClientGuid(function(error, guid) {
      if (error) {
        // @todo
        return;
      }

      // Set a bunch of defaults
      var data = OT.$.extend({
        // TODO: OT.properties.version only gives '2.2', not '2.2.9.3'.
        clientVersion: 'js-' + OT.properties.version.replace('v', ''),
        guid: guid,
        partnerId: partnerId,
        source: window.location.href,
        logVersion: LOG_VERSION,
        clientSystemTime: new Date().getTime(),
        duration: 0 //in milliseconds
      }, options);

      _analytics.logQOS(data);
    });
  };
};

// tb_require('../helpers.js')
// tb_require('./config.js')
// tb_require('./analytics.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

OT.ConnectivityAttemptPinger = function(options) {
  var _state = 'Initial',
    _previousState,
    states = ['Initial', 'Attempt',  'Success', 'Failure'],
    pingTimer,               // Timer for the Attempting pings
    PING_INTERVAL = 5000,
    PING_COUNT_TOTAL = 6,
    pingCount;

  //// Private API
  var stateChanged = function(newState) {
      _state = newState;
      var invalidSequence = false;
      switch (_state) {
        case 'Attempt':
          if (_previousState !== 'Initial') {
            invalidSequence = true;
          }
          startAttemptPings();
          break;
        case 'Success':
          if (_previousState !== 'Attempt') {
            invalidSequence = true;
          }
          stopAttemptPings();
          break;
        case 'Failure':
          if (_previousState !== 'Attempt') {
            invalidSequence = true;
          }
          stopAttemptPings();
          break;
      }
      if (invalidSequence) {
        var data = options ? OT.$.clone(options) : {};
        data.action = 'Internal Error';
        data.variation = 'Non-fatal';
        data.payload = {
          debug: 'Invalid sequence: ' + options.action + ' ' +
            _previousState + ' -> ' + _state
        };
        OT.analytics.logEvent(data);
      }
    },

    setState = OT.$.statable(this, states, 'Failure', stateChanged),

    startAttemptPings = function() {
      pingCount = 0;
      pingTimer = setInterval(function() {
        if (pingCount < PING_COUNT_TOTAL) {
          var data = OT.$.extend(options, {variation: 'Attempting'});
          OT.analytics.logEvent(data);
        } else {
          stopAttemptPings();
        }
        pingCount++;
      }, PING_INTERVAL);
    },

    stopAttemptPings = function() {
      clearInterval(pingTimer);
    };

  this.setVariation = function(variation) {
    _previousState = _state;
    setState(variation);

    // We could change the ConnectivityAttemptPinger to a ConnectivityAttemptLogger
    // that also logs events in addition to logging the ping ('Attempting') events.
    //
    // var payload = OT.$.extend(options, {variation:variation});
    // OT.analytics.logEvent(payload);
  };

  this.stop = function() {
    stopAttemptPings();
  };
};

// tb_require('../helpers/helpers.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

/* Stylable Notes
 * Some bits are controlled by multiple flags, i.e. buttonDisplayMode and nameDisplayMode.
 * When there are multiple flags how is the final setting chosen?
 * When some style bits are set updates will need to be pushed through to the Chrome
 */

// Mixes the StylableComponent behaviour into the +self+ object. It will
// also set the default styles to +initialStyles+.
//
// @note This Mixin is dependent on OT.Eventing.
//
//
// @example
//
//  function SomeObject {
//      OT.StylableComponent(this, {
//          name: 'SomeObject',
//          foo: 'bar'
//      });
//  }
//
//  var obj = new SomeObject();
//  obj.getStyle('foo');        // => 'bar'
//  obj.setStyle('foo', 'baz')
//  obj.getStyle('foo');        // => 'baz'
//  obj.getStyle();             // => {name: 'SomeObject', foo: 'baz'}
//
OT.StylableComponent = function(self, initalStyles, showControls, logSetStyleWithPayload) {
  if (!self.trigger) {
    throw new Error('OT.StylableComponent is dependent on the mixin OT.$.eventing. ' +
      'Ensure that this is included in the object before StylableComponent.');
  }

  var _readOnly = false;

  // Broadcast style changes as the styleValueChanged event
  var onStyleChange = function(key, value, oldValue) {
    if (oldValue) {
      self.trigger('styleValueChanged', key, value, oldValue);
    } else {
      self.trigger('styleValueChanged', key, value);
    }
  };

  if (showControls === false) {
    initalStyles = {
      buttonDisplayMode: 'off',
      nameDisplayMode: 'off',
      audioLevelDisplayMode: 'off'
    };

    _readOnly = true;
    logSetStyleWithPayload({
      showControls: false
    });
  }

  var _style = new Style(initalStyles, onStyleChange);

  /**
   * Returns an object that has the properties that define the current user interface controls of
   * the Publisher. You can modify the properties of this object and pass the object to the
   * <code>setStyle()</code> method of thePublisher object. (See the documentation for
   * <a href="#setStyle">setStyle()</a> to see the styles that define this object.)
   * @return {Object} The object that defines the styles of the Publisher.
   * @see <a href="#setStyle">setStyle()</a>
   * @method #getStyle
   * @memberOf Publisher
   */
   
  /**
   * Returns an object that has the properties that define the current user interface controls of
   * the Subscriber. You can modify the properties of this object and pass the object to the
   * <code>setStyle()</code> method of the Subscriber object. (See the documentation for
   * <a href="#setStyle">setStyle()</a> to see the styles that define this object.)
   * @return {Object} The object that defines the styles of the Subscriber.
   * @see <a href="#setStyle">setStyle()</a>
   * @method #getStyle
   * @memberOf Subscriber
   */
  // If +key+ is falsly then all styles will be returned.
  self.getStyle = function(key) {
    return _style.get(key);
  };

  /**
   * Sets properties that define the appearance of some user interface controls of the Publisher.
   *
   * <p>You can either pass one parameter or two parameters to this method.</p>
   *
   * <p>If you pass one parameter, <code>style</code>, it is an object that has the following
   * properties:
   *
   *     <ul>
   *       <li><code>audioLevelDisplayMode</code> (String) &mdash; How to display the audio level
   *       indicator. Possible values are: <code>"auto"</code> (the indicator is displayed when the
   *       video is disabled), <code>"off"</code> (the indicator is not displayed), and
   *       <code>"on"</code> (the indicator is always displayed).</li>
   *
   *       <li><code>backgroundImageURI</code> (String) &mdash; A URI for an image to display as
   *       the background image when a video is not displayed. (A video may not be displayed if
   *       you call <code>publishVideo(false)</code> on the Publisher object). You can pass an http
   *       or https URI to a PNG, JPEG, or non-animated GIF file location. You can also use the
   *       <code>data</code> URI scheme (instead of http or https) and pass in base-64-encrypted
   *       PNG data, such as that obtained from the
   *       <a href="Publisher.html#getImgData">Publisher.getImgData()</a> method. For example,
   *       you could set the property to <code>"data:VBORw0KGgoAA..."</code>, where the portion of
   *       the string after <code>"data:"</code> is the result of a call to
   *       <code>Publisher.getImgData()</code>. If the URL or the image data is invalid, the
   *       property is ignored (the attempt to set the image fails silently).</li>
   *
   *       <li><code>buttonDisplayMode</code> (String) &mdash; How to display the microphone
   *       controls. Possible values are: <code>"auto"</code> (controls are displayed when the
   *       stream is first displayed and when the user mouses over the display), <code>"off"</code>
   *       (controls are not displayed), and <code>"on"</code> (controls are always displayed).</li>
   *
   *       <li><code>nameDisplayMode</code> (String) &#151; Whether to display the stream name.
   *       Possible values are: <code>"auto"</code> (the name is displayed when the stream is first
   *       displayed and when the user mouses over the display), <code>"off"</code> (the name is not
   *       displayed), and <code>"on"</code> (the name is always displayed).</li>
   *   </ul>
   * </p>
   *
   * <p>For example, the following code passes one parameter to the method:</p>
   *
   * <pre>myPublisher.setStyle({nameDisplayMode: "off"});</pre>
   *
   * <p>If you pass two parameters, <code>style</code> and <code>value</code>, they are
   * key-value pair that define one property of the display style. For example, the following
   * code passes two parameter values to the method:</p>
   *
   * <pre>myPublisher.setStyle("nameDisplayMode", "off");</pre>
   *
   * <p>You can set the initial settings when you call the <code>Session.publish()</code>
   * or <code>OT.initPublisher()</code> method. Pass a <code>style</code> property as part of the
   * <code>properties</code> parameter of the method.</p>
   *
   * <p>The OT object dispatches an <code>exception</code> event if you pass in an invalid style
   * to the method. The <code>code</code> property of the ExceptionEvent object is set to 1011.</p>
   *
   * @param {Object} style Either an object containing properties that define the style, or a
   * String defining this single style property to set.
   * @param {String} value The value to set for the <code>style</code> passed in. Pass a value
   * for this parameter only if the value of the <code>style</code> parameter is a String.</p>
   *
   * @see <a href="#getStyle">getStyle()</a>
   * @return {Publisher} The Publisher object
   * @see <a href="#setStyle">setStyle()</a>
   *
   * @see <a href="Session.html#subscribe">Session.publish()</a>
   * @see <a href="OT.html#initPublisher">OT.initPublisher()</a>
   * @method #setStyle
   * @memberOf Publisher
   */
   
  /**
   * Sets properties that define the appearance of some user interface controls of the Subscriber.
   *
   * <p>You can either pass one parameter or two parameters to this method.</p>
   *
   * <p>If you pass one parameter, <code>style</code>, it is an object that has the following
   * properties:
   *
   *     <ul>
   *       <li><code>audioLevelDisplayMode</code> (String) &mdash; How to display the audio level
   *       indicator. Possible values are: <code>"auto"</code> (the indicator is displayed when the
   *       video is disabled), <code>"off"</code> (the indicator is not displayed), and
   *       <code>"on"</code> (the indicator is always displayed).</li>
   *
   *       <li><code>backgroundImageURI</code> (String) &mdash; A URI for an image to display as
   *       the background image when a video is not displayed. (A video may not be displayed if
   *       you call <code>subscribeToVideo(false)</code> on the Publisher object). You can pass an
   *       http or https URI to a PNG, JPEG, or non-animated GIF file location. You can also use the
   *       <code>data</code> URI scheme (instead of http or https) and pass in base-64-encrypted
   *       PNG data, such as that obtained from the
   *       <a href="Subscriber.html#getImgData">Subscriber.getImgData()</a> method. For example,
   *       you could set the property to <code>"data:VBORw0KGgoAA..."</code>, where the portion of
   *       the string after <code>"data:"</code> is the result of a call to
   *       <code>Publisher.getImgData()</code>. If the URL or the image data is invalid, the
   *       property is ignored (the attempt to set the image fails silently).</li>
   *
   *       <li><code>buttonDisplayMode</code> (String) &mdash; How to display the speaker
   *       controls. Possible values are: <code>"auto"</code> (controls are displayed when the
   *       stream is first displayed and when the user mouses over the display), <code>"off"</code>
   *       (controls are not displayed), and <code>"on"</code> (controls are always displayed).</li>
   *
   *       <li><code>nameDisplayMode</code> (String) &#151; Whether to display the stream name.
   *       Possible values are: <code>"auto"</code> (the name is displayed when the stream is first
   *       displayed and when the user mouses over the display), <code>"off"</code> (the name is not
   *       displayed), and <code>"on"</code> (the name is always displayed).</li>
   *
   *       <li><code>videoDisabledDisplayMode</code> (String) &#151; Whether to display the video
   *       disabled indicator and video disabled warning icons for a Subscriber. These icons
   *       indicate that the video has been disabled (or is in risk of being disabled for
   *       the warning icon) due to poor stream quality. Possible values are: <code>"auto"</code>
   *       (the icons are automatically when the displayed video is disabled or in risk of being
   *       disabled due to poor stream quality), <code>"off"</code> (do not display the icons), and
   *       <code>"on"</code> (display the icons).</li>
   *   </ul>
   * </p>
   *
   * <p>For example, the following code passes one parameter to the method:</p>
   *
   * <pre>mySubscriber.setStyle({nameDisplayMode: "off"});</pre>
   *
   * <p>If you pass two parameters, <code>style</code> and <code>value</code>, they are key-value
   * pair that define one property of the display style. For example, the following code passes
   * two parameter values to the method:</p>
   *
   * <pre>mySubscriber.setStyle("nameDisplayMode", "off");</pre>
   *
   * <p>You can set the initial settings when you call the <code>Session.subscribe()</code> method.
   * Pass a <code>style</code> property as part of the <code>properties</code> parameter of the
   * method.</p>
   *
   * <p>The OT object dispatches an <code>exception</code> event if you pass in an invalid style
   * to the method. The <code>code</code> property of the ExceptionEvent object is set to 1011.</p>
   *
   * @param {Object} style Either an object containing properties that define the style, or a
   * String defining this single style property to set.
   * @param {String} value The value to set for the <code>style</code> passed in. Pass a value
   * for this parameter only if the value of the <code>style</code> parameter is a String.</p>
   *
   * @returns {Subscriber} The Subscriber object.
   *
   * @see <a href="#getStyle">getStyle()</a>
   * @see <a href="#setStyle">setStyle()</a>
   *
   * @see <a href="Session.html#subscribe">Session.subscribe()</a>
   * @method #setStyle
   * @memberOf Subscriber
   */
   
  if (_readOnly) {
    self.setStyle = function() {
      OT.warn('Calling setStyle() has no effect because the' +
        'showControls option was set to false');
      return this;
    };
  } else {
    self.setStyle = function(keyOrStyleHash, value, silent) {
      var logPayload = {};
      if (typeof keyOrStyleHash !== 'string') {
        _style.setAll(keyOrStyleHash, silent);
        logPayload = keyOrStyleHash;
      } else {
        _style.set(keyOrStyleHash, value);
        logPayload[keyOrStyleHash] = value;
      }
      if (logSetStyleWithPayload) logSetStyleWithPayload(logPayload);
      return this;
    };
  }
};

/*jshint latedef:false */
var Style = function(initalStyles, onStyleChange) {
  /*jshint latedef:true */
  var _style = {},
      _COMPONENT_STYLES,
      _validStyleValues,
      isValidStyle,
      castValue;

  _COMPONENT_STYLES = [
    'showMicButton',
    'showSpeakerButton',
    'nameDisplayMode',
    'buttonDisplayMode',
    'backgroundImageURI',
    'audioLevelDisplayMode'
  ];

  _validStyleValues = {
    buttonDisplayMode: ['auto', 'mini', 'mini-auto', 'off', 'on'],
    nameDisplayMode: ['auto', 'off', 'on'],
    audioLevelDisplayMode: ['auto', 'off', 'on'],
    showSettingsButton: [true, false],
    showMicButton: [true, false],
    backgroundImageURI: null,
    showControlBar: [true, false],
    showArchiveStatus: [true, false],
    videoDisabledDisplayMode: ['auto', 'off', 'on']
  };

  // Validates the style +key+ and also whether +value+ is valid for +key+
  isValidStyle = function(key, value) {
    return key === 'backgroundImageURI' ||
      (_validStyleValues.hasOwnProperty(key) &&
        OT.$.arrayIndexOf(_validStyleValues[key], value) !== -1);
  };

  castValue = function(value) {
    switch (value) {
      case 'true':
        return true;
      case 'false':
        return false;
      default:
        return value;
    }
  };

  // Returns a shallow copy of the styles.
  this.getAll = function() {
    var style = OT.$.clone(_style);

    for (var key in style) {
      if (!style.hasOwnProperty(key)) {
        continue;
      }
      if (OT.$.arrayIndexOf(_COMPONENT_STYLES, key) < 0) {

        // Strip unnecessary properties out, should this happen on Set?
        delete style[key];
      }
    }

    return style;
  };

  this.get = function(key) {
    if (key) {
      return _style[key];
    }

    // We haven't been asked for any specific key, just return the lot
    return this.getAll();
  };

  // *note:* this will not trigger onStyleChange if +silent+ is truthy
  this.setAll = function(newStyles, silent) {
    var oldValue, newValue;

    for (var key in newStyles) {
      if (!newStyles.hasOwnProperty(key)) {
        continue;
      }
      newValue = castValue(newStyles[key]);

      if (isValidStyle(key, newValue)) {
        oldValue = _style[key];

        if (newValue !== oldValue) {
          _style[key] = newValue;
          if (!silent) onStyleChange(key, newValue, oldValue);
        }

      } else {
        OT.warn('Style.setAll::Invalid style property passed ' + key + ' : ' + newValue);
      }
    }

    return this;
  };

  this.set = function(key, value) {
    OT.debug('setStyle: ' + key.toString());

    var newValue = castValue(value),
        oldValue;

    if (!isValidStyle(key, newValue)) {
      OT.warn('Style.set::Invalid style property passed ' + key + ' : ' + newValue);
      return this;
    }

    oldValue = _style[key];
    if (newValue !== oldValue) {
      _style[key] = newValue;

      onStyleChange(key, value, oldValue);
    }

    return this;
  };

  if (initalStyles) this.setAll(initalStyles, true);
};

// tb_require('../helpers/helpers.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

// A Factory method for generating simple state machine classes.
//
// @usage
//    var StateMachine = OT.generateSimpleStateMachine('start', ['start', 'middle', 'end', {
//      start: ['middle'],
//      middle: ['end'],
//      end: ['start']
//    }]);
//
//    var states = new StateMachine();
//    state.current;            // <-- start
//    state.set('middle');
//
OT.generateSimpleStateMachine = function(initialState, states, transitions) {
  var validStates = states.slice(),
      validTransitions = OT.$.clone(transitions);

  var isValidState = function(state) {
    return OT.$.arrayIndexOf(validStates, state) !== -1;
  };

  var isValidTransition = function(fromState, toState) {
    return validTransitions[fromState] &&
      OT.$.arrayIndexOf(validTransitions[fromState], toState) !== -1;
  };

  return function(stateChangeFailed) {
    var currentState = initialState,
        previousState = null;

    this.current = currentState;

    function signalChangeFailed(message, newState) {
      stateChangeFailed({
        message: message,
        newState: newState,
        currentState: currentState,
        previousState: previousState
      });
    }

    // Validates +newState+. If it's invalid it triggers stateChangeFailed and returns false.
    function handleInvalidStateChanges(newState) {
      if (!isValidState(newState)) {
        signalChangeFailed('\'' + newState + '\' is not a valid state', newState);

        return false;
      }

      if (!isValidTransition(currentState, newState)) {
        signalChangeFailed('\'' + currentState + '\' cannot transition to \'' +
          newState + '\'', newState);

        return false;
      }

      return true;
    }

    this.set = function(newState) {
      if (!handleInvalidStateChanges(newState)) return;
      previousState = currentState;
      this.current = currentState = newState;
    };

  };
};

// tb_require('../helpers/helpers.js')
// tb_require('./state_machine.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

!(function() {

  // Models a Subscriber's subscribing State
  //
  // Valid States:
  //     NotSubscribing            (the initial state
  //     Init                      (basic setup of DOM
  //     ConnectingToPeer          (Failure Cases -> No Route, Bad Offer, Bad Answer
  //     BindingRemoteStream       (Failure Cases -> Anything to do with the media being
  //                               (invalid, the media never plays
  //     Subscribing               (this is 'onLoad'
  //     Failed                    (terminal state, with a reason that maps to one of the
  //                               (failure cases above
  //     Destroyed                 (The subscriber has been cleaned up, terminal state
  //
  //
  // Valid Transitions:
  //     NotSubscribing ->
  //         Init
  //
  //     Init ->
  //             ConnectingToPeer
  //           | BindingRemoteStream         (if we are subscribing to ourselves and we alreay
  //                                         (have a stream
  //           | NotSubscribing              (destroy()
  //
  //     ConnectingToPeer ->
  //             BindingRemoteStream
  //           | NotSubscribing
  //           | Failed
  //           | NotSubscribing              (destroy()
  //
  //     BindingRemoteStream ->
  //             Subscribing
  //           | Failed
  //           | NotSubscribing              (destroy()
  //
  //     Subscribing ->
  //             NotSubscribing              (unsubscribe
  //           | Failed                      (probably a peer connection failure after we began
  //                                         (subscribing
  //
  //     Failed ->
  //             Destroyed
  //
  //     Destroyed ->                        (terminal state)
  //
  //
  // @example
  //     var state = new SubscribingState(function(change) {
  //       console.log(change.message);
  //     });
  //
  //     state.set('Init');
  //     state.current;                 -> 'Init'
  //
  //     state.set('Subscribing');      -> triggers stateChangeFailed and logs out the error message
  //
  //
  var validStates,
      validTransitions,
      initialState = 'NotSubscribing';

  validStates = [
    'NotSubscribing', 'Init', 'ConnectingToPeer',
    'BindingRemoteStream', 'Subscribing', 'Failed',
    'Destroyed'
  ];

  validTransitions = {
    NotSubscribing: ['NotSubscribing', 'Init', 'Destroyed'],
    Init: ['NotSubscribing', 'ConnectingToPeer', 'BindingRemoteStream', 'Destroyed'],
    ConnectingToPeer: ['NotSubscribing', 'BindingRemoteStream', 'Failed', 'Destroyed'],
    BindingRemoteStream: ['NotSubscribing', 'Subscribing', 'Failed', 'Destroyed'],
    Subscribing: ['NotSubscribing', 'Failed', 'Destroyed'],
    Failed: ['Destroyed'],
    Destroyed: []
  };

  OT.SubscribingState = OT.generateSimpleStateMachine(initialState, validStates, validTransitions);

  OT.SubscribingState.prototype.isDestroyed = function() {
    return this.current === 'Destroyed';
  };

  OT.SubscribingState.prototype.isFailed = function() {
    return this.current === 'Failed';
  };

  OT.SubscribingState.prototype.isSubscribing = function() {
    return this.current === 'Subscribing';
  };

  OT.SubscribingState.prototype.isAttemptingToSubscribe = function() {
    return OT.$.arrayIndexOf(
      ['Init', 'ConnectingToPeer', 'BindingRemoteStream'],
      this.current
    ) !== -1;
  };

})(window);

// tb_require('../helpers/helpers.js')
// tb_require('./state_machine.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

!(function() {

  // Models a Publisher's publishing State
  //
  // Valid States:
  //    NotPublishing
  //    GetUserMedia
  //    BindingMedia
  //    MediaBound
  //    PublishingToSession
  //    Publishing
  //    Failed
  //    Destroyed
  //
  //
  // Valid Transitions:
  //    NotPublishing ->
  //        GetUserMedia
  //
  //    GetUserMedia ->
  //        BindingMedia
  //      | Failed                     (Failure Reasons -> stream error, constraints,
  //                                   (permission denied
  //      | NotPublishing              (destroy()
  //
  //
  //    BindingMedia ->
  //        MediaBound
  //      | Failed                     (Failure Reasons -> Anything to do with the media
  //                                   (being invalid, the media never plays
  //      | NotPublishing              (destroy()
  //
  //    MediaBound ->
  //        PublishingToSession        (MediaBound could transition to PublishingToSession
  //                                   (if a stand-alone publish is bound to a session
  //      | Failed                     (Failure Reasons -> media issues with a stand-alone publisher
  //      | NotPublishing              (destroy()
  //
  //    PublishingToSession
  //        Publishing
  //      | Failed                     (Failure Reasons -> timeout while waiting for ack of
  //                                   (stream registered. We do not do this right now
  //      | NotPublishing              (destroy()
  //
  //
  //    Publishing ->
  //        NotPublishing              (Unpublish
  //      | Failed                     (Failure Reasons -> loss of network, media error, anything
  //                                   (that causes *all* Peer Connections to fail (less than all
  //                                   (failing is just an error, all is failure)
  //      | NotPublishing              (destroy()
  //
  //    Failed ->
  //       Destroyed
  //
  //    Destroyed ->                   (Terminal state
  //
  //

  var validStates = [
      'NotPublishing', 'GetUserMedia', 'BindingMedia', 'MediaBound',
      'PublishingToSession', 'Publishing', 'Failed',
      'Destroyed'
    ],

    validTransitions = {
      NotPublishing: ['NotPublishing', 'GetUserMedia', 'Destroyed'],
      GetUserMedia: ['BindingMedia', 'Failed', 'NotPublishing', 'Destroyed'],
      BindingMedia: ['MediaBound', 'Failed', 'NotPublishing', 'Destroyed'],
      MediaBound: ['NotPublishing', 'PublishingToSession', 'Failed', 'Destroyed'],
      PublishingToSession: ['NotPublishing', 'Publishing', 'Failed', 'Destroyed'],
      Publishing: ['NotPublishing', 'MediaBound', 'Failed', 'Destroyed'],
      Failed: ['Destroyed'],
      Destroyed: []
    },

    initialState = 'NotPublishing';

  OT.PublishingState = OT.generateSimpleStateMachine(initialState, validStates, validTransitions);

  OT.PublishingState.prototype.isDestroyed = function() {
    return this.current === 'Destroyed';
  };

  OT.PublishingState.prototype.isAttemptingToPublish = function() {
    return OT.$.arrayIndexOf(
      ['GetUserMedia', 'BindingMedia', 'MediaBound', 'PublishingToSession'],
      this.current) !== -1;
  };

  OT.PublishingState.prototype.isPublishing = function() {
    return this.current === 'Publishing';
  };

})(window);

// tb_require('../helpers/helpers.js')
// tb_require('../helpers/lib/web_rtc.js')

!(function() {

  /* jshint globalstrict: true, strict: false, undef: true, unused: true,
            trailing: true, browser: true, smarttabs:true */
  /* global OT */

  /*
   * A Publishers Microphone.
   *
   * TODO
   * * bind to changes in mute/unmute/volume/etc and respond to them
   */
  OT.Microphone = function(webRTCStream, muted) {
    var _muted;

    OT.$.defineProperties(this, {
      muted: {
        get: function() {
          return _muted;
        },
        set: function(muted) {
          if (_muted === muted) return;

          _muted = muted;

          var audioTracks = webRTCStream.getAudioTracks();

          for (var i = 0, num = audioTracks.length; i < num; ++i) {
            audioTracks[i].setEnabled(!_muted);
          }
        }
      }
    });

    // Set the initial value
    if (muted !== undefined) {
      this.muted(muted === true);

    } else if (webRTCStream.getAudioTracks().length) {
      this.muted(!webRTCStream.getAudioTracks()[0].enabled);

    } else {
      this.muted(false);
    }

  };

})(window);

// tb_require('../helpers/helpers.js')

!(function() {
  /* jshint globalstrict: true, strict: false, undef: true, unused: true,
            trailing: true, browser: true, smarttabs:true */
  /* global OT */

  /**
   * The Event object defines the basic OpenTok event object that is passed to
   * event listeners. Other OpenTok event classes implement the properties and methods of
   * the Event object.</p>
   *
   * <p>For example, the Stream object dispatches a <code>streamPropertyChanged</code> event when
   * the stream's properties are updated. You add a callback for an event using the
   * <code>on()</code> method of the Stream object:</p>
   *
   * <pre>
   * stream.on("streamPropertyChanged", function (event) {
   *     alert("Properties changed for stream " + event.target.streamId);
   * });</pre>
   *
   * @class Event
   * @property {Boolean} cancelable Whether the event has a default behavior that is cancelable
   * (<code>true</code>) or not (<code>false</code>). You can cancel the default behavior by
   * calling the <code>preventDefault()</code> method of the Event object in the callback
   * function. (See <a href="#preventDefault">preventDefault()</a>.)
   *
   * @property {Object} target The object that dispatched the event.
   *
   * @property {String} type  The type of event.
   */
  OT.Event = OT.$.Event();
  /**
  * Prevents the default behavior associated with the event from taking place.
  *
  * <p>To see whether an event has a default behavior, check the <code>cancelable</code> property
  * of the event object. </p>
  *
  * <p>Call the <code>preventDefault()</code> method in the callback function for the event.</p>
  *
  * <p>The following events have default behaviors:</p>
  *
  * <ul>
  *
  *   <li><code>sessionDisconnect</code> &#151; See
  *   <a href="SessionDisconnectEvent.html#preventDefault">
  *   SessionDisconnectEvent.preventDefault()</a>.</li>
  *
  *   <li><code>streamDestroyed</code> &#151; See <a href="StreamEvent.html#preventDefault">
  *   StreamEvent.preventDefault()</a>.</li>
  *
  *   <li><code>accessDialogOpened</code> &#151; See the
  *   <a href="Publisher.html#event:accessDialogOpened">accessDialogOpened event</a>.</li>
  *
  *   <li><code>accessDenied</code> &#151; See the <a href="Publisher.html#event:accessDenied">
  *   accessDenied event</a>.</li>
  *
  * </ul>
  *
  * @method #preventDefault
  * @memberof Event
  */
  /**
  * Whether the default event behavior has been prevented via a call to
  * <code>preventDefault()</code> (<code>true</code>) or not (<code>false</code>).
  * See <a href="#preventDefault">preventDefault()</a>.
  * @method #isDefaultPrevented
  * @return {Boolean}
  * @memberof Event
  */

  // Event names lookup
  OT.Event.names = {
    // Activity Status for cams/mics
    ACTIVE: 'active',
    INACTIVE: 'inactive',
    UNKNOWN: 'unknown',

    // Archive types
    PER_SESSION: 'perSession',
    PER_STREAM: 'perStream',

    // OT Events
    EXCEPTION: 'exception',
    ISSUE_REPORTED: 'issueReported',

    // Session Events
    SESSION_CONNECTED: 'sessionConnected',
    SESSION_DISCONNECTED: 'sessionDisconnected',
    STREAM_CREATED: 'streamCreated',
    STREAM_DESTROYED: 'streamDestroyed',
    CONNECTION_CREATED: 'connectionCreated',
    CONNECTION_DESTROYED: 'connectionDestroyed',
    SIGNAL: 'signal',
    STREAM_PROPERTY_CHANGED: 'streamPropertyChanged',
    MICROPHONE_LEVEL_CHANGED: 'microphoneLevelChanged',

    // Publisher Events
    RESIZE: 'resize',
    SETTINGS_BUTTON_CLICK: 'settingsButtonClick',
    DEVICE_INACTIVE: 'deviceInactive',
    INVALID_DEVICE_NAME: 'invalidDeviceName',
    ACCESS_ALLOWED: 'accessAllowed',
    ACCESS_DENIED: 'accessDenied',
    ACCESS_DIALOG_OPENED: 'accessDialogOpened',
    ACCESS_DIALOG_CLOSED: 'accessDialogClosed',
    ECHO_CANCELLATION_MODE_CHANGED: 'echoCancellationModeChanged',
    MEDIA_STOPPED: 'mediaStopped',
    PUBLISHER_DESTROYED: 'destroyed',

    // Subscriber Events
    SUBSCRIBER_DESTROYED: 'destroyed',

    // DeviceManager Events
    DEVICES_DETECTED: 'devicesDetected',

    // DevicePanel Events
    DEVICES_SELECTED: 'devicesSelected',
    CLOSE_BUTTON_CLICK: 'closeButtonClick',

    MICLEVEL: 'microphoneActivityLevel',
    MICGAINCHANGED: 'microphoneGainChanged',

    // Environment Loader
    ENV_LOADED: 'envLoaded',
    ENV_UNLOADED: 'envUnloaded',

    // Audio activity Events
    AUDIO_LEVEL_UPDATED: 'audioLevelUpdated'
  };

  OT.ExceptionCodes = {
    JS_EXCEPTION: 2000,
    AUTHENTICATION_ERROR: 1004,
    INVALID_SESSION_ID: 1005,
    CONNECT_FAILED: 1006,
    CONNECT_REJECTED: 1007,
    CONNECTION_TIMEOUT: 1008,
    NOT_CONNECTED: 1010,
    INVALID_PARAMETER: 1011,
    P2P_CONNECTION_FAILED: 1013,
    API_RESPONSE_FAILURE: 1014,
    TERMS_OF_SERVICE_FAILURE: 1026,
    UNABLE_TO_PUBLISH: 1500,
    UNABLE_TO_SUBSCRIBE: 1501,
    UNABLE_TO_FORCE_DISCONNECT: 1520,
    UNABLE_TO_FORCE_UNPUBLISH: 1530,
    PUBLISHER_ICE_WORKFLOW_FAILED: 1553,
    SUBSCRIBER_ICE_WORKFLOW_FAILED: 1554,
    UNEXPECTED_SERVER_RESPONSE: 2001,
    REPORT_ISSUE_ERROR: 2011
  };

  /**
  * The {@link OT} class dispatches <code>exception</code> events when the OpenTok API encounters
  * an exception (error). The ExceptionEvent object defines the properties of the event
  * object that is dispatched.
  *
  * <p>Note that you set up a callback for the <code>exception</code> event by calling the
  * <code>OT.on()</code> method.</p>
  *
  * @class ExceptionEvent
  * @property {Number} code The error code. The following is a list of error codes:</p>
  *
  * <table class="docs_table">
  *  <tbody><tr>
  *   <td>
  *   <b>code</b>
  *
  *   </td>
  *   <td>
  *   <b>title</b>
  *   </td>
  *  </tr>
  *
  *  <tr>
  *   <td>
  *   1004
  *
  *   </td>
  *   <td>
  *   Authentication error
  *   </td>
  *  </tr>
  *
  *  <tr>
  *   <td>
  *   1005
  *
  *   </td>
  *   <td>
  *   Invalid Session ID
  *   </td>
  *  </tr>
  *  <tr>
  *   <td>
  *   1006
  *
  *   </td>
  *   <td>
  *   Connect Failed
  *   </td>
  *  </tr>
  *  <tr>
  *   <td>
  *   1007
  *
  *   </td>
  *   <td>
  *   Connect Rejected
  *   </td>
  *  </tr>
  *  <tr>
  *   <td>
  *   1008
  *
  *   </td>
  *   <td>
  *   Connect Time-out
  *   </td>
  *  </tr>
  *  <tr>
  *   <td>
  *   1009
  *
  *   </td>
  *   <td>
  *   Security Error
  *   </td>
  *  </tr>
  *   <tr>
  *    <td>
  *    1010
  *
  *    </td>
  *    <td>
  *    Not Connected
  *    </td>
  *   </tr>
  *   <tr>
  *    <td>
  *    1011
  *
  *    </td>
  *    <td>
  *    Invalid Parameter
  *    </td>
  *   </tr>
  *   <tr>
  *    <td>
  *    1013
  *    </td>
  *    <td>
  *    Connection Failed
  *    </td>
  *   </tr>
  *   <tr>
  *    <td>
  *    1014
  *    </td>
  *    <td>
  *    API Response Failure
  *    </td>
  *   </tr>
  *   <tr>
  *    <td>
  *    1026
  *    </td>
  *    <td>
  *    Terms of Service Violation: Export Compliance
  *    </td>
  *   </tr>
  *  <tr>
  *    <td>
  *    1500
  *    </td>
  *    <td>
  *    Unable to Publish
  *    </td>
  *   </tr>
  *
  *  <tr>
  *    <td>
  *    1520
  *    </td>
  *    <td>
  *    Unable to Force Disconnect
  *    </td>
  *   </tr>
  *
  *  <tr>
  *    <td>
  *    1530
  *    </td>
  *    <td>
  *    Unable to Force Unpublish
  *    </td>
  *   </tr>
  *  <tr>
  *    <td>
  *    1535
  *    </td>
  *    <td>
  *    Force Unpublish on Invalid Stream
  *    </td>
  *   </tr>
  *
  *  <tr>
  *    <td>
  *    2000
  *
  *    </td>
  *    <td>
  *    Internal Error
  *    </td>
  *  </tr>
  *
  *  <tr>
  *    <td>
  *    2010
  *
  *    </td>
  *    <td>
  *    Report Issue Failure
  *    </td>
  *  </tr>
  *
  *
  *  </tbody></table>
  *
  *  <p>Check the <code>message</code> property for more details about the error.</p>
  *
  * @property {String} message The error message.
  *
  * @property {Object} target The object that the event pertains to. For an
  * <code>exception</code> event, this will be an object other than the OT object
  * (such as a Session object or a Publisher object).
  *
  * @property {String} title The error title.
  * @augments Event
  */
  OT.ExceptionEvent = function(type, message, title, code, component, target) {
    OT.Event.call(this, type);

    this.message = message;
    this.title = title;
    this.code = code;
    this.component = component;
    this.target = target;
  };

  OT.IssueReportedEvent = function(type, issueId) {
    OT.Event.call(this, type);
    this.issueId = issueId;
  };

  // Triggered when the JS dynamic config and the DOM have loaded.
  OT.EnvLoadedEvent = function(type) {
    OT.Event.call(this, type);
  };

  /**
   * Defines <code>connectionCreated</code> and <code>connectionDestroyed</code> events dispatched
   * by the {@link Session} object.
   * <p>
   * The Session object dispatches a <code>connectionCreated</code> event when a client (including
   * your own) connects to a Session. It also dispatches a <code>connectionCreated</code> event for
   * every client in the session when you first connect. (when your local client connects, the
   * Session object also dispatches a <code>sessionConnected</code> event, defined by the
   * {@link SessionConnectEvent} class.)
   * <p>
   * While you are connected to the session, the Session object dispatches a
   * <code>connectionDestroyed</code> event when another client disconnects from the Session.
   * (When you disconnect, the Session object also dispatches a <code>sessionDisconnected</code>
   * event, defined by the {@link SessionDisconnectEvent} class.)
   *
   * <h5><a href="example"></a>Example</h5>
   *
   * <p>The following code keeps a running total of the number of connections to a session
   * by monitoring the <code>connections</code> property of the <code>sessionConnect</code>,
   * <code>connectionCreated</code> and <code>connectionDestroyed</code> events:</p>
   *
   * <pre>var apiKey = ""; // Replace with your API key. See https://dashboard.tokbox.com/projects
   * var sessionID = ""; // Replace with your own session ID.
   *                     // See https://dashboard.tokbox.com/projects
   * var token = ""; // Replace with a generated token that has been assigned the moderator role.
   *                 // See https://dashboard.tokbox.com/projects
   * var connectionCount = 0;
   *
   * var session = OT.initSession(apiKey, sessionID);
   * session.on("connectionCreated", function(event) {
   *    connectionCount++;
   *    displayConnectionCount();
   * });
   * session.on("connectionDestroyed", function(event) {
   *    connectionCount--;
   *    displayConnectionCount();
   * });
   * session.connect(token);
   *
   * function displayConnectionCount() {
   *     document.getElementById("connectionCountField").value = connectionCount.toString();
   * }</pre>
   *
   * <p>This example assumes that there is an input text field in the HTML DOM
   * with the <code>id</code> set to <code>"connectionCountField"</code>:</p>
   *
   * <pre>&lt;input type="text" id="connectionCountField" value="0"&gt;&lt;/input&gt;</pre>
   *
   *
   * @property {Connection} connection A Connection objects for the connections that was
   * created or deleted.
   *
   * @property {Array} connections Deprecated. Use the <code>connection</code> property. A
   * <code>connectionCreated</code> or <code>connectionDestroyed</code> event is dispatched
   * for each connection created and destroyed in the session.
   *
   * @property {String} reason For a <code>connectionDestroyed</code> event,
   *  a description of why the connection ended. This property can have two values:
   * </p>
   * <ul>
   *  <li><code>"clientDisconnected"</code> &#151; A client disconnected from the session by calling
   *     the <code>disconnect()</code> method of the Session object or by closing the browser.
   *     (See <a href="Session.html#disconnect">Session.disconnect()</a>.)</li>
   *
   *  <li><code>"forceDisconnected"</code> &#151; A moderator has disconnected the publisher
   *      from the session, by calling the <code>forceDisconnect()</code> method of the Session
   *      object. (See <a href="Session.html#forceDisconnect">Session.forceDisconnect()</a>.)</li>
   *
   *  <li><code>"networkDisconnected"</code> &#151; The network connection terminated abruptly
   *      (for example, the client lost their internet connection).</li>
   * </ul>
   *
   * <p>Depending on the context, this description may allow the developer to refine
   * the course of action they take in response to an event.</p>
   *
   * <p>For a <code>connectionCreated</code> event, this string is undefined.</p>
   *
   * @class ConnectionEvent
   * @augments Event
   */
  var connectionEventPluralDeprecationWarningShown = false;
  OT.ConnectionEvent = function(type, connection, reason) {
    OT.Event.call(this, type, false);

    if (OT.$.canDefineProperty) {
      Object.defineProperty(this, 'connections', {
        get: function() {
          if (!connectionEventPluralDeprecationWarningShown) {
            OT.warn('OT.ConnectionEvent connections property is deprecated, ' +
              'use connection instead.');
            connectionEventPluralDeprecationWarningShown = true;
          }
          return [connection];
        }
      });
    } else {
      this.connections = [connection];
    }

    this.connection = connection;
    this.reason = reason;
  };

  /**
   * StreamEvent is an event that can have the type "streamCreated" or "streamDestroyed".
   * These events are dispatched by the Session object when another client starts or
   * stops publishing a stream to a {@link Session}. For a local client's stream, the
   * Publisher object dispatches the event.
   *
   * <h4><a href="example_streamCreated"></a>Example &#151; streamCreated event dispatched
   * by the Session object</h4>
   *  <p>The following code initializes a session and sets up an event listener for when
   *    a stream published by another client is created:</p>
   *
   * <pre>
   * session.on("streamCreated", function(event) {
   *   // streamContainer is a DOM element
   *   subscriber = session.subscribe(event.stream, targetElement);
   * }).connect(token);
   * </pre>
   *
   *  <h4><a href="example_streamDestroyed"></a>Example &#151; streamDestroyed event dispatched
   * by the Session object</h4>
   *
   *    <p>The following code initializes a session and sets up an event listener for when
   *       other clients' streams end:</p>
   *
   * <pre>
   * session.on("streamDestroyed", function(event) {
   *     console.log("Stream " + event.stream.name + " ended. " + event.reason);
   * }).connect(token);
   * </pre>
   *
   * <h4><a href="example_streamCreated_publisher"></a>Example &#151; streamCreated event dispatched
   * by a Publisher object</h4>
   *  <p>The following code publishes a stream and adds an event listener for when the streaming
   * starts</p>
   *
   * <pre>
   * var publisher = session.publish(targetElement)
   *   .on("streamCreated", function(event) {
   *     console.log("Publisher started streaming.");
   *   );
   * </pre>
   *
   *  <h4><a href="example_streamDestroyed_publisher"></a>Example &#151; streamDestroyed event
   * dispatched by a Publisher object</h4>
   *
   *  <p>The following code publishes a stream, and leaves the Publisher in the HTML DOM
   * when the streaming stops:</p>
   *
   * <pre>
   * var publisher = session.publish(targetElement)
   *   .on("streamDestroyed", function(event) {
   *     event.preventDefault();
   *     console.log("Publisher stopped streaming.");
   *   );
   * </pre>
   *
   * @class StreamEvent
   *
   * @property {Boolean} cancelable   Whether the event has a default behavior that is cancelable
   *  (<code>true</code>) or not (<code>false</code>). You can cancel the default behavior by
   * calling the <code>preventDefault()</code> method of the StreamEvent object in the event
   * listener function. The <code>streamDestroyed</code> event is cancelable.
   * (See <a href="#preventDefault">preventDefault()</a>.)
   *
   * @property {String} reason For a <code>streamDestroyed</code> event,
   *  a description of why the session disconnected. This property can have one of the following
   *  values:
   * </p>
   * <ul>
   *  <li><code>"clientDisconnected"</code> &#151; A client disconnected from the session by calling
   *     the <code>disconnect()</code> method of the Session object or by closing the browser.
   *     (See <a href="Session.html#disconnect">Session.disconnect()</a>.)</li>
   *
   *  <li><code>"forceDisconnected"</code> &#151; A moderator has disconnected the publisher of the
   *    stream from the session, by calling the <code>forceDisconnect()</code> method of the Session
   *     object. (See <a href="Session.html#forceDisconnect">Session.forceDisconnect()</a>.)</li>
   *
   *  <li><code>"forceUnpublished"</code> &#151; A moderator has forced the publisher of the stream
   *    to stop publishing the stream, by calling the <code>forceUnpublish()</code> method of the
   *    Session object.
   *    (See <a href="Session.html#forceUnpublish">Session.forceUnpublish()</a>.)</li>
   *
   *  <li><code>"mediaStopped"</code> &#151; The user publishing the stream has stopped sharing the
   *    screen. This value is only used in screen-sharing video streams.</li>
   *
   *  <li><code>"networkDisconnected"</code> &#151; The network connection terminated abruptly (for
   *      example, the client lost their internet connection).</li>
   *
   * </ul>
   *
   * <p>Depending on the context, this description may allow the developer to refine
   * the course of action they take in response to an event.</p>
   *
   * <p>For a <code>streamCreated</code> event, this string is undefined.</p>
   *
   * @property {Stream} stream A Stream object corresponding to the stream that was added (in the
   * case of a <code>streamCreated</code> event) or deleted (in the case of a
   * <code>streamDestroyed</code> event).
   *
   * @property {Array} streams Deprecated. Use the <code>stream</code> property. A
   * <code>streamCreated</code> or <code>streamDestroyed</code> event is dispatched for
   * each stream added or destroyed.
   *
   * @augments Event
   */

  var streamEventPluralDeprecationWarningShown = false;
  OT.StreamEvent = function(type, stream, reason, cancelable) {
    OT.Event.call(this, type, cancelable);

    if (OT.$.canDefineProperty) {
      Object.defineProperty(this, 'streams', {
        get: function() {
          if (!streamEventPluralDeprecationWarningShown) {
            OT.warn('OT.StreamEvent streams property is deprecated, use stream instead.');
            streamEventPluralDeprecationWarningShown = true;
          }
          return [stream];
        }
      });
    } else {
      this.streams = [stream];
    }

    this.stream = stream;
    this.reason = reason;
  };

  /**
  * Prevents the default behavior associated with the event from taking place.
  *
  * <p>For the <code>streamDestroyed</code> event dispatched by the Session object,
  * the default behavior is that all Subscriber objects that are subscribed to the stream are
  * unsubscribed and removed from the HTML DOM. Each Subscriber object dispatches a
  * <code>destroyed</code> event when the element is removed from the HTML DOM. If you call the
  * <code>preventDefault()</code> method in the event listener for the <code>streamDestroyed</code>
  * event, the default behavior is prevented and you can clean up Subscriber objects using your
  * own code. See
  * <a href="Session.html#getSubscribersForStream">Session.getSubscribersForStream()</a>.</p>
  * <p>
  * For the <code>streamDestroyed</code> event dispatched by a Publisher object, the default
  * behavior is that the Publisher object is removed from the HTML DOM. The Publisher object
  * dispatches a <code>destroyed</code> event when the element is removed from the HTML DOM.
  * If you call the <code>preventDefault()</code> method in the event listener for the
  * <code>streamDestroyed</code> event, the default behavior is prevented, and you can
  * retain the Publisher for reuse or clean it up using your own code.
  *</p>
  * <p>To see whether an event has a default behavior, check the <code>cancelable</code> property of
  * the event object. </p>
  *
  * <p>
  *   Call the <code>preventDefault()</code> method in the event listener function for the event.
  * </p>
  *
  * @method #preventDefault
  * @memberof StreamEvent
*/

  /**
   * The Session object dispatches SessionConnectEvent object when a session has successfully
   * connected in response to a call to the <code>connect()</code> method of the Session object.
   * <p>
   * In version 2.2, the completionHandler of the <code>Session.connect()</code> method
   * indicates success or failure in connecting to the session.
   *
   * @class SessionConnectEvent
   * @property {Array} connections Deprecated in version 2.2 (and set to an empty array). In
   * version 2.2, listen for the <code>connectionCreated</code> event dispatched by the Session
   * object. In version 2.2, the Session object dispatches a <code>connectionCreated</code> event
   * for each connection (including your own). This includes connections present when you first
   * connect to the session.
   *
   * @property {Array} streams Deprecated in version 2.2 (and set to an empty array). In version
   * 2.2, listen for the <code>streamCreated</code> event dispatched by the Session object. In
   * version 2.2, the Session object dispatches a <code>streamCreated</code> event for each stream
   * other than those published by your client. This includes streams
   * present when you first connect to the session.
   *
   * @see <a href="Session.html#connect">Session.connect()</a></p>
   * @augments Event
   */

  var sessionConnectedConnectionsDeprecationWarningShown = false;
  var sessionConnectedStreamsDeprecationWarningShown = false;
  var sessionConnectedArchivesDeprecationWarningShown = false;

  OT.SessionConnectEvent = function(type) {
    OT.Event.call(this, type, false);
    if (OT.$.canDefineProperty) {
      Object.defineProperties(this, {
        connections: {
          get: function() {
            if (!sessionConnectedConnectionsDeprecationWarningShown) {
              OT.warn('OT.SessionConnectedEvent no longer includes connections. Listen ' +
                'for connectionCreated events instead.');
              sessionConnectedConnectionsDeprecationWarningShown = true;
            }
            return [];
          }
        },
        streams: {
          get: function() {
            if (!sessionConnectedStreamsDeprecationWarningShown) {
              OT.warn('OT.SessionConnectedEvent no longer includes streams. Listen for ' +
                'streamCreated events instead.');
              sessionConnectedConnectionsDeprecationWarningShown = true;
            }
            return [];
          }
        },
        archives: {
          get: function() {
            if (!sessionConnectedArchivesDeprecationWarningShown) {
              OT.warn('OT.SessionConnectedEvent no longer includes archives. Listen for ' +
                'archiveStarted events instead.');
              sessionConnectedArchivesDeprecationWarningShown = true;
            }
            return [];
          }
        }
      });
    } else {
      this.connections = [];
      this.streams = [];
      this.archives = [];
    }
  };

  /**
   * The Session object dispatches SessionDisconnectEvent object when a session has disconnected.
   * This event may be dispatched asynchronously in response to a successful call to the
   * <code>disconnect()</code> method of the session object.
   *
   *  <h4>
   *    <a href="example"></a>Example
   *  </h4>
   *  <p>
   *    The following code initializes a session and sets up an event listener for when a session is
   * disconnected.
   *  </p>
   * <pre>var apiKey = ""; // Replace with your API key. See https://dashboard.tokbox.com/projects
   *  var sessionID = ""; // Replace with your own session ID.
   *                      // See https://dashboard.tokbox.com/projects
   *  var token = ""; // Replace with a generated token that has been assigned the moderator role.
   *                  // See https://dashboard.tokbox.com/projects
   *
   *  var session = OT.initSession(apiKey, sessionID);
   *  session.on("sessionDisconnected", function(event) {
   *      alert("The session disconnected. " + event.reason);
   *  });
   *  session.connect(token);
   *  </pre>
   *
   * @property {String} reason A description of why the session disconnected.
   *   This property can have two values:
   *  </p>
   *  <ul>
   *    <li>
   *      <code>"clientDisconnected"</code> — A client disconnected from the
   *      session by calling the <code>disconnect()</code> method of the Session
   *      object or by closing the browser. ( See <a href=
   *      "Session.html#disconnect">Session.disconnect()</a>.)
   *    </li>
   *
   *    <li>
   *      <code>"forceDisconnected"</code> — A moderator has disconnected you from
   *      the session by calling the <code>forceDisconnect()</code> method of the
   *      Session object. (See <a href=
   *      "Session.html#forceDisconnect">Session.forceDisconnect()</a>.)
   *    </li>
   *
   *    <li><code>"networkDisconnected"</code> — The network connection terminated
   *    abruptly (for example, the client lost their internet connection).</li>
   *  </ul>
   *  <ul>
   *
   * @class SessionDisconnectEvent
   * @augments Event
   */
  OT.SessionDisconnectEvent = function(type, reason, cancelable) {
    OT.Event.call(this, type, cancelable);
    this.reason = reason;
  };

  /**
  * Prevents the default behavior associated with the event from taking place.
  *
  * <p>
  *   For the <code>sessionDisconnectEvent</code>, the default behavior is that all
  *   Subscriber objects are unsubscribed and removed from the HTML DOM. Each
  *   Subscriber object dispatches a <code>destroyed</code> event when the element
  *   is removed from the HTML DOM. If you call the <code>preventDefault()</code>
  *   method in the event listener for the <code>sessionDisconnect</code> event,
  *   the default behavior is prevented, and you can, optionally, clean up
  *   Subscriber objects using your own code). *
  * </p>
  * <p>
  *   To see whether an event has a default behavior, check the
  *   <code>cancelable</code> property of the event object.
  * </p>*
  * <p>
  *   Call the <code>preventDefault()</code> method in the event listener function
  *   for the event.
  * </p>
  *
  * @method #preventDefault
  * @memberof SessionDisconnectEvent
  */

  /**
   * The Session object dispatches a <code>streamPropertyChanged</code> event in the
   * following circumstances:
   *
   * <ul>
   *   <li> A stream has started or stopped publishing audio or video (see
   *     <a href="Publisher.html#publishAudio">Publisher.publishAudio()</a> and
   *     <a href="Publisher.html#publishVideo">Publisher.publishVideo()</a>).
   *     This change results from a call to the <code>publishAudio()</code> or
   *     <code>publishVideo()</code> methods of the Publish object. Note that a
   *     subscriber's video can be disabled or enabled for reasons other than the
   *     publisher disabling or enabling it. A Subscriber object dispatches
   *     <code>videoDisabled</code> and <code>videoEnabled</code> events in all
   *     conditions that cause the subscriber's stream to be disabled or enabled.
   *   </li>
   *   <li> The <code>videoDimensions</code> property of the Stream object has
   *     changed (see <a href="Stream.html#properties">Stream.videoDimensions</a>).
   *   </li>
   *   <li> The <code>videoType</code> property of the Stream object has changed.
   *     This can happen in a stream published by a mobile device. (See
   *     <a href="Stream.html#properties">Stream.videoType</a>.)
   *   </li>
   * </ul>
   *
   * @class StreamPropertyChangedEvent
   * @property {String} changedProperty The property of the stream that changed. This value
   * is either <code>"hasAudio"</code>, <code>"hasVideo"</code>, or <code>"videoDimensions"</code>.
   * @property {Object} newValue The new value of the property (after the change).
   * @property {Object} oldValue The old value of the property (before the change).
   * @property {Stream} stream The Stream object for which a property has changed.
   *
   * @see <a href="Publisher.html#publishAudio">Publisher.publishAudio()</a></p>
   * @see <a href="Publisher.html#publishVideo">Publisher.publishVideo()</a></p>
   * @see <a href="Stream.html#properties">Stream.videoDimensions</a></p>
   * @augments Event
   */
  OT.StreamPropertyChangedEvent = function(type, stream, changedProperty, oldValue, newValue) {
    OT.Event.call(this, type, false);
    this.type = type;
    this.stream = stream;
    this.changedProperty = changedProperty;
    this.oldValue = oldValue;
    this.newValue = newValue;
  };

  OT.VideoDimensionsChangedEvent = function(target, oldValue, newValue) {
    OT.Event.call(this, 'videoDimensionsChanged', false);
    this.type = 'videoDimensionsChanged';
    this.target = target;
    this.oldValue = oldValue;
    this.newValue = newValue;
  };

  /**
   * Defines event objects for the <code>archiveStarted</code> and <code>archiveStopped</code>
   * events. The Session object dispatches these events when an archive recording of the session
   * starts and stops.
   *
   * @property {String} id The archive ID.
   * @property {String} name The name of the archive. You can assign an archive a name when you
   * create it, using the <a href="http://www.tokbox.com/opentok/api">OpenTok REST API</a> or one
   * of the <a href="http://www.tokbox.com/opentok/libraries/server">OpenTok server SDKs</a>.
   *
   * @class ArchiveEvent
   * @augments Event
   */
  OT.ArchiveEvent = function(type, archive) {
    OT.Event.call(this, type, false);
    this.type = type;
    this.id = archive.id;
    this.name = archive.name;
    this.status = archive.status;
    this.archive = archive;
  };

  OT.ArchiveUpdatedEvent = function(stream, key, oldValue, newValue) {
    OT.Event.call(this, 'updated', false);
    this.target = stream;
    this.changedProperty = key;
    this.oldValue = oldValue;
    this.newValue = newValue;
  };

  /**
   * The Session object dispatches a signal event when the client receives a signal from the
   * session.
   *
   * @class SignalEvent
   * @property {String} type The type assigned to the signal (if there is one). Use the type to
   * filter signals received (by adding an event handler for signal:type1 or signal:type2, etc.)
   * @property {String} data The data string sent with the signal (if there is one).
   * @property {Connection} from The Connection corresponding to the client that sent with the
   * signal.
   *
   * @see <a href="Session.html#signal">Session.signal()</a></p>
   * @see <a href="Session.html#events">Session events (signal and signal:type)</a></p>
   * @augments Event
   */
  OT.SignalEvent = function(type, data, from) {
    OT.Event.call(this, type ? 'signal:' + type : OT.Event.names.SIGNAL, false);
    this.data = data;
    this.from = from;
  };

  OT.StreamUpdatedEvent = function(stream, key, oldValue, newValue) {
    OT.Event.call(this, 'updated', false);
    this.target = stream;
    this.changedProperty = key;
    this.oldValue = oldValue;
    this.newValue = newValue;
  };

  OT.DestroyedEvent = function(type, target, reason) {
    OT.Event.call(this, type, false);
    this.target = target;
    this.reason = reason;
  };

  /**
   * Defines the event object for the <code>videoDisabled</code> and <code>videoEnabled</code>
   * events dispatched by the Subscriber.
   *
   * @class VideoEnabledChangedEvent
   *
   * @property {Boolean} cancelable Whether the event has a default behavior that is cancelable
   * (<code>true</code>) or not (<code>false</code>). You can cancel the default behavior by
   * calling the <code>preventDefault()</code> method of the event object in the callback
   * function. (See <a href="#preventDefault">preventDefault()</a>.)
   *
   * @property {String} reason The reason the video was disabled or enabled. This can be set to one
   * of the following values:
   *
   * <ul>
   *
   *   <li><code>"publishVideo"</code> &mdash; The publisher started or stopped publishing video,
   *   by calling <code>publishVideo(true)</code> or <code>publishVideo(false)</code>.</li>
   *
   *   <li><code>"quality"</code> &mdash; The OpenTok Media Router starts or stops sending video
   *   to the subscriber based on stream quality changes. This feature of the OpenTok Media
   *   Router has a subscriber drop the video stream when connectivity degrades. (The subscriber
   *   continues to receive the audio stream, if there is one.)
   *   <p>
   *   If connectivity improves to support video again, the Subscriber object dispatches
   *   a <code>videoEnabled</code> event, and the Subscriber resumes receiving video.
   *   <p>
   *   By default, the Subscriber displays a video disabled indicator when a
   *   <code>videoDisabled</code> event with this reason is dispatched and removes the indicator
   *   when the <code>videoDisabled</code> event with this reason is dispatched. You can control
   *   the display of this icon by calling the <code>setStyle()</code> method of the Subscriber,
   *   setting the <code>videoDisabledDisplayMode</code> property(or you can set the style when
   *   calling the <code>Session.subscribe()</code> method, setting the <code>style</code> property
   *   of the <code>properties</code> parameter).
   *   <p>
   *   This feature is only available in sessions that use the OpenTok Media Router (sessions with
   *   the <a href="http://tokbox.com/opentok/tutorials/create-session/#media-mode">media mode</a>
   *   set to routed), not in sessions with the media mode set to relayed.
   *   </li>
   *
   *   <li><code>"subscribeToVideo"</code> &mdash; The subscriber started or stopped subscribing to
   *   video, by calling <code>subscribeToVideo(true)</code> or
   *   <code>subscribeToVideo(false)</code>.</li>
   *
   * </ul>
   *
   * @property {Object} target The object that dispatched the event.
   *
   * @property {String} type  The type of event: <code>"videoDisabled"</code> or
   * <code>"videoEnabled"</code>.
   *
   * @see <a href="Subscriber.html#event:videoDisabled">Subscriber videoDisabled event</a></p>
   * @see <a href="Subscriber.html#event:videoEnabled">Subscriber videoEnabled event</a></p>
   * @augments Event
   */
  OT.VideoEnabledChangedEvent = function(type, properties) {
    OT.Event.call(this, type, false);
    this.reason = properties.reason;
  };

  OT.VideoDisableWarningEvent = function(type/*, properties*/) {
    OT.Event.call(this, type, false);
  };

  /**
   * Dispatched periodically by a Subscriber or Publisher object to indicate the audio
   * level. This event is dispatched up to 60 times per second, depending on the browser.
   *
   * @property {String} audioLevel The audio level, from 0 to 1.0. Adjust this value logarithmically
   * for use in adjusting a user interface element, such as a volume meter. Use a moving average
   * to smooth the data.
   *
   * @class AudioLevelUpdatedEvent
   * @augments Event
   */
  OT.AudioLevelUpdatedEvent = function(audioLevel) {
    OT.Event.call(this, OT.Event.names.AUDIO_LEVEL_UPDATED, false);
    this.audioLevel = audioLevel;
  };

  OT.MediaStoppedEvent = function(target) {
    OT.Event.call(this, OT.Event.names.MEDIA_STOPPED, true);
    this.target = target;
  };

})(window);

// tb_require('../../helpers/helpers.js')
// tb_require('../events.js')

var screenSharingExtensionByKind = {},
    screenSharingExtensionClasses = {};

OT.registerScreenSharingExtensionHelper = function(kind, helper) {
  screenSharingExtensionClasses[kind] = helper;
  if (helper.autoRegisters && helper.isSupportedInThisBrowser) {
    OT.registerScreenSharingExtension(kind);
  }
};

/**
 * Register a Chrome extension for screen-sharing support.
 * <p>
 * Use the <code>OT.checkScreenSharingCapability()</code> method to check if an extension is
 * required, registered, and installed.
 * <p>
 * The OpenTok
 * <a href="https://github.com/opentok/screensharing-extensions">screensharing-extensions</a>
 * sample includes code for creating an extension for screen-sharing support.
 *
 * @param {String} kind Set this parameter to <code>"chrome"</code>. Currently, you can only
 * register a screen-sharing extension for Chrome.
 *
 * @see <a href="OT.html#initPublisher">OT.initPublisher()</a>
 * @see <a href="OT.html#checkScreenSharingCapability">OT.checkScreenSharingCapability()</a>
 * @method OT.registerScreenSharingExtension
 * @memberof OT
 */

OT.registerScreenSharingExtension = function(kind) {
  var initArgs = Array.prototype.slice.call(arguments, 1);

  if (screenSharingExtensionClasses[kind] == null) {
    throw new Error('Unsupported kind passed to OT.registerScreenSharingExtension');
  }

  var x = screenSharingExtensionClasses[kind]
          .register.apply(screenSharingExtensionClasses[kind], initArgs);
  screenSharingExtensionByKind[kind] = x;
};

var screenSharingPickHelper = function() {

  var foundClass = OT.$.find(OT.$.keys(screenSharingExtensionClasses), function(cls) {
    return screenSharingExtensionClasses[cls].isSupportedInThisBrowser;
  });

  if (foundClass === void 0) {
    return {};
  }

  return {
    name: foundClass,
    proto: screenSharingExtensionClasses[foundClass],
    instance: screenSharingExtensionByKind[foundClass]
  };

};

OT.pickScreenSharingHelper = function() {
  return screenSharingPickHelper();
};

/**
 * Checks for screen sharing support on the client browser. The object passed to the callback
 * function defines whether screen sharing is supported as well as whether an extension is
 * required, installed, and registered (if needed).
 * <p>
 * <pre>
 * OT.checkScreenSharingCapability(function(response) {
 *   if (!response.supported || response.extensionRegistered === false) {
 *     // This browser does not support screen sharing
 *   } else if (response.extensionInstalled === false) {
 *     // Prompt to install the extension
 *   } else {
 *     // Screen sharing is available.
 *   }
 * });
 * </pre>
 * <p>
 * The OpenTok
 * <a href="https://github.com/opentok/screensharing-extensions">screensharing-extensions</a>
 * sample includes code for creating Chrome and Firefox extensions for screen-sharing support.
 *
 * @param {function} callback The callback invoked with the support options object passed as
 * the parameter. This object has the following properties that indicate support for publishing
 * screen-sharing streams in the client:
 * <p>
 * <ul>
 *   <li>
 *     <code>supported</code> (Boolean) &mdash; Set to true if screen sharing is supported in the
 *     browser. Check the <code>extensionRequired</code> property to see if the browser requires
 *     an extension for screen sharing.
 *   </li>
 *   <li>
 *     <code>supportedSources</code> (Object) &mdash; An object with the following properties:
 *     <code>application</code>, <code>screen</code>, and <code>window</code>. Each property is
 *     a Boolean value indicating support. In Firefox, each of these properties is set to
 *     <code>true</code>. Currently in Chrome, only the <code>screen</code> property is
 *     set to <code>true</code>.
 *   </li>
 * </ul>
 * <p> The options parameter also includes the following properties, which apply to screen-sharing
 * support in Chrome:
 * <ul>
 *   <li>
 *     <code>extensionRequired</code> (String) &mdash; Set to <code>"chrome"</code> in Chrome,
 *     which requires a screen sharing extension to be installed. Otherwise, this property is
 *     undefined.
 *   </li>
 *   <li>
 *     <code>extensionRegistered</code> (Boolean) &mdash; In Chrome, this property is set to
 *     <code>true</code> if a screen-sharing extension is registered; otherwise it is set to
 *     <code>false</code>. If the extension type does not require registration (for example,
 *     in Firefox), this property is set to <code>true</code>. In other browsers (which do not
 *     require an extension), this property is undefined. Use the
 *     <code>OT.registerScreenSharingExtension()</code> method to register an extension in Chrome.
 *   </li>
 *   <li>
 *     <code>extensionInstalled</code> (Boolean) &mdash; If an extension is required, this is set
 *     to <code>true</code> if the extension installed (and registered, if needed); otherwise it
 *     is set to <code>false</code>. If an extension is not required (for example in Firefox),
 *     this property is undefined.
 *   </li>
 * </ul>
 *
 * @see <a href="OT.html#initPublisher">OT.initPublisher()</a>
 * @see <a href="OT.html#registerScreenSharingExtension">OT.registerScreenSharingExtension()</a>
 * @method OT.checkScreenSharingCapability
 * @memberof OT
 */
OT.checkScreenSharingCapability = function(callback) {

  var response = {
    supported: false,
    extensionRequired: void 0,
    extensionRegistered: void 0,
    extensionInstalled: void 0,
    supportedSources: {}
  };

  // find a supported browser

  var helper = screenSharingPickHelper();

  if (helper.name === void 0) {
    setTimeout(callback.bind(null, response));
    return;
  }

  response.supported = true;
  response.extensionRequired = helper.proto.extensionRequired ? helper.name : void 0;

  response.supportedSources = {
    screen: helper.proto.sources.screen,
    application: helper.proto.sources.application,
    window: helper.proto.sources.window,
    browser: helper.proto.sources.browser
  };

  if (!helper.instance) {
    response.extensionRegistered = false;
    if (response.extensionRequired) {
      response.extensionInstalled = false;
    }
    setTimeout(callback.bind(null, response));
    return;
  }

  response.extensionRegistered = response.extensionRequired ? true : void 0;
  helper.instance.isInstalled(function(installed) {
    response.extensionInstalled = response.extensionRequired ? installed : void 0;
    callback(response);
  });
  
};

// tb_require('./register.js')

OT.registerScreenSharingExtensionHelper('firefox', {
  isSupportedInThisBrowser: OT.$.env.name === 'Firefox',
  autoRegisters: true,
  extensionRequired: false,
  getConstraintsShowsPermissionUI: false,
  sources: {
    screen: true,
    application: OT.$.env.name === 'Firefox' && OT.$.env.version >= 34,
    window: OT.$.env.name === 'Firefox' && OT.$.env.version >= 34,
    browser: OT.$.env.name === 'Firefox' && OT.$.env.version >= 38
  },
  register: function() {
    return {
      isInstalled: function(callback) {
        callback(true);
      },
      getConstraints: function(source, constraints, callback) {
        constraints.video = {
          mediaSource: source
        };

        // copy constraints under the video object and removed them from the root constraint object
        if (constraints.browserWindow) {
          constraints.video.browserWindow = constraints.browserWindow;
          delete constraints.browserWindow;
        }
        if (typeof constraints.scrollWithPage !== 'undefined') {
          constraints.video.scrollWithPage = constraints.scrollWithPage;
          delete constraints.scrollWithPage;
        }

        callback(void 0, constraints);
      }
    };
  }
});

OT.registerScreenSharingExtensionHelper('chrome', {
  isSupportedInThisBrowser: !!navigator.webkitGetUserMedia && typeof chrome !== 'undefined',
  autoRegisters: false,
  extensionRequired: true,
  getConstraintsShowsPermissionUI: true,
  sources: {
    screen: true,
    application: false,
    window: false,
    browser: false
  },
  register: function(extensionID) {
    if (!extensionID) {
      throw new Error('initChromeScreenSharingExtensionHelper: extensionID is required.');
    }

    var isChrome = !!navigator.webkitGetUserMedia && typeof chrome !== 'undefined',
        callbackRegistry = {},
        isInstalled = void 0;

    var prefix = 'com.tokbox.screenSharing.' + extensionID;
    var request = function(method, payload) {
      var res = { payload: payload, from: 'jsapi' };
      res[prefix] = method;
      return res;
    };

    var addCallback = function(fn, timeToWait) {
      var requestId = OT.$.uuid(),
          timeout;
      callbackRegistry[requestId] = function() {
        clearTimeout(timeout);
        timeout = null;
        fn.apply(null, arguments);
      };
      if (timeToWait) {
        timeout = setTimeout(function() {
          delete callbackRegistry[requestId];
          fn(new Error('Timeout waiting for response to request.'));
        }, timeToWait);
      }
      return requestId;
    };

    var isAvailable = function(callback) {
      if (!callback) {
        throw new Error('isAvailable: callback is required.');
      }

      if (!isChrome) {
        setTimeout(callback.bind(null, false));
      }

      if (isInstalled !== void 0) {
        setTimeout(callback.bind(null, isInstalled));
      } else {
        var requestId = addCallback(function(error, event) {
          if (isInstalled !== true) {
            isInstalled = (event === 'extensionLoaded');
          }
          callback(isInstalled);
        }, 2000);
        var post = request('isExtensionInstalled', { requestId: requestId });
        window.postMessage(post, '*');
      }
    };

    var getConstraints = function(source, constraints, callback) {
      if (!callback) {
        throw new Error('getSourceId: callback is required');
      }
      isAvailable(function(isInstalled) {
        if (isInstalled) {
          var requestId = addCallback(function(error, event, payload) {
            if (event === 'permissionDenied') {
              callback(new Error('PermissionDeniedError'));
            } else {
              if (!constraints.video) {
                constraints.video = {};
              }
              if (!constraints.video.mandatory) {
                constraints.video.mandatory = {};
              }
              constraints.video.mandatory.chromeMediaSource = 'desktop';
              constraints.video.mandatory.chromeMediaSourceId = payload.sourceId;
              callback(void 0, constraints);
            }
          });
          window.postMessage(request('getSourceId', { requestId: requestId, source: source }), '*');
        } else {
          callback(new Error('Extension is not installed'));
        }
      });
    };

    window.addEventListener('message', function(event) {

      if (event.origin !== window.location.origin) {
        return;
      }

      if (!(event.data != null && typeof event.data === 'object')) {
        return;
      }

      if (event.data.from !== 'extension') {
        return;
      }

      var method = event.data[prefix],
          payload = event.data.payload;

      if (payload && payload.requestId) {
        var callback = callbackRegistry[payload.requestId];
        delete callbackRegistry[payload.requestId];
        if (callback) {
          callback(null, method, payload);
        }
      }

      if (method === 'extensionLoaded') {
        isInstalled = true;
      }
    });

    return {
      isInstalled: isAvailable,
      getConstraints: getConstraints
    };
  }
});

// tb_require('../helpers/helpers.js')
// tb_require('../helpers/lib/properties.js')
// tb_require('../helpers/lib/video_element.js')
// tb_require('./events.js')

// id: String                           | mandatory | immutable
// type: String {video/audio/data/...}  | mandatory | immutable
// active: Boolean                      | mandatory | mutable
// orientation: Integer?                | optional  | mutable
// frameRate: Float                     | optional  | mutable
// height: Integer                      | optional  | mutable
// width: Integer                       | optional  | mutable
// maxFrameRate: Float                  | optional  | mutable
// maxHeight: Integer                   | optional  | mutable
// maxWidth: Integer                    | optional  | mutable
//
OT.StreamChannel = function(options) {
  this.id = options.id;
  this.type = options.type;
  this.active = OT.$.castToBoolean(options.active);
  this.orientation = options.orientation || OT.VideoOrientation.ROTATED_NORMAL;
  if (options.frameRate) this.frameRate = parseFloat(options.frameRate);
  if (options.maxFrameRate) this.maxFrameRate = parseFloat(options.maxFrameRate);
  if (options.maxWidth) this.maxWidth = parseInt(options.maxWidth, 10);
  if (options.maxHeight) this.maxHeight = parseInt(options.maxHeight, 10);
  this.width = parseInt(options.width, 10);
  this.height = parseInt(options.height, 10);

  // The defaults are used for incoming streams from pre 2015Q1 release clients.
  this.source = options.source || 'camera';
  this.fitMode = options.fitMode || 'cover';

  OT.$.eventing(this, true);

  // Returns true if a property was updated.
  this.update = function(attributes) {
    var videoDimensions = {},
        oldVideoDimensions = {};

    for (var key in attributes) {
      if (!attributes.hasOwnProperty(key)) {
        continue;
      }

      // we shouldn't really read this before we know the key is valid
      var oldValue = this[key];

      switch (key) {
        case 'active':
          this.active = OT.$.castToBoolean(attributes[key]);
          break;

        case 'disableWarning':
          this.disableWarning = OT.$.castToBoolean(attributes[key]);
          break;

        case 'frameRate':
          this.frameRate = parseFloat(attributes[key], 10);
          break;

        case 'width':
        case 'height':
          this[key] = parseInt(attributes[key], 10);

          videoDimensions[key] = this[key];
          oldVideoDimensions[key] = oldValue;
          break;

        case 'orientation':
          this[key] = attributes[key];

          videoDimensions[key] = this[key];
          oldVideoDimensions[key] = oldValue;
          break;

        case 'fitMode':
          this[key] = attributes[key];
          break;

        case 'source':
          this[key] = attributes[key];
          break;

        default:
          OT.warn('Tried to update unknown key ' + key + ' on ' + this.type +
            ' channel ' + this.id);
          return;
      }

      this.trigger('update', this, key, oldValue, this[key]);
    }

    if (OT.$.keys(videoDimensions).length) {
      // To make things easier for the public API, we broadcast videoDimensions changes,
      // which is an aggregate of width, height, and orientation changes.
      this.trigger('update', this, 'videoDimensions', oldVideoDimensions, videoDimensions);
    }

    return true;
  };
};

// tb_require('../helpers/helpers.js')
// tb_require('../helpers/lib/properties.js')
// tb_require('./events.js')
// tb_require('./stream_channel.js')

!(function() {

  var validPropertyNames = ['name', 'archiving'];

  /**
   * Specifies a stream. A stream is a representation of a published stream in a session. When a
   * client calls the <a href="Session.html#publish">Session.publish() method</a>, a new stream is
   * created. Properties of the Stream object provide information about the stream.
   *
   *  <p>When a stream is added to a session, the Session object dispatches a
   * <code>streamCreatedEvent</code>. When a stream is destroyed, the Session object dispatches a
   * <code>streamDestroyed</code> event. The StreamEvent object, which defines these event objects,
   * has a <code>stream</code> property, which is an array of Stream object. For details and a code
   * example, see {@link StreamEvent}.</p>
   *
   *  <p>When a connection to a session is made, the Session object dispatches a
   * <code>sessionConnected</code> event, defined by the SessionConnectEvent object. The
   * SessionConnectEvent object has a <code>streams</code> property, which is an array of Stream
   * objects pertaining to the streams in the session at that time. For details and a code example,
   * see {@link SessionConnectEvent}.</p>
   *
   * @class Stream
   * @property {Connection} connection The Connection object corresponding
   * to the connection that is publishing the stream. You can compare this to to the
   * <code>connection</code> property of the Session object to see if the stream is being published
   * by the local web page.
   *
   * @property {Number} creationTime The timestamp for the creation
   * of the stream. This value is calculated in milliseconds. You can convert this value to a
   * Date object by calling <code>new Date(creationTime)</code>, where <code>creationTime</code> is
   * the <code>creationTime</code> property of the Stream object.
   *
   * @property {Number} frameRate The frame rate of the video stream. This property is only set if
   * the publisher of the stream specifies a frame rate when calling the
   * <code>OT.initPublisher()</code> method; otherwise, this property is undefined.
   *
   * @property {Boolean} hasAudio Whether the stream has audio. This property can change if the
   * publisher turns on or off audio (by calling
   * <a href="Publisher.html#publishAudio">Publisher.publishAudio()</a>). When this occurs, the
   * {@link Session} object dispatches a <code>streamPropertyChanged</code> event (see
   * {@link StreamPropertyChangedEvent}).
   *
   * @property {Boolean} hasVideo Whether the stream has video. This property can change if the
   * publisher turns on or off video (by calling
   * <a href="Publisher.html#publishVideo">Publisher.publishVideo()</a>). When this occurs, the
   * {@link Session} object dispatches a <code>streamPropertyChanged</code> event (see
   * {@link StreamPropertyChangedEvent}).
   *
   * @property {String} name The name of the stream. Publishers can specify a name when publishing
   * a stream (using the <code>publish()</code> method of the publisher's Session object).
   *
   * @property {String} streamId The unique ID of the stream.
   *
   * @property {Object} videoDimensions This object has two properties: <code>width</code> and
   * <code>height</code>. Both are numbers. The <code>width</code> property is the width of the
   * encoded stream; the <code>height</code> property is the height of the encoded stream. (These
   * are independent of the actual width of Publisher and Subscriber objects corresponding to the
   * stream.) This property can change if a stream published from a mobile device resizes, based on
   * a change in the device orientation. When the video dimensions change,
   * the {@link Session} object dispatches a <code>streamPropertyChanged</code> event
   * (see {@link StreamPropertyChangedEvent}).
   *
   * @property {String} videoType The type of video &mdash; either <code>"camera"</code> or
   * <code>"screen"</code>. A <code>"screen"</code> video uses screen sharing on the publisher
   * as the video source; for other videos, this property is set to <code>"camera"</code>.
   * This property can change if a stream published from a mobile device changes from a
   * camera to a screen-sharing video type. When the video type changes, the {@link Session} object
   * dispatches a <code>streamPropertyChanged</code> event (see {@link StreamPropertyChangedEvent}).
   */

  OT.Stream = function(id, name, creationTime, connection, session, channel) {
    var destroyedReason;

    this.id = this.streamId = id;
    this.name = name;
    this.creationTime = Number(creationTime);

    this.connection = connection;
    this.channel = channel;
    this.publisher = OT.publishers.find({streamId: this.id});

    OT.$.eventing(this);

    var onChannelUpdate = OT.$.bind(function(channel, key, oldValue, newValue) {
      var _key = key;

      switch (_key) {
        case 'active':
          _key = channel.type === 'audio' ? 'hasAudio' : 'hasVideo';
          this[_key] = newValue;
          break;

        case 'disableWarning':
          _key = channel.type === 'audio' ? 'audioDisableWarning' : 'videoDisableWarning';
          this[_key] = newValue;
          if (!this[channel.type === 'audio' ? 'hasAudio' : 'hasVideo']) {
            return; // Do NOT event in this case.
          }
          break;

        case 'fitMode':
          _key = 'defaultFitMode';
          this[_key] = newValue;
          break;

        case 'source':
          _key = channel.type === 'audio' ? 'audioType' : 'videoType';
          this[_key] = newValue;
          break;

        case 'videoDimensions':
          this.videoDimensions = newValue;
          break;

        case 'orientation':
        case 'width':
        case 'height':
          // We dispatch this via the videoDimensions key instead so do not
          // trigger an event for them.
          return;
      }

      this.dispatchEvent(new OT.StreamUpdatedEvent(this, _key, oldValue, newValue));
    }, this);

    var associatedWidget = OT.$.bind(function() {
      if (this.publisher) {
        return this.publisher;
      } else {
        return OT.subscribers.find(function(subscriber) {
          return subscriber.stream && subscriber.stream.id === this.id &&
            subscriber.session.id === session.id;
        }, this);
      }
    }, this);

    // Returns true if this stream is subscribe to.
    var isBeingSubscribedTo = OT.$.bind(function() {
      // @fixme This is not strictly speaking the right test as a stream
      // can be published and subscribed by the same connection. But the
      // update features don't handle this case properly right now anyway.
      //
      // The issue is that the stream needs to know whether the stream is
      // 'owned' by a publisher or a subscriber. The reason for that is that
      // when a Publisher updates a stream channel then we need to send the
      // `streamChannelUpdate` message, whereas if a Subscriber does then we
      // need to send `subscriberChannelUpdate`. The current code will always
      // send `streamChannelUpdate`.
      return !this.publisher;
    }, this);

    // Returns all channels that have a type of +type+.
    this.getChannelsOfType = function(type) {
      return OT.$.filter(this.channel, function(channel) {
        return channel.type === type;
      });
    };

    this.getChannel = function(id) {
      for (var i = 0; i < this.channel.length; ++i) {
        if (this.channel[i].id === id) return this.channel[i];
      }

      return null;
    };

    //// implement the following using the channels
    // hasAudio
    // hasVideo
    // videoDimensions

    var audioChannel = this.getChannelsOfType('audio')[0],
        videoChannel = this.getChannelsOfType('video')[0];

    // @todo this should really be: "has at least one video/audio track" instead of
    // "the first video/audio track"
    this.hasAudio = audioChannel != null && audioChannel.active;
    this.hasVideo = videoChannel != null && videoChannel.active;

    this.videoType = videoChannel && videoChannel.source;
    this.defaultFitMode = videoChannel && videoChannel.fitMode;

    this.videoDimensions = {};
    if (videoChannel) {
      this.videoDimensions.width = videoChannel.width;
      this.videoDimensions.height = videoChannel.height;
      this.videoDimensions.orientation = videoChannel.orientation;

      videoChannel.on('update', onChannelUpdate);
      this.frameRate = videoChannel.frameRate;
    }

    if (audioChannel) {
      audioChannel.on('update', onChannelUpdate);
    }

    this.setChannelActiveState = function(channelType, activeState, activeReason) {
      var attributes = {
        active: activeState
      };
      if (activeReason) {
        attributes.activeReason = activeReason;
      }
      updateChannelsOfType(channelType, attributes);
    };

    this.setVideoDimensions = function(width, height) {
      updateChannelsOfType('video', {
        width: width,
        height: height,
        orientation: 0
      });
    };

    this.setRestrictFrameRate = function(restrict) {
      updateChannelsOfType('video', {
        restrictFrameRate: restrict
      });
    };

    this.setPreferredResolution = function(resolution) {
      if (!isBeingSubscribedTo()) {
        OT.warn('setPreferredResolution has no affect when called by a publisher');
        return;
      }

      if (session.sessionInfo.p2pEnabled) {
        OT.warn('Stream.setPreferredResolution will not work in a P2P Session');
        return;
      }

      if (resolution &&
          resolution.width === void 0 &&
          resolution.height === void 0) {
        return;
      }

      // This duplicates some of the code in updateChannelsOfType. We do this for a
      // couple of reasons:
      //   1. Because most of the work that updateChannelsOfType does is in calling
      //      getChannelsOfType, which we need to do here anyway so that we can update
      //      the value of maxResolution in the Video Channel.
      //   2. updateChannelsOfType on only sends a message to update the channel in
      //      Rumor. The client then expects to receive a subsequent channel update
      //      indicating that the update was successful. We don't receive those updates
      //      for maxFrameRate/maxResolution so we need to complete both tasks and it's
      //      neater to do the related tasks right next to each other.
      //   3. This code shouldn't be in OT.Stream anyway. There is way too much coupling
      //      between Stream, Session, Publisher, and Subscriber. This will eventually be
      //      fixed, and when it is then it will be easier to exact the code if it's a
      //      single piece.
      //
      var video = this.getChannelsOfType('video')[0];
      if (!video) {
        return;
      }

      if (resolution && resolution.width) {
        if (isNaN(parseInt(resolution.width, 10))) {
          throw new OT.$.Error('stream preferred width must be an integer', 'Subscriber');
        }

        video.maxWidth = parseInt(resolution.width, 10);
      } else {
        video.maxWidth = void 0;
      }

      if (resolution && resolution.height) {
        if (isNaN(parseInt(resolution.height, 10))) {
          throw new OT.$.Error('stream preferred height must be an integer', 'Subscriber');
        }

        video.maxHeight = parseInt(resolution.height, 10);
      } else {
        video.maxHeight = void 0;
      }

      session._.subscriberChannelUpdate(this, associatedWidget(), video, {
        maxWidth: video.maxWidth || 0,
        maxHeight: video.maxHeight || 0
      });
    };

    this.getPreferredResolution = function() {
      var videoChannel = this.getChannelsOfType('video')[0];
      if (!videoChannel || (!videoChannel.maxWidth && !videoChannel.maxHeight)) {
        return void 0;
      }

      return {
        width: videoChannel.maxWidth,
        height: videoChannel.maxHeight
      };
    };

    this.setPreferredFrameRate = function(maxFrameRate) {
      if (!isBeingSubscribedTo()) {
        OT.warn('setPreferredFrameRate has no affect when called by a publisher');
        return;
      }

      if (session.sessionInfo.p2pEnabled) {
        OT.warn('Stream.setPreferredFrameRate will not work in a P2P Session');
        return;
      }

      if (maxFrameRate && isNaN(parseFloat(maxFrameRate))) {
        throw new OT.$.Error('stream preferred frameRate must be a number', 'Subscriber');
      }

      // This duplicates some of the code in updateChannelsOfType. We do this for a
      // couple of reasons:
      //   1. Because most of the work that updateChannelsOfType does is in calling
      //      getChannelsOfType, which we need to do here anyway so that we can update
      //      the value of maxFrameRate in the Video Channel.
      //   2. updateChannelsOfType on only sends a message to update the channel in
      //      Rumor. The client then expects to receive a subsequent channel update
      //      indicating that the update was successful. We don't receive those updates
      //      for maxFrameRate/maxResolution so we need to complete both tasks and it's
      //      neater to do the related tasks right next to each other.
      //   3. This code shouldn't be in OT.Stream anyway. There is way too much coupling
      //      between Stream, Session, Publisher, and Subscriber. This will eventually be
      //      fixed, and when it is then it will be easier to exact the code if it's a
      //      single piece.
      //
      var video = this.getChannelsOfType('video')[0];

      if (video) {
        video.maxFrameRate = maxFrameRate ? parseFloat(maxFrameRate) : void 0;

        session._.subscriberChannelUpdate(this, associatedWidget(), video, {
          maxFrameRate: video.maxFrameRate || 0
        });
      }
    };

    this.getPreferredFrameRate = function() {
      var videoChannel = this.getChannelsOfType('video')[0];
      return videoChannel ? videoChannel.maxFrameRate : void 0;
    };

    var updateChannelsOfType = OT.$.bind(function(channelType, attributes) {
      var setChannelActiveState;
      if (!this.publisher) {
        var subscriber = associatedWidget();

        setChannelActiveState = function(channel) {
          session._.subscriberChannelUpdate(this, subscriber, channel, attributes);
        };
      } else {
        setChannelActiveState = function(channel) {
          session._.streamChannelUpdate(this, channel, attributes);
        };
      }

      OT.$.forEach(this.getChannelsOfType(channelType), OT.$.bind(setChannelActiveState, this));
    }, this);

    this.destroyed = false;
    this.destroyedReason = void 0;

    this.destroy = function(reason, quiet) {
      destroyedReason = reason || 'clientDisconnected';
      this.destroyed = true;
      this.destroyedReason = destroyedReason;

      if (quiet !== true) {
        this.dispatchEvent(
          new OT.DestroyedEvent(
            'destroyed',      // This should be OT.Event.names.STREAM_DESTROYED, but
                              // the value of that is currently shared with Session
            this,
            destroyedReason
          )
        );
      }
    };

    /// PRIVATE STUFF CALLED BY Raptor.Dispatcher

    // Confusingly, this should not be called when you want to change
    // the stream properties. This is used by Raptor dispatch to notify
    // the stream that it's properies have been successfully updated
    //
    // @todo make this sane. Perhaps use setters for the properties that can
    // send the appropriate Raptor message. This would require that Streams
    // have access to their session.
    //
    this._ = {};
    this._.updateProperty = OT.$.bind(function(key, value) {
      if (OT.$.arrayIndexOf(validPropertyNames, key) === -1) {
        OT.warn('Unknown stream property "' + key + '" was modified to "' + value + '".');
        return;
      }

      var oldValue = this[key],
          newValue = value;

      switch (key) {
        case 'name':
          this[key] = newValue;
          break;

        case 'archiving':
          var widget = associatedWidget();
          if (widget) {
            widget._.archivingStatus(newValue);
          }
          this[key] = newValue;
          break;
      }

      var event = new OT.StreamUpdatedEvent(this, key, oldValue, newValue);
      this.dispatchEvent(event);
    }, this);

    // Mass update, called by Raptor.Dispatcher
    this._.update = OT.$.bind(function(attributes) {
      for (var key in attributes) {
        if (!attributes.hasOwnProperty(key)) {
          continue;
        }
        this._.updateProperty(key, attributes[key]);
      }
    }, this);

    this._.updateChannel = OT.$.bind(function(channelId, attributes) {
      this.getChannel(channelId).update(attributes);
    }, this);
  };

})(window);

// tb_require('../helpers/helpers.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

/*
 * Executes the provided callback thanks to <code>window.setInterval</code>.
 *
 * @param {function()} callback
 * @param {number} frequency how many times per second we want to execute the callback
 * @constructor
 */
OT.IntervalRunner = function(callback, frequency) {
  var _callback = callback,
    _frequency = frequency,
    _intervalId = null;

  this.start = function() {
    _intervalId = window.setInterval(_callback, 1000 / _frequency);
  };

  this.stop = function() {
    window.clearInterval(_intervalId);
    _intervalId = null;
  };
};

// tb_require('../helpers/helpers.js')
// tb_require('../helpers/lib/properties.js')

!(function() {
  /* jshint globalstrict: true, strict: false, undef: true, unused: true,
            trailing: true, browser: true, smarttabs:true */
  /* global OT */

  /**
   * The Error class is used to define the error object passed into completion handlers.
   * Each of the following methods, which execute asynchronously, includes a
   * <code>completionHandler</code> parameter:
   *
   * <ul>
   *   <li><a href="Session.html#connect">Session.connect()</a></li>
   *   <li><a href="Session.html#forceDisconnect">Session.forceDisconnect()</a></li>
   *   <li><a href="Session.html#forceUnpublish">Session.forceUnpublish()</a></li>
   *   <li><a href="Session.html#publish">Session.publish()</a></li>
   *   <li><a href="Session.html#subscribe">Session.subscribe()</a></li>
   *   <li><a href="OT.html#initPublisher">OT.initPublisher()</a></li>
   *   <li><a href="OT.html#reportIssue">OT.reportIssue()</a></li>
   * </ul>
   *
   * <p>
   * The <code>completionHandler</code> parameter is a function that is called when the call to
   * the asynchronous method succeeds or fails. If the asynchronous call fails, the completion
   * handler function is passes an error object (defined by the Error class). The <code>code</code>
   * and <code>message</code> properties of the error object provide details about the error.
   *
   * @property {Number} code The error code, defining the error.
   *
   *   <p>
   *   In the event of an error, the <code>code</code> value of the <code>error</code> parameter can
   *   have one of the following values:
   * </p>
   *
   * <p>Errors when calling <code>Session.connect()</code>:</p>
   *
   * <table class="docs_table"><tbody>
   * <tr>
   *   <td><b><code>code</code></b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *   <td>1004</td>
   *   <td>Authentication error. Check the error message for details. This error can result if you
   *     in an expired token when trying to connect to a session. It can also occur if you pass
   *     in an invalid token or API key. Make sure that you are generating the token using the
   *     current version of one of the
   *     <a href="http://tokbox.com/opentok/libraries/server">OpenTok server SDKs</a>.</td>
   * </tr>
   * <tr>
   *   <td>1005</td>
   *   <td>Invalid Session ID. Make sure you generate the session ID using the current version of
   *     one of the <a href="http://tokbox.com/opentok/libraries/server">OpenTok server
   *     SDKs</a>.</td>
   * </tr>
   * <tr>
   *   <td>1006</td>
   *   <td>Connect Failed. Unable to connect to the session. You may want to have the client check
   *     the network connection.</td>
   * </tr>
   * <tr>
   *  <td>1026</td>
   *  <td>Terms of service violation: export compliance. See the
   *    <a href="https://tokbox.com/support/tos">Terms of Service</a>.</td>
   * </tr>
   * <tr>
   *   <td>2001</td>
   *   <td>Connect Failed. Unexpected response from the OpenTok server. Try connecting again
   *     later.</td>
   * </tr>
   * </tbody></table>
   *
   * <p>Errors when calling <code>Session.forceDisconnect()</code>:</p>
   *
   * <table class="docs_table"><tbody>
   * <tr>
   *   <td><b>
   *       <code>code</code>
   *     </b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *   <td>1010</td>
   *   <td>The client is not connected to the OpenTok session. Check that client connects
   *     successfully and has not disconnected before calling forceDisconnect().</td>
   * </tr>
   * <tr>
   *   <td>1520</td>
   *   <td>Unable to force disconnect. The client's token does not have the role set to moderator.
   *     Once the client has connected to the session, the <code>capabilities</code> property of
   *     the Session object lists the client's capabilities.</td>
   * </tr>
   * </tbody></table>
   *
   * <p>Errors when calling <code>Session.forceUnpublish()</code>:</p>
   *
   * <table class="docs_table"><tbody>
   * <tr>
   *   <td><b><code>code</code></b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *   <td>1010</td>
   *   <td>The client is not connected to the OpenTok session. Check that client connects
   *     successfully and has not disconnected before calling forceUnpublish().</td>
   * </tr>
   * <tr>
   *   <td>1530</td>
   *   <td>Unable to force unpublish. The client's token does not have the role set to moderator.
   *     Once the client has connected to the session, the <code>capabilities</code> property of
   *     the Session object lists the client's capabilities.</td>
   * </tr>
   * <tr>
   *   <td>1535</td>
   *   <td>Force Unpublish on an invalid stream. Make sure that the stream has not left the
   *     session before you call the <code>forceUnpublish()</code> method.</td>
   * </tr>
   * </tbody></table>
   *
   * <p>Errors when calling <code>Session.publish()</code>:</p>
   *
   * <table class="docs_table"><tbody>
   * <tr>
   *   <td><b><code>code</code></b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *   <td>1010</td>
   *   <td>The client is not connected to the OpenTok session. Check that the client connects
   *     successfully before trying to publish. And check that the client has not disconnected
   *     before trying to publish.</td>
   * </tr>
   * <tr>
   *   <td>1500</td>
   *   <td>Unable to Publish. The client's token does not have the role set to to publish or
   *     moderator. Once the client has connected to the session, the <code>capabilities</code>
   *     property of the Session object lists the client's capabilities.</td>
   * </tr>
   * <tr>
   *   <td>1553</td>
   *   <td>WebRTC ICE workflow error. Try publishing again or reconnecting to the session.</td>
   * </tr>
   * <tr>
   *   <td>1601</td>
   *   <td>Internal error -- WebRTC publisher error. Try republishing or reconnecting to the
   *     session.</td>
   * </tr>
   * <tr>
   *   <td>2001</td>
   *   <td>Publish Failed. Unexpected response from the OpenTok server. Try publishing again
   *     later.</td>
   * </tr>
   * </tbody></table>
   *
   * <p>Errors when calling <code>Session.signal()</code>:</p>
   *
   * <table class="docs_table" style="width:100%"><tbody>
   * <tr>
   *   <td><b><code>code</code></b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *     <td>400</td>
   *     <td>One of the signal properties &mdash; data, type, or to &mdash;
   *                 is invalid. Or the data cannot be parsed as JSON.</td>
   * </tr>
   * <tr>
   *   <td>404</td> <td>The to connection does not exist.</td>
   * </tr>
   * <tr>
   *   <td>413</td> <td>The type string exceeds the maximum length (128 bytes),
   *     or the data string exceeds the maximum size (8 kB).</td>
   * </tr>
   * <tr>
   *   <td>500</td>
   *   <td>The client is not connected to the OpenTok session. Check that the client connects
   *     successfully before trying to signal. And check that the client has not disconnected before
   *     trying to publish.</td>
   * </tr>
   * <tr>
   *   <td>2001</td>
   *   <td>Signal Failed. Unexpected response from the OpenTok server. Try sending the signal again
   *     later.</td>
   * </tr>
   * </tbody></table>
   *
   * <p>Errors when calling <code>Session.subscribe()</code>:</p>
   *
   * <table class="docs_table" style="width:100%"><tbody>
   * <tr>
   *   <td><b>
   *       <code>code</code>
   *     </b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *   <td>1013</td>
   *   <td>WebRTC PeerConnection error. Try resubscribing to the stream or
   *     reconnecting to the session.</td>
   * </tr>
   * <tr>
   *   <td>1554</td>
   *   <td>WebRTC ICE workflow error. Try resubscribing to the stream or
   *     reconnecting to the session.</td>
   * </tr>
   * <tr>
   *   <td>1600</td>
   *   <td>Internal error -- WebRTC subscriber error. Try resubscribing to the stream or
   *     reconnecting to the session.</td>
   * </tr>
   * <tr>
   *   <td>2001</td>
   *   <td>Subscribe Failed. Unexpected response from the OpenTok server. Try subscribing again
   *     later.</td>
   * </tr>
   * </tbody></table>
   *
   * <p>Errors when calling <code>OT.initPublisher()</code>:</p>
   *
   * <table class="docs_table"><tbody>
   * <tr>
   *   <td><b><code>code</code></b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *   <td>1004</td>
   *   <td>Authentication error. Check the error message for details. This error can result if you
   *     pass in an expired token when trying to connect to a session. It can also occur if you
   *     pass in an invalid token or API key. Make sure that you are generating the token using
   *     the current version of one of the
   *     <a href="http://tokbox.com/opentok/libraries/server">OpenTok server SDKs</a>.</td>
   * </tr>
   * <tr>
   *   <td>1550</td>
   *   <td>Screen sharing is not supported (and you set the <code>videoSource</code> property
   *     of the <code>options</code> parameter of <code>OT.initPublisher()</code> to
   *     <code>"screen"</code>). Before calling <code>OT.initPublisher()</code>, you can call
   *     <a href="OT.html#checkScreenSharingCapability">OT.checkScreenSharingCapability()</a>
   *     to check if screen sharing is supported.</td>
   * </tr>
   * <tr>
   *   <td>1551</td>
   *   <td>A screen sharing extension needs to be registered but it is not. This error can occur
   *     when you set the <code>videoSource</code> property of the <code>options</code> parameter
   *     of <code>OT.initPublisher()</code> to <code>"screen"</code>. Before calling
   *     <code>OT.initPublisher()</code>, you can call
   *     <a href="OT.html#checkScreenSharingCapability">OT.checkScreenSharingCapability()</a>
   *     to check if screen sharing requires an extension to be registered.</td>
   * </tr>
   * <tr>
   *   <td>1552</td>
   *   <td>A screen sharing extension is required, but it is not installed. This error can occur
   *     when you set the <code>videoSource</code> property of the <code>options</code> parameter
   *     of <code>OT.initPublisher()</code> to <code>"screen"</code>. Before calling
   *     <code>OT.initPublisher()</code>, you can call
   *     <a href="OT.html#checkScreenSharingCapability">OT.checkScreenSharingCapability()</a>
   *     to check if screen sharing requires an extension to be installed.</td>
   * </tr>
   * </tbody></table>
   *
   * <p>Errors when calling <code>OT.initPublisher()</code>:</p>
   *
   * <table class="docs_table"><tbody>
   * <tr>
   *   <td><b><code>code</code></b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *   <td>2011</td>
   *   <td>Error calling OT.reportIssue(). Check the client's network connection.</td>
   * </tr>
   * </tbody></table>
   *
   * <p>General errors that can occur when calling any method:</p>
   *
   * <table class="docs_table" style="width:100%"><tbody>
   * <tr>
   *   <td><b><code>code</code></b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *   <td>1011</td>
   *   <td>Invalid Parameter. Check that you have passed valid parameter values into the method
   *     call.</td>
   * </tr>
   * <tr>
   *   <td>2000</td>
   *   <td>Internal Error. Try reconnecting to the OpenTok session and trying the action again.</td>
   * </tr>
   * </tbody></table>
   *
   * @property {String} message The message string provides details about the error.
   *
   * @class Error
   * @augments Event
   */
  OT.Error = function(code, message) {
    this.code = code;
    this.message = message;
  };

  var errorsCodesToTitle = {
    1004: 'Authentication error',
    1005: 'Invalid Session ID',
    1006: 'Connect Failed',
    1007: 'Connect Rejected',
    1008: 'Connect Time-out',
    1009: 'Security Error',
    1010: 'Not Connected',
    1011: 'Invalid Parameter',
    1012: 'Peer-to-peer Stream Play Failed',
    1013: 'Connection Failed',
    1014: 'API Response Failure',
    1015: 'Session connected, cannot test network',
    1021: 'Request Timeout',
    1026: 'Terms of Service Violation: Export Compliance',
    1500: 'Unable to Publish',
    1503: 'No TURN server found',
    1520: 'Unable to Force Disconnect',
    1530: 'Unable to Force Unpublish',
    1553: 'ICEWorkflow failed',
    1600: 'createOffer, createAnswer, setLocalDescription, setRemoteDescription',
    2000: 'Internal Error',
    2001: 'Unexpected Server Response',
    4000: 'WebSocket Connection Failed',
    4001: 'WebSocket Network Disconnected'
  };

  function _exceptionHandler(component, msg, errorCode, context) {
    var title = OT.getErrorTitleByCode(errorCode),
        contextCopy = context ? OT.$.clone(context) : {};

    OT.error('OT.exception :: title: ' + title + ' (' + errorCode + ') msg: ' + msg);

    if (!contextCopy.partnerId) contextCopy.partnerId = OT.APIKEY;

    try {
      OT.analytics.logError(errorCode, 'tb.exception', title, {details:msg}, contextCopy);

      OT.dispatchEvent(
        new OT.ExceptionEvent(OT.Event.names.EXCEPTION, msg, title, errorCode, component, component)
      );
    } catch (err) {
      OT.error('OT.exception :: Failed to dispatch exception - ' + err.toString());
      // Don't throw an error because this is asynchronous
      // don't do an exceptionHandler because that would be recursive
    }
  }

  /**
   * Get the title of an error by error code
   *
   * @property {Number|String} code The error code to lookup
   * @return {String} The title of the message with that code
   *
   * @example
   *
   * OT.getErrorTitleByCode(1006) === 'Connect Failed'
   */
  OT.getErrorTitleByCode = function(code) {
    return errorsCodesToTitle[+code];
  };

  // @todo redo this when we have time to tidy up
  //
  // @example
  //
  //  OT.handleJsException("Descriptive error message", 2000, {
  //      session: session,
  //      target: stream|publisher|subscriber|session|etc
  //  });
  //
  OT.handleJsException = function(errorMsg, code, options) {
    options = options || {};

    var context,
        session = options.session;

    if (session) {
      context = {
        sessionId: session.sessionId
      };

      if (session.isConnected()) context.connectionId = session.connection.connectionId;
      if (!options.target) options.target = session;

    } else if (options.sessionId) {
      context = {
        sessionId: options.sessionId
      };

      if (!options.target) options.target = null;
    }

    _exceptionHandler(options.target, errorMsg, code, context);
  };

})(window);

// tb_require('../helpers/helpers.js')
// tb_require('../helpers/lib/config.js')
// tb_require('./events.js')

!(function() {

  /* jshint globalstrict: true, strict: false, undef: true, unused: true,
            trailing: true, browser: true, smarttabs:true */
  /* global OT */

  // Helper to synchronise several startup tasks and then dispatch a unified
  // 'envLoaded' event.
  //
  // This depends on:
  // * OT
  // * OT.Config
  //
  function EnvironmentLoader() {
    var _configReady = false,

        // If the plugin is installed, then we should wait for it to
        // be ready as well.
        _pluginSupported = OTPlugin.isSupported(),
        _pluginLoadAttemptComplete = _pluginSupported ? OTPlugin.isReady() : true,

        isReady = function() {
          return !OT.$.isDOMUnloaded() && OT.$.isReady() &&
                      _configReady && _pluginLoadAttemptComplete;
        },

        onLoaded = function() {
          if (isReady()) {
            OT.dispatchEvent(new OT.EnvLoadedEvent(OT.Event.names.ENV_LOADED));
          }
        },

        onDomReady = function() {
          OT.$.onDOMUnload(onDomUnload);

          // The Dynamic Config won't load until the DOM is ready
          OT.Config.load(OT.properties.configURL);

          onLoaded();
        },

        onDomUnload = function() {
          OT.dispatchEvent(new OT.EnvLoadedEvent(OT.Event.names.ENV_UNLOADED));
        },

        onPluginReady = function(err) {
          // We mark the plugin as ready so as not to stall the environment
          // loader. In this case though, OTPlugin is not supported.
          _pluginLoadAttemptComplete = true;

          if (err) {
            OT.debug('TB Plugin failed to load or was not installed');
          }

          onLoaded();
        },

        configLoaded = function() {
          _configReady = true;
          OT.Config.off('dynamicConfigChanged', configLoaded);
          OT.Config.off('dynamicConfigLoadFailed', configLoadFailed);

          onLoaded();
        },

        configLoadFailed = function() {
          configLoaded();
        };

    OT.Config.on('dynamicConfigChanged', configLoaded);
    OT.Config.on('dynamicConfigLoadFailed', configLoadFailed);

    OT.$.onDOMLoad(onDomReady);

    // If the plugin should work on this platform then
    // see if it loads.
    if (_pluginSupported) OTPlugin.ready(onPluginReady);

    this.onLoad = function(cb, context) {
      if (isReady()) {
        cb.call(context);
        return;
      }

      OT.on(OT.Event.names.ENV_LOADED, cb, context);
    };

    this.onUnload = function(cb, context) {
      if (this.isUnloaded()) {
        cb.call(context);
        return;
      }

      OT.on(OT.Event.names.ENV_UNLOADED, cb, context);
    };

    this.isUnloaded = function() {
      return OT.$.isDOMUnloaded();
    };
  }

  var EnvLoader = new EnvironmentLoader();

  OT.onLoad = function(cb, context) {
    EnvLoader.onLoad(cb, context);
  };

  OT.onUnload = function(cb, context) {
    EnvLoader.onUnload(cb, context);
  };

  OT.isUnloaded = function() {
    return EnvLoader.isUnloaded();
  };

})();

// tb_require('../ot.js')
// tb_require('../helpers/lib/dialogs.js')
// tb_require('../ot/environment_loader.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT, OTPlugin */

// Global parameters used by upgradeSystemRequirements
var _intervalId,
    _lastHash = document.location.hash;

OT.HAS_REQUIREMENTS = 1;
OT.NOT_HAS_REQUIREMENTS = 0;

/**
 * Checks if the system supports OpenTok for WebRTC.
 * @return {Number} Whether the system supports OpenTok for WebRTC (1) or not (0).
 * @see <a href="#upgradeSystemRequirements">OT.upgradeSystemRequirements()</a>
 * @method OT.checkSystemRequirements
 * @memberof OT
 */
OT.checkSystemRequirements = function() {
  OT.debug('OT.checkSystemRequirements()');

  // Try native support first, then OTPlugin...
  var systemRequirementsMet = OT.$.hasCapabilities('websockets', 'webrtc') ||
                                    OTPlugin.isInstalled();

  systemRequirementsMet = systemRequirementsMet ?
                                    this.HAS_REQUIREMENTS : this.NOT_HAS_REQUIREMENTS;

  OT.checkSystemRequirements = function() {
    OT.debug('OT.checkSystemRequirements()');
    return systemRequirementsMet;
  };

  if (systemRequirementsMet === this.NOT_HAS_REQUIREMENTS) {
    OT.analytics.logEvent({
      action: 'checkSystemRequirements',
      variation: 'notHasRequirements',
      partnerId: OT.APIKEY,
      payload: {userAgent: OT.$.env.userAgent}
    });
  }

  return systemRequirementsMet;
};

/**
 * Displays information about system requirments for OpenTok for WebRTC. This
 * information is displayed in an iframe element that fills the browser window.
 * <p>
 * <i>Note:</i> this information is displayed automatically when you call the
 * <code>OT.initSession()</code> or the <code>OT.initPublisher()</code> method
 * if the client does not support OpenTok for WebRTC.
 * </p>
 * @see <a href="#checkSystemRequirements">OT.checkSystemRequirements()</a>
 * @method OT.upgradeSystemRequirements
 * @memberof OT
 */
OT.upgradeSystemRequirements = function() {
  // trigger after the OT environment has loaded
  OT.onLoad(function() {

    if (OTPlugin.isSupported()) {
      OT.Dialogs.Plugin.promptToInstall().on({
        download: function() {
          window.location = OTPlugin.pathToInstaller();
        },
        refresh: function() {
          location.reload();
        },
        closed: function() {}
      });
      return;
    }

    var id = '_upgradeFlash';

    // Load the iframe over the whole page.
    document.body.appendChild((function() {
      var d = document.createElement('iframe');
      d.id = id;
      d.style.position = 'absolute';
      d.style.position = 'fixed';
      d.style.height = '100%';
      d.style.width = '100%';
      d.style.top = '0px';
      d.style.left = '0px';
      d.style.right = '0px';
      d.style.bottom = '0px';
      d.style.zIndex = 1000;
      try {
        d.style.backgroundColor = 'rgba(0,0,0,0.2)';
      } catch (err) {
        // Old IE browsers don't support rgba and we still want to show the upgrade message
        // but we just make the background of the iframe completely transparent.
        d.style.backgroundColor = 'transparent';
        d.setAttribute('allowTransparency', 'true');
      }
      d.setAttribute('frameBorder', '0');
      d.frameBorder = '0';
      d.scrolling = 'no';
      d.setAttribute('scrolling', 'no');

      var minimumBrowserVersion = OT.properties.minimumVersion[OT.$.env.name.toLowerCase()],
          isSupportedButOld =  minimumBrowserVersion > OT.$.env.version;
      d.src = OT.properties.assetURL + '/html/upgrade.html#' +
                        encodeURIComponent(isSupportedButOld ? 'true' : 'false') + ',' +
                        encodeURIComponent(JSON.stringify(OT.properties.minimumVersion)) + '|' +
                        encodeURIComponent(document.location.href);

      return d;
    })());

    // Now we need to listen to the event handler if the user closes this dialog.
    // Since this is from an IFRAME within another domain we are going to listen to hash
    // changes. The best cross browser solution is to poll for a change in the hashtag.
    if (_intervalId) clearInterval(_intervalId);
    _intervalId = setInterval(function() {
      var hash = document.location.hash,
          re = /^#?\d+&/;
      if (hash !== _lastHash && re.test(hash)) {
        _lastHash = hash;
        if (hash.replace(re, '') === 'close_window') {
          document.body.removeChild(document.getElementById(id));
          document.location.hash = '';
        }
      }
    }, 100);
  });
};

// tb_require('../helpers/helpers.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

OT.ConnectionCapabilities = function(capabilitiesHash) {
  // Private helper methods
  var castCapabilities = function(capabilitiesHash) {
    capabilitiesHash.supportsWebRTC = OT.$.castToBoolean(capabilitiesHash.supportsWebRTC);
    return capabilitiesHash;
  };

  // Private data
  var _caps = castCapabilities(capabilitiesHash);
  this.supportsWebRTC = _caps.supportsWebRTC;
};

// tb_require('../helpers/helpers.js')
// tb_require('../helpers/lib/properties.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

/**
 * A class defining properties of the <code>capabilities</code> property of a
 * Session object. See <a href="Session.html#properties">Session.capabilities</a>.
 * <p>
 * All Capabilities properties are undefined until you have connected to a session
 * and the Session object has dispatched the <code>sessionConnected</code> event.
 * <p>
 * For more information on token roles, see the
 * <a href="https://tokbox.com/developer/guides/create-token/">Token Creation Overview</a>.
 *
 * @class Capabilities
 *
 * @property {Number} forceDisconnect Specifies whether you can call
 * the <code>Session.forceDisconnect()</code> method (1) or not (0). To call the
 * <code>Session.forceDisconnect()</code> method,
 * the user must have a token that is assigned the role of moderator.
 * @property {Number} forceUnpublish Specifies whether you can call
 * the <code>Session.forceUnpublish()</code> method (1) or not (0). To call the
 * <code>Session.forceUnpublish()</code> method, the user must have a token that
 * is assigned the role of moderator.
 * @property {Number} publish Specifies whether you can publish to the session (1) or not (0).
 * The ability to publish is based on a few factors. To publish, the user must have a token that
 * is assigned a role that supports publishing. There must be a connected camera and microphone.
 * @property {Number} subscribe Specifies whether you can subscribe to streams
 * in the session (1) or not (0). Currently, this capability is available for all users on all
 * platforms.
 */
OT.Capabilities = function(permissions) {
  this.publish = OT.$.arrayIndexOf(permissions, 'publish') !== -1 ? 1 : 0;
  this.subscribe = OT.$.arrayIndexOf(permissions, 'subscribe') !== -1 ? 1 : 0;
  this.forceUnpublish = OT.$.arrayIndexOf(permissions, 'forceunpublish') !== -1 ? 1 : 0;
  this.forceDisconnect = OT.$.arrayIndexOf(permissions, 'forcedisconnect') !== -1 ? 1 : 0;
  this.supportsWebRTC = OT.$.hasCapabilities('webrtc') ? 1 : 0;

  this.permittedTo = function(action) {
    return this.hasOwnProperty(action) && this[action] === 1;
  };
};

// tb_require('../helpers/helpers.js')
// tb_require('../helpers/lib/properties.js')
// tb_require('./events.js')
// tb_require('./capabilities.js')
// tb_require('./connection_capabilities.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

/**
 * The Connection object represents a connection to an OpenTok session. Each client that connects
 * to a session has a unique connection, with a unique connection ID (represented by the
 * <code>id</code> property of the Connection object for the client).
 * <p>
 * The Session object has a <code>connection</code> property that is a Connection object.
 * It represents the local client's connection. (A client only has a connection once the
 * client has successfully called the <code>connect()</code> method of the {@link Session}
 * object.)
 * <p>
 * The Session object dispatches a <code>connectionCreated</code> event when each client (including
 * your own) connects to a session (and for clients that are present in the session when you
 * connect). The <code>connectionCreated</code> event object has a <code>connection</code>
 * property, which is a Connection object corresponding to the client the event pertains to.
 * <p>
 * The Stream object has a <code>connection</code> property that is a Connection object.
 * It represents the connection of the client that is publishing the stream.
 *
 * @class Connection
 * @property {String} connectionId The ID of this connection.
 * @property {Number} creationTime The timestamp for the creation of the connection. This
 * value is calculated in milliseconds.
 * You can convert this value to a Date object by calling <code>new Date(creationTime)</code>,
 * where <code>creationTime</code>
 * is the <code>creationTime</code> property of the Connection object.
 * @property {String} data A string containing metadata describing the
 * connection. When you generate a user token string pass the connection data string to the
 * <code>generate_token()</code> method of our
 * <a href="https://tokbox.com/opentok/libraries/server/">server-side libraries</a>. You can
 * also generate a token and define connection data on the
 * <a href="https://dashboard.tokbox.com/projects">Dashboard</a> page.
 */
OT.Connection = function(id, creationTime, data, capabilitiesHash, permissionsHash) {
  var destroyedReason;

  this.id = this.connectionId = id;
  this.creationTime = creationTime ? Number(creationTime) : null;
  this.data = data;
  this.capabilities = new OT.ConnectionCapabilities(capabilitiesHash);
  this.permissions = new OT.Capabilities(permissionsHash);
  this.quality = null;

  OT.$.eventing(this);

  this.destroy = OT.$.bind(function(reason, quiet) {
    destroyedReason = reason || 'clientDisconnected';

    if (quiet !== true) {
      this.dispatchEvent(
        new OT.DestroyedEvent(
          'destroyed',      // This should be OT.Event.names.CONNECTION_DESTROYED, but
                            // the value of that is currently shared with Session
          this,
          destroyedReason
        )
      );
    }
  }, this);

  this.destroyed = function() {
    return destroyedReason !== void 0;
  };

  this.destroyedReason = function() {
    return destroyedReason;
  };

};

OT.Connection.fromHash = function(hash) {
  return new OT.Connection(hash.id,
                           hash.creationTime,
                           hash.data,
                           OT.$.extend(hash.capablities || {}, { supportsWebRTC: true }),
                           hash.permissions || []);
};

// tb_require('../../../helpers/helpers.js')
// tb_require('./message.js')
// tb_require('../../connection.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

!(function() {

  var MAX_SIGNAL_DATA_LENGTH = 8192,
      MAX_SIGNAL_TYPE_LENGTH = 128;

  //
  // Error Codes:
  // 413 - Type too long
  // 400 - Type is invalid
  // 413 - Data too long
  // 400 - Data is invalid (can't be parsed as JSON)
  // 429 - Rate limit exceeded
  // 500 - Websocket connection is down
  // 404 - To connection does not exist
  // 400 - To is invalid
  //
  OT.Signal = function(sessionId, fromConnectionId, options) {
    var isInvalidType = function(type) {
        // Our format matches the unreserved characters from the URI RFC:
        // http://www.ietf.org/rfc/rfc3986
        return !/^[a-zA-Z0-9\-\._~]+$/.exec(type);
      },

      validateTo = function(toAddress) {
        if (!toAddress) {
          return {
            code: 400,
            reason: 'The signal to field was invalid. Either set it to a OT.Connection, ' +
                                'OT.Session, or omit it entirely'
          };
        }

        if (!(toAddress instanceof OT.Connection || toAddress instanceof OT.Session)) {
          return {
            code: 400,
            reason: 'The To field was invalid'
          };
        }

        return null;
      },

      validateType = function(type) {
        var error = null;

        if (type === null || type === void 0) {
          error = {
            code: 400,
            reason: 'The signal type was null or undefined. Either set it to a String value or ' +
              'omit it'
          };
        } else if (type.length > MAX_SIGNAL_TYPE_LENGTH) {
          error = {
            code: 413,
            reason: 'The signal type was too long, the maximum length of it is ' +
              MAX_SIGNAL_TYPE_LENGTH + ' characters'
          };
        } else if (isInvalidType(type)) {
          error = {
            code: 400,
            reason: 'The signal type was invalid, it can only contain letters, ' +
              'numbers, \'-\', \'_\', and \'~\'.'
          };
        }

        return error;
      },

      validateData = function(data) {
        var error = null;
        if (data === null || data === void 0) {
          error = {
            code: 400,
            reason: 'The signal data was null or undefined. Either set it to a String value or ' +
              'omit it'
          };
        } else {
          try {
            if (JSON.stringify(data).length > MAX_SIGNAL_DATA_LENGTH) {
              error = {
                code: 413,
                reason: 'The data field was too long, the maximum size of it is ' +
                  MAX_SIGNAL_DATA_LENGTH + ' characters'
              };
            }
          } catch (e) {
            error = {code: 400, reason: 'The data field was not valid JSON'};
          }
        }

        return error;
      };

    this.toRaptorMessage = function() {
      var to = this.to;

      if (to && typeof to !== 'string') {
        to = to.id;
      }

      return OT.Raptor.Message.signals.create(OT.APIKEY, sessionId, to, this.type, this.data);
    };

    this.toHash = function() {
      return options;
    };

    this.error = null;

    if (options) {
      if (options.hasOwnProperty('data')) {
        this.data = OT.$.clone(options.data);
        this.error = validateData(this.data);
      }

      if (options.hasOwnProperty('to')) {
        this.to = options.to;

        if (!this.error) {
          this.error = validateTo(this.to);
        }
      }

      if (options.hasOwnProperty('type')) {
        if (!this.error) {
          this.error = validateType(options.type);
        }
        this.type = options.type;
      }
    }

    this.valid = this.error === null;
  };

}(this));

// tb_require('../../../helpers/helpers.js')
// tb_require('../rumor/rumor.js')
// tb_require('./message.js')
// tb_require('./dispatch.js')
// tb_require('./signal.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

function SignalError(code, message) {
  this.code = code;
  this.message = message;

  // Undocumented. Left in for backwards compatibility:
  this.reason = message;
}

// The Dispatcher bit is purely to make testing simpler, it defaults to a new OT.Raptor.Dispatcher
// so in normal operation you would omit it.
OT.Raptor.Socket = function(connectionId, widgetId, messagingSocketUrl, symphonyUrl, dispatcher) {
  var _states = ['disconnected', 'connecting', 'connected', 'error', 'disconnecting'],
      _sessionId,
      _token,
      _rumor,
      _dispatcher,
      _completion;

  //// Private API
  var setState = OT.$.statable(this, _states, 'disconnected'),

      onConnectComplete = function onConnectComplete(error) {
        if (error) {
          setState('error');
        } else {
          setState('connected');
        }

        _completion.apply(null, arguments);
      },

      onClose = OT.$.bind(function onClose(err) {
        var reason = 'clientDisconnected';
        if (!this.is('disconnecting') && _rumor.is('error')) {
          reason = 'networkDisconnected';
        }
        if (err && err.code === 4001) {
          reason = 'networkTimedout';
        }

        setState('disconnected');

        _dispatcher.onClose(reason);
      }, this),

      onError = function onError() {};
  // @todo what does having an error mean? Are they always fatal? Are we disconnected now?

  //// Public API

  this.connect = function(token, sessionInfo, completion) {
    if (!this.is('disconnected', 'error')) {
      OT.warn('Cannot connect the Raptor Socket as it is currently connected. You should ' +
        'disconnect first.');
      return;
    }

    setState('connecting');
    _sessionId = sessionInfo.sessionId;
    _token = token;
    _completion = completion;

    var rumorChannel = '/v2/partner/' + OT.APIKEY + '/session/' + _sessionId;

    _rumor = new OT.Rumor.Socket(messagingSocketUrl, symphonyUrl);
    _rumor.onClose(onClose);
    _rumor.onMessage(OT.$.bind(_dispatcher.dispatch, _dispatcher));

    _rumor.connect(connectionId, OT.$.bind(function(error) {
      if (error) {
        onConnectComplete({
          reason: 'WebSocketConnection',
          code: error.code,
          message: error.message
        });
        return;
      }

      // we do this here to avoid getting connect errors twice
      _rumor.onError(onError);

      OT.debug('Raptor Socket connected. Subscribing to ' +
        rumorChannel + ' on ' + messagingSocketUrl);

      _rumor.subscribe([rumorChannel]);

      //connect to session
      var connectMessage = OT.Raptor.Message.connections.create(OT.APIKEY,
        _sessionId, _rumor.id());
      this.publish(connectMessage, {'X-TB-TOKEN-AUTH': _token}, OT.$.bind(function(error) {
        if (error) {
          var errorCode,
            errorMessage,
            knownErrorCodes = [400, 403, 409];

          if (error.code === OT.ExceptionCodes.CONNECT_FAILED) {
            errorCode = error.code;
            errorMessage = OT.getErrorTitleByCode(error.code);
          } else if (error.code && OT.$.arrayIndexOf(knownErrorCodes, error.code) > -1) {
            errorCode = OT.ExceptionCodes.CONNECT_FAILED;
            errorMessage = 'Received error response to connection create message.';
          } else {
            errorCode = OT.ExceptionCodes.UNEXPECTED_SERVER_RESPONSE;
            errorMessage = 'Unexpected server response. Try this operation again later.';
          }

          error.message = 'ConnectToSession:' + error.code +
              ':Received error response to connection create message.';
          var payload = {
            reason: 'ConnectToSession',
            code: errorCode,
            message: errorMessage
          };
          var event = {
            action: 'Connect',
            variation: 'Failure',
            payload: payload,
            sessionId: _sessionId,
            partnerId: OT.APIKEY,
            connectionId: connectionId
          };
          OT.analytics.logEvent(event);
          onConnectComplete(payload);
          return;
        }

        this.publish(OT.Raptor.Message.sessions.get(OT.APIKEY, _sessionId),
          function(error) {
          if (error) {
            var errorCode,
              errorMessage,
              knownErrorCodes = [400, 403, 409];
            if (error.code && OT.$.arrayIndexOf(knownErrorCodes, error.code) > -1) {
              errorCode = OT.ExceptionCodes.CONNECT_FAILED;
              errorMessage = 'Received error response to session read';
            } else {
              errorCode = OT.ExceptionCodes.UNEXPECTED_SERVER_RESPONSE;
              errorMessage = 'Unexpected server response. Try this operation again later.';
            }
            var payload = {
              reason: 'GetSessionState',
              code: error.code,
              message: errorMessage
            };
            var event = {
              action: 'Connect',
              variation: 'Failure',
              payload: payload,
              sessionId: _sessionId,
              partnerId: OT.APIKEY,
              connectionId: connectionId
            };
            OT.analytics.logEvent(event);
            onConnectComplete(payload);
          } else {
            onConnectComplete.apply(null, arguments);
          }
        });
      }, this));
    }, this));
  };

  this.disconnect = function(drainSocketBuffer) {
    if (this.is('disconnected')) return;

    setState('disconnecting');
    _rumor.disconnect(drainSocketBuffer);
  };

  // Publishs +message+ to the Symphony app server.
  //
  // The completion handler is optional, as is the headers
  // dict, but if you provide the completion handler it must
  // be the last argument.
  //
  this.publish = function(message, headers, completion) {
    if (_rumor.isNot('connected')) {
      OT.error('OT.Raptor.Socket: cannot publish until the socket is connected.' + message);
      return;
    }

    var transactionId = OT.$.uuid(),
        _headers = {},
        _completion;

    // Work out if which of the optional arguments (headers, completion)
    // have been provided.
    if (headers) {
      if (OT.$.isFunction(headers)) {
        _headers = {};
        _completion = headers;
      } else {
        _headers = headers;
      }
    }
    if (!_completion && completion && OT.$.isFunction(completion)) _completion = completion;

    if (_completion) _dispatcher.registerCallback(transactionId, _completion);

    OT.debug('OT.Raptor.Socket Publish (ID:' + transactionId + ') ');
    OT.debug(message);

    _rumor.publish([symphonyUrl], message, OT.$.extend(_headers, {
      'Content-Type': 'application/x-raptor+v2',
      'TRANSACTION-ID': transactionId,
      'X-TB-FROM-ADDRESS': _rumor.id()
    }));

    return transactionId;
  };

  // Register a new stream against _sessionId
  this.streamCreate = function(name, streamId, audioFallbackEnabled, channels, minBitrate,
    maxBitrate, completion) {
    var message = OT.Raptor.Message.streams.create(OT.APIKEY,
                                                   _sessionId,
                                                   streamId,
                                                   name,
                                                   audioFallbackEnabled,
                                                   channels,
                                                   minBitrate,
                                                   maxBitrate);

    this.publish(message, function(error, message) {
      completion(error, streamId, message);
    });
  };

  this.streamDestroy = function(streamId) {
    this.publish(OT.Raptor.Message.streams.destroy(OT.APIKEY, _sessionId, streamId));
  };

  this.streamChannelUpdate = function(streamId, channelId, attributes) {
    this.publish(OT.Raptor.Message.streamChannels.update(OT.APIKEY, _sessionId,
      streamId, channelId, attributes));
  };

  this.subscriberCreate = function(streamId, subscriberId, channelsToSubscribeTo, completion) {
    this.publish(OT.Raptor.Message.subscribers.create(OT.APIKEY, _sessionId,
      streamId, subscriberId, _rumor.id(), channelsToSubscribeTo), completion);
  };

  this.subscriberDestroy = function(streamId, subscriberId) {
    this.publish(OT.Raptor.Message.subscribers.destroy(OT.APIKEY, _sessionId,
      streamId, subscriberId));
  };

  this.subscriberUpdate = function(streamId, subscriberId, attributes) {
    this.publish(OT.Raptor.Message.subscribers.update(OT.APIKEY, _sessionId,
      streamId, subscriberId, attributes));
  };

  this.subscriberChannelUpdate = function(streamId, subscriberId, channelId, attributes) {
    this.publish(OT.Raptor.Message.subscriberChannels.update(OT.APIKEY, _sessionId,
      streamId, subscriberId, channelId, attributes));
  };

  this.forceDisconnect = function(connectionIdToDisconnect, completion) {
    this.publish(OT.Raptor.Message.connections.destroy(OT.APIKEY, _sessionId,
      connectionIdToDisconnect), completion);
  };

  this.forceUnpublish = function(streamIdToUnpublish, completion) {
    this.publish(OT.Raptor.Message.streams.destroy(OT.APIKEY, _sessionId,
      streamIdToUnpublish), completion);
  };

  this.jsepCandidate = function(streamId, candidate) {
    this.publish(
      OT.Raptor.Message.streams.candidate(OT.APIKEY, _sessionId, streamId, candidate)
    );
  };

  this.jsepCandidateP2p = function(streamId, subscriberId, candidate) {
    this.publish(
      OT.Raptor.Message.subscribers.candidate(OT.APIKEY, _sessionId, streamId,
        subscriberId, candidate)
    );
  };

  this.jsepOffer = function(uri, offerSdp) {
    this.publish(OT.Raptor.Message.offer(uri, offerSdp));
  };

  this.jsepAnswer = function(streamId, answerSdp) {
    this.publish(OT.Raptor.Message.streams.answer(OT.APIKEY, _sessionId, streamId, answerSdp));
  };

  this.jsepAnswerP2p = function(streamId, subscriberId, answerSdp) {
    this.publish(OT.Raptor.Message.subscribers.answer(OT.APIKEY, _sessionId, streamId,
      subscriberId, answerSdp));
  };

  this.signal = function(options, completion, logEventFn) {
    var signal = new OT.Signal(_sessionId, _rumor.id(), options || {});

    if (!signal.valid) {
      if (completion && OT.$.isFunction(completion)) {
        completion(new SignalError(signal.error.code, signal.error.reason), signal.toHash());
      }

      return;
    }

    this.publish(signal.toRaptorMessage(), function(err) {
      var error,
        errorCode,
        errorMessage,
        expectedErrorCodes = [400, 403, 404, 413];
      if (err) {
        if (err.code && OT.$.arrayIndexOf(expectedErrorCodes, error.code) > -1) {
          errorCode = err.code;
          errorMessage = err.message;
        } else {
          errorCode = OT.ExceptionCodes.UNEXPECTED_SERVER_RESPONSE;
          errorMessage = 'Unexpected server response. Try this operation again later.';
        }
        error = new OT.SignalError(errorCode, errorMessage);
      } else {
        var typeStr = signal.data ? typeof (signal.data) : null;
        logEventFn('signal', 'send', {type: typeStr});
      }

      if (completion && OT.$.isFunction(completion)) completion(error, signal.toHash());
    });
  };

  this.id = function() {
    return _rumor && _rumor.id();
  };

  if (dispatcher == null) {
    dispatcher = new OT.Raptor.Dispatcher();
  }
  _dispatcher = dispatcher;
};

// tb_require('../helpers/helpers.js')
// tb_require('../helpers/lib/capabilities.js')
// tb_require('./peer_connection/publisher_peer_connection.js')
// tb_require('./peer_connection/subscriber_peer_connection.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

/*
 * A <code>RTCPeerConnection.getStats</code> based audio level sampler.
 *
 * It uses the the <code>getStats</code> method to get the <code>audioOutputLevel</code>.
 * This implementation expects the single parameter version of the <code>getStats</code> method.
 *
 * Currently the <code>audioOutputLevel</code> stats is only supported in Chrome.
 *
 * @param {OT.SubscriberPeerConnection} peerConnection the peer connection to use to get the stats
 * @constructor
 */
OT.GetStatsAudioLevelSampler = function(peerConnection) {

  if (!OT.$.hasCapabilities('audioOutputLevelStat')) {
    throw new Error('The current platform does not provide the required capabilities');
  }

  var _peerConnection = peerConnection,
      _statsProperty = 'audioOutputLevel';

  /*
   * Acquires the audio level.
   *
   * @param {function(?number)} done a callback to be called with the acquired value in the
   * [0, 1] range when available or <code>null</code> if no value could be acquired
   */
  this.sample = function(done) {
    _peerConnection.getStats(function(error, stats) {
      if (!error) {
        for (var idx = 0; idx < stats.length; idx++) {
          var stat = stats[idx];
          var audioOutputLevel = parseFloat(stat[_statsProperty]);
          if (!isNaN(audioOutputLevel)) {
            // the mex value delivered by getStats for audio levels is 2^15
            done(audioOutputLevel / 32768);
            return;
          }
        }
      }

      done(null);
    });
  };
};

/*
 * An <code>AudioContext</code> based audio level sampler. It returns the maximum value in the
 * last 1024 samples.
 *
 * It is worth noting that the remote <code>MediaStream</code> audio analysis is currently only
 * available in FF.
 *
 * This implementation gracefully handles the case where the <code>MediaStream</code> has not
 * been set yet by returning a <code>null</code> value until the stream is set. It is up to the
 * call site to decide what to do with this value (most likely ignore it and retry later).
 *
 * @constructor
 * @param {AudioContext} audioContext an audio context instance to get an analyser node
 */
OT.AnalyserAudioLevelSampler = function(audioContext) {

  var _sampler = this,
      _analyser = null,
      _timeDomainData = null,
      _webRTCStream = null;

  var buildAnalyzer = function(stream) {
        var sourceNode = audioContext.createMediaStreamSource(stream);
        var analyser = audioContext.createAnalyser();
        sourceNode.connect(analyser);
        return analyser;
      };

  OT.$.defineProperties(_sampler, {
    webRTCStream: {
      get: function() {
        return _webRTCStream;
      },
      set: function(webRTCStream) {
        // when the stream is updated we need to create a new analyzer
        _webRTCStream = webRTCStream;
        _analyser = buildAnalyzer(_webRTCStream);
        _timeDomainData = new Uint8Array(_analyser.frequencyBinCount);
      }
    }
  });

  this.sample = function(done) {
    if (_analyser) {
      _analyser.getByteTimeDomainData(_timeDomainData);

      // varies from 0 to 255
      var max = 0;
      for (var idx = 0; idx < _timeDomainData.length; idx++) {
        max = Math.max(max, Math.abs(_timeDomainData[idx] - 128));
      }

      // normalize the collected level to match the range delivered by
      // the getStats' audioOutputLevel
      done(max / 128);
    } else {
      done(null);
    }
  };
};

/*
 * Transforms a raw audio level to produce a "smoother" animation when using displaying the
 * audio level. This transformer is state-full because it needs to keep the previous average
 * value of the signal for filtering.
 *
 * It applies a low pass filter to get rid of level jumps and apply a log scale.
 *
 * @constructor
 */
OT.AudioLevelTransformer = function() {

  var _averageAudioLevel = null;

  /*
   *
   * @param {number} audioLevel a level in the [0,1] range
   * @returns {number} a level in the [0,1] range transformed
   */
  this.transform = function(audioLevel) {
    if (_averageAudioLevel === null || audioLevel >= _averageAudioLevel) {
      _averageAudioLevel = audioLevel;
    } else {
      // a simple low pass filter with a smoothing of 70
      _averageAudioLevel = audioLevel * 0.3 + _averageAudioLevel * 0.7;
    }

    // 1.5 scaling to map -30-0 dBm range to [0,1]
    var logScaled = (Math.log(_averageAudioLevel) / Math.LN10) / 1.5 + 1;

    return Math.min(Math.max(logScaled, 0), 1);
  };
};

// tb_require('../helpers/helpers.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

/*
 * Lazy instantiates an audio context and always return the same instance on following calls
 *
 * @returns {AudioContext}
 */
OT.audioContext = function() {
  var context = new window.AudioContext();
  OT.audioContext = function() {
    return context;
  };
  return context;
};

// tb_require('../helpers/helpers.js')
// tb_require('./events.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

OT.Archive = function(id, name, status) {
  this.id = id;
  this.name = name;
  this.status = status;

  this._ = {};

  OT.$.eventing(this);

  // Mass update, called by Raptor.Dispatcher
  this._.update = OT.$.bind(function(attributes) {
    for (var key in attributes) {
      if (!attributes.hasOwnProperty(key)) {
        continue;
      }
      var oldValue = this[key];
      this[key] = attributes[key];

      var event = new OT.ArchiveUpdatedEvent(this, key, oldValue, this[key]);
      this.dispatchEvent(event);
    }
  }, this);

  this.destroy = function() {};

};

// tb_require('../helpers/helpers.js')
// tb_require('../helpers/lib/properties.js')
// tb_require('./events.js')

!(function() {
  var Anvil = {};

  // @todo These aren't the same for all resource types.
  var httpToClientCode = {
    400: OT.ExceptionCodes.INVALID_SESSION_ID,
    403: OT.ExceptionCodes.AUTHENTICATION_ERROR,
    404: OT.ExceptionCodes.INVALID_SESSION_ID,
    409: OT.ExceptionCodes.TERMS_OF_SERVICE_FAILURE
  };

  var anvilErrors = {
    RESPONSE_BADLY_FORMED: {
      code: null,
      message: 'Unknown error: JSON response was badly formed'
    },

    UNEXPECTED_SERVER_RESPONSE: {
      code: OT.ExceptionCodes.UNEXPECTED_SERVER_RESPONSE,
      message: 'Unexpected server response. Try this operation again later.'
    }
  };

  // Transform an error that we don't understand (+originalError+) to a general
  // UNEXPECTED_SERVER_RESPONSE one. We also include the original error details
  // on the `error.details` property.
  //
  var normaliseUnexpectedError = function normaliseUnexpectedError(originalError) {
    var error = OT.$.clone(anvilErrors.UNEXPECTED_SERVER_RESPONSE);

    // We don't know this code...capture whatever the original
    // error and code were for debugging purposes.
    error.details = {
      originalCode: originalError.code.toString(),
      originalMessage: originalError.message
    };

    return error;
  };

  Anvil.getRequestParams = function getApiRequestOptions(resourcePath, token) {
    var url = OT.properties.apiURL + '/' + resourcePath;
    var options;

    if (OT.$.env.name === 'IE' && OT.$.env.version < 10) {
      url = url + '&format=json&token=' + encodeURIComponent(token) +
                  '&version=1&cache=' + OT.$.uuid();
      options = {
        xdomainrequest: true
      };
    } else {
      options = {
        headers: {
          'X-TB-TOKEN-AUTH': token,
          'X-TB-VERSION': 1
        }
      };
    }

    return {
      url: url,
      options: options
    };
  };

  Anvil.getErrorsFromHTTP = function(httpError) {
    if (!httpError) {
      return false;
    }

    var error = {
      code: httpError.target && httpError.target.status
    };

    return error;
  };

  Anvil.getErrorsFromResponse = function getErrorsFromResponse(responseJson) {
    if (!OT.$.isArray(responseJson)) {
      return anvilErrors.RESPONSE_BADLY_FORMED;
    }

    var error = OT.$.find(responseJson, function(node) {
      return node.error !== void 0 && node.error !== null;
    });

    if (!error) {
      return false;
    }

    // Yup :-(
    error = error.error;
    error.message = error.errorMessage && error.errorMessage.message;

    return error;
  };

  var normaliseError = function normaliseError(error, responseText) {
    if (error.code && !httpToClientCode[error.code]) {
      error = normaliseUnexpectedError(error);
    } else {
      error.code = httpToClientCode[error.code];
    }

    if (responseText.length === 0) {
      // This is a weird edge case that usually means that there was connectivity
      // loss after Anvil sent the response but before the client had fully received it
      error.code = OT.ExceptionCodes.CONNECT_FAILED;
      responseText = 'Response body was empty, probably due to connectivity loss';
    } else if (!error.code) {
      error = normaliseUnexpectedError(error);
    }

    if (!error.details) error.details = {};
    error.details.responseText = responseText;

    return error;
  };

  Anvil.get = function getFromAnvil(resourcePath, token) {
    var params = Anvil.getRequestParams(resourcePath, token);

    return new OT.$.RSVP.Promise(function(resolve, reject) {
      OT.$.getJSON(params.url, params.options, function(httpError, responseJson) {
        var err = Anvil.getErrorsFromHTTP(httpError) || Anvil.getErrorsFromResponse(responseJson);
        var responseText;

        if (err) {
          responseText = responseJson && !OT.$.isEmpty(responseJson) ?
                                          JSON.stringify(responseJson) : '';
          err = normaliseError(err, responseText);

          reject(new OT.$.Error(err.message, 'AnvilError', {
            code: err.code,
            details: err.details
          }));

          return;
        }

        resolve(responseJson[0]);
      });
    });
  };

  OT.Anvil = Anvil;

})(window);

// tb_require('./anvil.js')

!(function() {

  // This sequence defines the delay before retry. Therefore a 0 indicates
  // that a retry should happen immediately.
  //
  // e.g. 0, 600, 1200 means retry immediately, then in 600 ms, then in 1200ms
  //
  var retryDelays = [0, 600, 1200];

  // These codes are possibly transient and it's worth retrying if a Anvil request
  // fails with one of these codes.
  var transientErrorCodes = [
    OT.ExceptionCodes.CONNECT_FAILED,
    OT.ExceptionCodes.UNEXPECTED_SERVER_RESPONSE
  ];

  OT.SessionInfo = function(rawSessionInfo) {
    OT.log('SessionInfo Response:');
    OT.log(rawSessionInfo);

    /*jshint camelcase:false*/
    //jscs:disable requireCamelCaseOrUpperCaseIdentifiers
    this.sessionId = rawSessionInfo.session_id;
    this.partnerId = rawSessionInfo.partner_id;
    this.sessionStatus = rawSessionInfo.session_status;
    this.messagingServer = rawSessionInfo.messaging_server_url;
    this.messagingURL = rawSessionInfo.messaging_url;
    this.symphonyAddress = rawSessionInfo.symphony_address;

    if (rawSessionInfo.properties) {
      // `simulcast` is tri-state:
      // true: simulcast is on for this session
      // false: simulcast is off for this session
      // undefined: the developer can choose
      //
      this.simulcast = rawSessionInfo.properties.simulcast;

      this.p2pEnabled = !!(rawSessionInfo.properties.p2p &&
        rawSessionInfo.properties.p2p.preference &&
        rawSessionInfo.properties.p2p.preference.value === 'enabled');
    } else {
      this.p2pEnabled = false;
    }
  };

  // Retrieves Session Info for +session+. The SessionInfo object will be passed
  // to the +onSuccess+ callback. The +onFailure+ callback will be passed an error
  // object and the DOMEvent that relates to the error.
  //
  OT.SessionInfo.get = function(id, token) {
    var remainingRetryDelays = retryDelays.slice();

    var attempt = function(err, resolve, reject) {
      if (remainingRetryDelays.length === 0) {
        reject(err);
        return;
      }

      OT.Anvil.get('session/' + id + '?extended=true', token).then(function(anvilResponse) {
        resolve(new OT.SessionInfo(anvilResponse));
      }, function(err) {
        if (OT.$.arrayIndexOf(transientErrorCodes, err.code) > -1) {
          // This error is possibly transient, so retry
          setTimeout(function() {
            attempt(err, resolve, reject);
          }, remainingRetryDelays.shift());
        } else {
          reject(err);
        }
      });
    };

    return new OT.$.RSVP.Promise(function(resolve, reject) {
      attempt(void 0, resolve, reject);
    });
  };

})(window);

// tb_require('../helpers/helpers.js')
// tb_require('../helpers/lib/properties.js')
// tb_require('../helpers/lib/analytics.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

OT.analytics = new OT.Analytics(OT.properties.loggingURL);

// tb_require('../helpers/helpers.js')
// tb_require('../helpers/lib/widget_view.js')
// tb_require('./analytics.js')
// tb_require('./events.js')
// tb_require('./system_requirements.js')
// tb_require('./stylable_component.js')
// tb_require('./stream.js')
// tb_require('./connection.js')
// tb_require('./subscribing_state.js')
// tb_require('./environment_loader.js')
// tb_require('./audio_level_samplers.js')
// tb_require('./audio_context.js')
// tb_require('./chrome/chrome.js')
// tb_require('./chrome/backing_bar.js')
// tb_require('./chrome/name_panel.js')
// tb_require('./chrome/mute_button.js')
// tb_require('./chrome/archiving.js')
// tb_require('./chrome/audio_level_meter.js')
// tb_require('./peer_connection/subscriber_peer_connection.js')
// tb_require('./peer_connection/get_stats_adapter.js')
// tb_require('./peer_connection/get_stats_helpers.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

/**
 * The Subscriber object is a representation of the local video element that is playing back
 * a remote stream. The Subscriber object includes methods that let you disable and enable
 * local audio playback for the subscribed stream. The <code>subscribe()</code> method of the
 * {@link Session} object returns a Subscriber object.
 *
 * @property {Element} element The HTML DOM element containing the Subscriber.
 * @property {String} id The DOM ID of the Subscriber.
 * @property {Stream} stream The stream to which you are subscribing.
 *
 * @class Subscriber
 * @augments EventDispatcher
 */
OT.Subscriber = function(targetElement, options, completionHandler) {
  var _widgetId = OT.$.uuid(),
      _domId = targetElement || _widgetId,
      _container,
      _streamContainer,
      _chrome,
      _audioLevelMeter,
      _fromConnectionId,
      _peerConnection,
      _session = options.session,
      _stream = options.stream,
      _subscribeStartTime,
      _startConnectingTime,
      _properties,
      _audioVolume = 100,
      _state,
      _prevStats,
      _lastSubscribeToVideoReason,
      _audioLevelCapable =  OT.$.hasCapabilities('audioOutputLevelStat') ||
                            OT.$.hasCapabilities('webAudioCapableRemoteStream'),
      _audioLevelSampler,
      _audioLevelRunner,
      _frameRateRestricted = false,
      _connectivityAttemptPinger,
      _subscriber = this,
      _isLocalStream;

  _properties = OT.$.defaults({}, options, {
    showControls: true,
    testNetwork: false,
    fitMode: _stream.defaultFitMode || 'cover'
  });

  this.id = _domId;
  this.widgetId = _widgetId;
  this.session = _session;
  this.stream = _stream = _properties.stream;
  this.streamId = _stream.id;

  _prevStats = {
    timeStamp: OT.$.now()
  };

  if (!_session) {
    OT.handleJsException('Subscriber must be passed a session option', 2000, {
      session: _session,
      target: this
    });

    return;
  }

  OT.$.eventing(this, false);

  if (typeof completionHandler === 'function') {
    this.once('subscribeComplete', completionHandler);
  }

  if (_audioLevelCapable) {
    this.on({
      'audioLevelUpdated:added': function(count) {
        if (count === 1 && _audioLevelRunner) {
          _audioLevelRunner.start();
        }
      },
      'audioLevelUpdated:removed': function(count) {
        if (count === 0 && _audioLevelRunner) {
          _audioLevelRunner.stop();
        }
      }
    });
  }

  var logAnalyticsEvent = function(action, variation, payload, throttle) {
        var args = [{
          action: action,
          variation: variation,
          payload: payload,
          streamId: _stream ? _stream.id : null,
          sessionId: _session ? _session.sessionId : null,
          connectionId: _session && _session.isConnected() ?
            _session.connection.connectionId : null,
          partnerId: _session && _session.isConnected() ? _session.sessionInfo.partnerId : null,
          subscriberId: _widgetId
        }];
        if (throttle) args.push(throttle);
        OT.analytics.logEvent.apply(OT.analytics, args);
      },

      logConnectivityEvent = function(variation, payload) {
        if (variation === 'Attempt' || !_connectivityAttemptPinger) {
          _connectivityAttemptPinger = new OT.ConnectivityAttemptPinger({
            action: 'Subscribe',
            sessionId: _session ? _session.sessionId : null,
            connectionId: _session && _session.isConnected() ?
              _session.connection.connectionId : null,
            partnerId: _session.isConnected() ? _session.sessionInfo.partnerId : null,
            streamId: _stream ? _stream.id : null
          });
        }
        _connectivityAttemptPinger.setVariation(variation);
        logAnalyticsEvent('Subscribe', variation, payload);
      },

      recordQOS = OT.$.bind(function(parsedStats) {
        var QoSBlob = {
          streamType: 'WebRTC',
          width: _container ? Number(OT.$.width(_container.domElement).replace('px', '')) : null,
          height: _container ? Number(OT.$.height(_container.domElement).replace('px', '')) : null,
          sessionId: _session ? _session.sessionId : null,
          connectionId: _session ? _session.connection.connectionId : null,
          mediaServerName: _session ? _session.sessionInfo.messagingServer : null,
          p2pFlag: _session ? _session.sessionInfo.p2pEnabled : false,
          partnerId: _session ? _session.apiKey : null,
          streamId: _stream.id,
          subscriberId: _widgetId,
          version: OT.properties.version,
          duration: parseInt(OT.$.now() - _subscribeStartTime, 10),
          remoteConnectionId: _stream.connection.connectionId
        };

        OT.analytics.logQOS(OT.$.extend(QoSBlob, parsedStats));
        this.trigger('qos', parsedStats);
      }, this),

      stateChangeFailed = function(changeFailed) {
        OT.error('Subscriber State Change Failed: ', changeFailed.message);
        OT.debug(changeFailed);
      },

      onLoaded = function() {
        if (_state.isSubscribing() || !_streamContainer) return;

        OT.debug('OT.Subscriber.onLoaded');

        _state.set('Subscribing');
        _subscribeStartTime = OT.$.now();

        var payload = {
          pcc: parseInt(_subscribeStartTime - _startConnectingTime, 10),
          hasRelayCandidates: _peerConnection && _peerConnection.hasRelayCandidates()
        };
        logAnalyticsEvent('createPeerConnection', 'Success', payload);

        _container.loading(false);
        _chrome.showAfterLoading();

        if (_frameRateRestricted) {
          _stream.setRestrictFrameRate(true);
        }

        if (_audioLevelMeter && _subscriber.getStyle('audioLevelDisplayMode') === 'auto') {
          _audioLevelMeter[_container.audioOnly() ? 'show' : 'hide']();
        }

        this.trigger('subscribeComplete', null, this);
        this.trigger('loaded', this);

        logConnectivityEvent('Success', {streamId: _stream.id});
      },

      onDisconnected = function() {
        OT.debug('OT.Subscriber has been disconnected from the Publisher\'s PeerConnection');

        if (_state.isAttemptingToSubscribe()) {
          // subscribing error
          _state.set('Failed');
          this.trigger('subscribeComplete', new OT.Error(null, 'ClientDisconnected'));

        } else if (_state.isSubscribing()) {
          _state.set('Failed');

          // we were disconnected after we were already subscribing
          // probably do nothing?
        }

        this.disconnect();
      },

      onPeerConnectionFailure = OT.$.bind(function(code, reason, peerConnection, prefix) {
        var payload;
        if (_state.isAttemptingToSubscribe()) {
          // We weren't subscribing yet so this was a failure in setting
          // up the PeerConnection or receiving the initial stream.
          payload = {
            reason: prefix ? prefix : 'PeerConnectionError',
            message: 'Subscriber PeerConnection Error: ' + reason,
            hasRelayCandidates: _peerConnection && _peerConnection.hasRelayCandidates()
          };
          logAnalyticsEvent('createPeerConnection', 'Failure', payload);

          _state.set('Failed');
          this.trigger('subscribeComplete', new OT.Error(null, reason));

        } else if (_state.isSubscribing()) {
          // we were disconnected after we were already subscribing
          _state.set('Failed');
          this.trigger('error', reason);
        }

        this.disconnect();

        var errorCode;
        if (prefix === 'ICEWorkflow') {
          errorCode = OT.ExceptionCodes.SUBSCRIBER_ICE_WORKFLOW_FAILED;
        } else {
          errorCode = OT.ExceptionCodes.P2P_CONNECTION_FAILED;
        }
        payload = {
          reason: prefix ? prefix : 'PeerConnectionError',
          message: 'Subscriber PeerConnection Error: ' + reason,
          code: errorCode
        };
        logConnectivityEvent('Failure', payload);

        OT.handleJsException('Subscriber PeerConnection Error: ' + reason,
          errorCode, {
            session: _session,
            target: this
          }
        );
        _showError.call(this, reason);
      }, this),

      onRemoteStreamAdded = function(webRTCStream) {
        OT.debug('OT.Subscriber.onRemoteStreamAdded');

        _state.set('BindingRemoteStream');

        // Disable the audio/video, if needed
        this.subscribeToAudio(_properties.subscribeToAudio);

        _lastSubscribeToVideoReason = 'loading';
        this.subscribeToVideo(_properties.subscribeToVideo, 'loading');
        this.setPreferredResolution(_properties.preferredResolution);
        this.setPreferredFrameRate(_properties.preferredFrameRate);

        var videoContainerOptions = {
          error: onPeerConnectionFailure,
          audioVolume: _audioVolume
        };

        // This is a workaround for a bug in Chrome where a track disabled on
        // the remote end doesn't fire loadedmetadata causing the subscriber to timeout
        // https://jira.tokbox.com/browse/OPENTOK-15605
        // Also https://jira.tokbox.com/browse/OPENTOK-16425
        //
        // Workaround for an IE issue https://jira.tokbox.com/browse/OPENTOK-18824
        // We still need to investigate further.
        //
        var tracks = webRTCStream.getVideoTracks();
        if (tracks.length > 0) {
          OT.$.forEach(tracks, function(track) {
            track.setEnabled(_stream.hasVideo && _properties.subscribeToVideo);
          });
        }

        _streamContainer = _container.bindVideo(webRTCStream,
                                            videoContainerOptions,
                                            OT.$.bind(function(err) {
          if (err) {
            onPeerConnectionFailure(null, err.message || err, _peerConnection, 'VideoElement');
            return;
          }
          if (_streamContainer) {
            _streamContainer.orientation({
              width: _stream.videoDimensions.width,
              height: _stream.videoDimensions.height,
              videoOrientation: _stream.videoDimensions.orientation
            });
          }

          onLoaded.call(this, null);
        }, this));

        if (OT.$.hasCapabilities('webAudioCapableRemoteStream') && _audioLevelSampler &&
          webRTCStream.getAudioTracks().length > 0) {
          _audioLevelSampler.webRTCStream(webRTCStream);
        }

        logAnalyticsEvent('createPeerConnection', 'StreamAdded');
        this.trigger('streamAdded', this);
      },

      onRemoteStreamRemoved = function(webRTCStream) {
        OT.debug('OT.Subscriber.onStreamRemoved');

        if (_streamContainer.stream === webRTCStream) {
          _streamContainer.destroy();
          _streamContainer = null;
        }

        this.trigger('streamRemoved', this);
      },

      streamDestroyed = function() {
        this.disconnect();
      },

      streamUpdated = function(event) {

        switch (event.changedProperty) {
          case 'videoDimensions':
            if (!_streamContainer) {
              // Ignore videoEmension updates before streamContainer is created OPENTOK-17253
              break;
            }
            _streamContainer.orientation({
              width: event.newValue.width,
              height: event.newValue.height,
              videoOrientation: event.newValue.orientation
            });

            this.dispatchEvent(new OT.VideoDimensionsChangedEvent(
              this, event.oldValue, event.newValue
            ));

            break;

          case 'videoDisableWarning':
            _chrome.videoDisabledIndicator.setWarning(event.newValue);
            this.dispatchEvent(new OT.VideoDisableWarningEvent(
              event.newValue ? 'videoDisableWarning' : 'videoDisableWarningLifted'
            ));
            break;

          case 'hasVideo':

            setAudioOnly(!(_stream.hasVideo && _properties.subscribeToVideo));

            this.dispatchEvent(new OT.VideoEnabledChangedEvent(
              _stream.hasVideo ? 'videoEnabled' : 'videoDisabled', {
              reason: 'publishVideo'
            }));
            break;

          case 'hasAudio':
          // noop
        }
      },

      /// Chrome

      // If mode is false, then that is the mode. If mode is true then we'll
      // definitely display  the button, but we'll defer the model to the
      // Publishers buttonDisplayMode style property.
      chromeButtonMode = function(mode) {
        if (mode === false) return 'off';

        var defaultMode = this.getStyle('buttonDisplayMode');

        // The default model is false, but it's overridden by +mode+ being true
        if (defaultMode === false) return 'on';

        // defaultMode is either true or auto.
        return defaultMode;
      },

      updateChromeForStyleChange = function(key, value/*, oldValue*/) {
        if (!_chrome) return;

        switch (key) {
          case 'nameDisplayMode':
            _chrome.name.setDisplayMode(value);
            _chrome.backingBar.setNameMode(value);
            break;

          case 'videoDisabledDisplayMode':
            _chrome.videoDisabledIndicator.setDisplayMode(value);
            break;

          case 'showArchiveStatus':
            _chrome.archive.setShowArchiveStatus(value);
            break;

          case 'buttonDisplayMode':
            _chrome.muteButton.setDisplayMode(value);
            _chrome.backingBar.setMuteMode(value);
            break;

          case 'audioLevelDisplayMode':
            _chrome.audioLevel.setDisplayMode(value);
            break;

          case 'bugDisplayMode':
            // bugDisplayMode can't be updated but is used by some partners

          case 'backgroundImageURI':
            _container.setBackgroundImageURI(value);
        }
      },

      _createChrome = function() {

        var widgets = {
          backingBar: new OT.Chrome.BackingBar({
            nameMode: !_properties.name ? 'off' : this.getStyle('nameDisplayMode'),
            muteMode: chromeButtonMode.call(this, this.getStyle('showMuteButton'))
          }),

          name: new OT.Chrome.NamePanel({
            name: _properties.name,
            mode: this.getStyle('nameDisplayMode')
          }),

          muteButton: new OT.Chrome.MuteButton({
            muted: _properties.muted,
            mode: chromeButtonMode.call(this, this.getStyle('showMuteButton'))
          }),

          archive: new OT.Chrome.Archiving({
            show: this.getStyle('showArchiveStatus'),
            archiving: false
          })
        };

        if (_audioLevelCapable) {
          var audioLevelTransformer = new OT.AudioLevelTransformer();

          var audioLevelUpdatedHandler = function(evt) {
            _audioLevelMeter.setValue(audioLevelTransformer.transform(evt.audioLevel));
          };

          _audioLevelMeter = new OT.Chrome.AudioLevelMeter({
            mode: this.getStyle('audioLevelDisplayMode'),
            onActivate: function() {
              _subscriber.on('audioLevelUpdated', audioLevelUpdatedHandler);
            },
            onPassivate: function() {
              _subscriber.off('audioLevelUpdated', audioLevelUpdatedHandler);
            }
          });

          widgets.audioLevel = _audioLevelMeter;
        }

        widgets.videoDisabledIndicator = new OT.Chrome.VideoDisabledIndicator({
          mode: this.getStyle('videoDisabledDisplayMode')
        });

        _chrome = new OT.Chrome({
          parent: _container.domElement
        }).set(widgets).on({
          muted: function() {
            muteAudio.call(this, true);
          },

          unmuted: function() {
            muteAudio.call(this, false);
          }
        }, this);

        // Hide the chrome until we explicitly show it
        _chrome.hideWhileLoading();
      },

      _showError = function() {
        // Display the error message inside the container, assuming it's
        // been created by now.
        if (_container) {
          _container.addError(
            'The stream was unable to connect due to a network error.',
            'Make sure your connection isn\'t blocked by a firewall.'
          );
        }
      };

  OT.StylableComponent(this, {
    nameDisplayMode: 'auto',
    buttonDisplayMode: 'auto',
    audioLevelDisplayMode: 'auto',
    videoDisabledDisplayMode: 'auto',
    backgroundImageURI: null,
    showArchiveStatus: true,
    showMicButton: true
  }, _properties.showControls, function(payload) {
    logAnalyticsEvent('SetStyle', 'Subscriber', payload, 0.1);
  });

  var setAudioOnly = function(audioOnly) {
    if (_peerConnection) {
      _peerConnection.subscribeToVideo(!audioOnly);
    }

    if (_container) {
      _container.audioOnly(audioOnly);
      _container.showPoster(audioOnly);
    }

    if (_audioLevelMeter && _subscriber.getStyle('audioLevelDisplayMode') === 'auto') {
      _audioLevelMeter[audioOnly ? 'show' : 'hide']();
    }
  };

  // logs an analytics event for getStats on the first call
  var notifyGetStatsCalled = (function() {
    var callCount = 0;
    return function throttlingNotifyGetStatsCalled() {
      if (callCount === 0) {
        logAnalyticsEvent('GetStats', 'Called');
      }
      callCount++;
    };
  })();

  this.destroy = function(reason, quiet) {
    if (_state.isDestroyed()) return;

    if (_state.isAttemptingToSubscribe()) {
      if (reason === 'streamDestroyed') {
        // We weren't subscribing yet so the stream was destroyed before we setup
        // the PeerConnection or receiving the initial stream.
        this.trigger('subscribeComplete', new OT.Error(null, 'InvalidStreamID'));
      } else {
        logConnectivityEvent('Canceled', {streamId: _stream.id});
        _connectivityAttemptPinger.stop();
      }
    }

    _state.set('Destroyed');

    if (_audioLevelRunner) {
      _audioLevelRunner.stop();
    }

    this.disconnect();

    if (_chrome) {
      _chrome.destroy();
      _chrome = null;
    }

    if (_container) {
      _container.destroy();
      _container = null;
      this.element = null;
    }

    if (_stream && !_stream.destroyed) {
      logAnalyticsEvent('unsubscribe', null, {streamId: _stream.id});
    }

    this.id = _domId = null;
    this.stream = _stream = null;
    this.streamId = null;

    this.session = _session = null;
    _properties = null;

    if (quiet !== true) {
      this.dispatchEvent(
        new OT.DestroyedEvent(
          OT.Event.names.SUBSCRIBER_DESTROYED,
          this,
          reason
        ),
        OT.$.bind(this.off, this)
      );
    }

    return this;
  };

  this.disconnect = function() {
    if (!_state.isDestroyed() && !_state.isFailed()) {
      // If we are already in the destroyed state then disconnect
      // has been called after (or from within) destroy.
      _state.set('NotSubscribing');
    }

    if (_streamContainer) {
      _streamContainer.destroy();
      _streamContainer = null;
    }

    if (_peerConnection) {
      _peerConnection.destroy();
      _peerConnection = null;

      logAnalyticsEvent('disconnect', 'PeerConnection', {streamId: _stream.id});
    }
  };

  this.processMessage = function(type, fromConnection, message) {
    OT.debug('OT.Subscriber.processMessage: Received ' + type + ' message from ' +
      fromConnection.id);
    OT.debug(message);

    if (_fromConnectionId !== fromConnection.id) {
      _fromConnectionId = fromConnection.id;
    }

    if (_peerConnection) {
      _peerConnection.processMessage(type, message);
    }
  };

  this.disableVideo = function(active) {
    if (!active) {
      OT.warn('Due to high packet loss and low bandwidth, video has been disabled');
    } else {
      if (_lastSubscribeToVideoReason === 'auto') {
        OT.info('Video has been re-enabled');
        _chrome.videoDisabledIndicator.disableVideo(false);
      } else {
        OT.info('Video was not re-enabled because it was manually disabled');
        return;
      }
    }
    this.subscribeToVideo(active, 'auto');
    if (!active) {
      _chrome.videoDisabledIndicator.disableVideo(true);
    }
    var payload = active ? {videoEnabled: true} : {videoDisabled: true};
    logAnalyticsEvent('updateQuality', 'video', payload);
  };

  /**
   * Returns the base-64-encoded string of PNG data representing the Subscriber video.
   *
   *  <p>You can use the string as the value for a data URL scheme passed to the src parameter of
   *  an image file, as in the following:</p>
   *
   *  <pre>
   *  var imgData = subscriber.getImgData();
   *
   *  var img = document.createElement("img");
   *  img.setAttribute("src", "data:image/png;base64," + imgData);
   *  var imgWin = window.open("about:blank", "Screenshot");
   *  imgWin.document.write("&lt;body&gt;&lt;/body&gt;");
   *  imgWin.document.body.appendChild(img);
   *  </pre>
   * @method #getImgData
   * @memberOf Subscriber
   * @return {String} The base-64 encoded string. Returns an empty string if there is no video.
   */
  this.getImgData = function() {
    if (!this.isSubscribing()) {
      OT.error('OT.Subscriber.getImgData: Cannot getImgData before the Subscriber ' +
        'is subscribing.');
      return null;
    }

    return _streamContainer.imgData();
  };

  this.getStats = function(callback) {
    if (!_peerConnection) {
      callback(new OT.$.Error('Subscriber is not connected cannot getStats', 1015));
      return;
    }

    notifyGetStatsCalled();

    _peerConnection.getStats(function(error, stats) {
      if (error) {
        callback(error);
        return;
      }

      var otStats = {
        timestamp: 0
      };

      OT.$.forEach(stats, function(stat) {
        if (OT.getStatsHelpers.isInboundStat(stat)) {
          var video = OT.getStatsHelpers.isVideoStat(stat);
          var audio = OT.getStatsHelpers.isAudioStat(stat);

          // it is safe to override the timestamp of one by another
          // if they are from the same getStats "batch" video and audio ts have the same value
          if (audio || video) {
            otStats.timestamp = OT.getStatsHelpers.normalizeTimestamp(stat.timestamp);
          }
          if (video) {
            otStats.video = OT.getStatsHelpers.parseStatCategory(stat);
          } else if (audio) {
            otStats.audio = OT.getStatsHelpers.parseStatCategory(stat);
          }
        }
      });

      callback(null, otStats);
    });
  };

  /**
   * Sets the audio volume, between 0 and 100, of the Subscriber.
   *
   * <p>You can set the initial volume when you call the <code>Session.subscribe()</code>
   * method. Pass a <code>audioVolume</code> property of the <code>properties</code> parameter
   * of the method.</p>
   *
   * @param {Number} value The audio volume, between 0 and 100.
   *
   * @return {Subscriber} The Subscriber object. This lets you chain method calls, as in the
   * following:
   *
   * <pre>mySubscriber.setAudioVolume(50).setStyle(newStyle);</pre>
   *
   * @see <a href="#getAudioVolume">getAudioVolume()</a>
   * @see <a href="Session.html#subscribe">Session.subscribe()</a>
   * @method #setAudioVolume
   * @memberOf Subscriber
   */
  this.setAudioVolume = function(value) {
    value = parseInt(value, 10);
    if (isNaN(value)) {
      OT.error('OT.Subscriber.setAudioVolume: value should be an integer between 0 and 100');
      return this;
    }
    _audioVolume = Math.max(0, Math.min(100, value));
    if (_audioVolume !== value) {
      OT.warn('OT.Subscriber.setAudioVolume: value should be an integer between 0 and 100');
    }
    if (_properties.muted && _audioVolume > 0) {
      _properties.premuteVolume = value;
      muteAudio.call(this, false);
    }
    if (_streamContainer) {
      _streamContainer.setAudioVolume(_audioVolume);
    }
    return this;
  };

  /**
  * Returns the audio volume, between 0 and 100, of the Subscriber.
  *
  * <p>Generally you use this method in conjunction with the <code>setAudioVolume()</code>
  * method.</p>
  *
  * @return {Number} The audio volume, between 0 and 100, of the Subscriber.
  * @see <a href="#setAudioVolume">setAudioVolume()</a>
  * @method #getAudioVolume
  * @memberOf Subscriber
  */
  this.getAudioVolume = function() {
    if (_properties.muted) {
      return 0;
    }
    if (_streamContainer) return _streamContainer.getAudioVolume();
    else return _audioVolume;
  };

  /**
  * Toggles audio on and off. Starts subscribing to audio (if it is available and currently
  * not being subscribed to) when the <code>value</code> is <code>true</code>; stops
  * subscribing to audio (if it is currently being subscribed to) when the <code>value</code>
  * is <code>false</code>.
  * <p>
  * <i>Note:</i> This method only affects the local playback of audio. It has no impact on the
  * audio for other connections subscribing to the same stream. If the Publsher is not
  * publishing audio, enabling the Subscriber audio will have no practical effect.
  * </p>
  *
  * @param {Boolean} value Whether to start subscribing to audio (<code>true</code>) or not
  * (<code>false</code>).
  *
  * @return {Subscriber} The Subscriber object. This lets you chain method calls, as in the
  * following:
  *
  * <pre>mySubscriber.subscribeToAudio(true).subscribeToVideo(false);</pre>
  *
  * @see <a href="#subscribeToVideo">subscribeToVideo()</a>
  * @see <a href="Session.html#subscribe">Session.subscribe()</a>
  * @see <a href="StreamPropertyChangedEvent.html">StreamPropertyChangedEvent</a>
  *
  * @method #subscribeToAudio
  * @memberOf Subscriber
  */
  this.subscribeToAudio = function(pValue) {
    var value = OT.$.castToBoolean(pValue, true);

    if (_peerConnection) {
      _peerConnection.subscribeToAudio(value && !_properties.subscribeMute);

      if (_session && _stream && value !== _properties.subscribeToAudio) {
        _stream.setChannelActiveState('audio', value && !_properties.subscribeMute);
      }
    }

    _properties.subscribeToAudio = value;

    return this;
  };

  var muteAudio = function(_mute) {
    _chrome.muteButton.muted(_mute);

    if (_mute === _properties.mute) {
      return;
    }
    if (OT.$.env.name === 'Chrome' || OTPlugin.isInstalled()) {
      _properties.subscribeMute = _properties.muted = _mute;
      this.subscribeToAudio(_properties.subscribeToAudio);
    } else {
      if (_mute) {
        _properties.premuteVolume = this.getAudioVolume();
        _properties.muted = true;
        this.setAudioVolume(0);
      } else if (_properties.premuteVolume || _properties.audioVolume) {
        _properties.muted = false;
        this.setAudioVolume(_properties.premuteVolume || _properties.audioVolume);
      }
    }
    _properties.mute = _properties.mute;
  };

  var reasonMap = {
    auto: 'quality',
    publishVideo: 'publishVideo',
    subscribeToVideo: 'subscribeToVideo'
  };

  /**
  * Toggles video on and off. Starts subscribing to video (if it is available and
  * currently not being subscribed to) when the <code>value</code> is <code>true</code>;
  * stops subscribing to video (if it is currently being subscribed to) when the
  * <code>value</code> is <code>false</code>.
  * <p>
  * <i>Note:</i> This method only affects the local playback of video. It has no impact on
  * the video for other connections subscribing to the same stream. If the Publsher is not
  * publishing video, enabling the Subscriber video will have no practical video.
  * </p>
  *
  * @param {Boolean} value Whether to start subscribing to video (<code>true</code>) or not
  * (<code>false</code>).
  *
  * @return {Subscriber} The Subscriber object. This lets you chain method calls, as in the
  * following:
  *
  * <pre>mySubscriber.subscribeToVideo(true).subscribeToAudio(false);</pre>
  *
  * @see <a href="#subscribeToAudio">subscribeToAudio()</a>
  * @see <a href="Session.html#subscribe">Session.subscribe()</a>
  * @see <a href="StreamPropertyChangedEvent.html">StreamPropertyChangedEvent</a>
  *
  * @method #subscribeToVideo
  * @memberOf Subscriber
  */
  this.subscribeToVideo = function(pValue, reason) {
    var value = OT.$.castToBoolean(pValue, true);

    setAudioOnly(!(value && _stream.hasVideo));

    if (value && _container && _container.video() && _stream.hasVideo) {
      _container.loading(value);
      _container.video().whenTimeIncrements(function() {
        _container.loading(false);
      }, this);
    }

    if (_chrome && _chrome.videoDisabledIndicator) {
      _chrome.videoDisabledIndicator.disableVideo(false);
    }

    if (_peerConnection) {
      if (_session && _stream && (value !== _properties.subscribeToVideo ||
          reason !== _lastSubscribeToVideoReason)) {
        _stream.setChannelActiveState('video', value, reason);
      }
    }

    _properties.subscribeToVideo = value;
    _lastSubscribeToVideoReason = reason;

    if (reason !== 'loading') {
      this.dispatchEvent(new OT.VideoEnabledChangedEvent(
        value ? 'videoEnabled' : 'videoDisabled',
        {
          reason: reasonMap[reason] || 'subscribeToVideo'
        }
      ));
    }

    return this;
  };

  this.setPreferredResolution = function(preferredResolution) {
    if (_state.isDestroyed() || !_peerConnection) {
      OT.warn('Cannot set the max Resolution when not subscribing to a publisher');
      return;
    }

    _properties.preferredResolution = preferredResolution;

    if (_session.sessionInfo.p2pEnabled) {
      OT.warn('Subscriber.setPreferredResolution will not work in a P2P Session');
      return;
    }

    var curMaxResolution = _stream.getPreferredResolution();

    var isUnchanged = (curMaxResolution && preferredResolution &&
                        curMaxResolution.width ===  preferredResolution.width &&
                        curMaxResolution.height ===  preferredResolution.height) ||
                      (!curMaxResolution && !preferredResolution);

    if (isUnchanged) {
      return;
    }

    _stream.setPreferredResolution(preferredResolution);
  };

  this.setPreferredFrameRate = function(preferredFrameRate) {
    if (_state.isDestroyed() || !_peerConnection) {
      OT.warn('Cannot set the max frameRate when not subscribing to a publisher');
      return;
    }

    _properties.preferredFrameRate = preferredFrameRate;

    if (_session.sessionInfo.p2pEnabled) {
      OT.warn('Subscriber.setPreferredFrameRate will not work in a P2P Session');
      return;
    }

    /* jshint -W116 */
    if (_stream.getPreferredFrameRate() == preferredFrameRate) {
      return;
    }
    /* jshint +W116 */

    _stream.setPreferredFrameRate(preferredFrameRate);
  };

  this.isSubscribing = function() {
    return _state.isSubscribing();
  };

  this.isWebRTC = true;

  this.isLoading = function() {
    return _container && _container.loading();
  };

  this.videoElement = function() {
    return _streamContainer.domElement();
  };

  /**
  * Returns the width, in pixels, of the Subscriber video.
  *
  * @method #videoWidth
  * @memberOf Subscriber
  * @return {Number} the width, in pixels, of the Subscriber video.
  */
  this.videoWidth = function() {
    return _streamContainer.videoWidth();
  };

  /**
  * Returns the height, in pixels, of the Subscriber video.
  *
  * @method #videoHeight
  * @memberOf Subscriber
  * @return {Number} the width, in pixels, of the Subscriber video.
  */
  this.videoHeight = function() {
    return _streamContainer.videoHeight();
  };

  /**
  * Restricts the frame rate of the Subscriber's video stream, when you pass in
  * <code>true</code>. When you pass in <code>false</code>, the frame rate of the video stream
  * is not restricted.
  * <p>
  * When the frame rate is restricted, the Subscriber video frame will update once or less per
  * second.
  * <p>
  * This feature is only available in sessions that use the OpenTok Media Router (sessions with
  * the <a href="http://tokbox.com/opentok/tutorials/create-session/#media-mode">media mode</a>
  * set to routed), not in sessions with the media mode set to relayed. In relayed sessions,
  * calling this method has no effect.
  * <p>
  * Restricting the subscriber frame rate has the following benefits:
  * <ul>
  *    <li>It reduces CPU usage.</li>
  *    <li>It reduces the network bandwidth consumed.</li>
  *    <li>It lets you subscribe to more streams simultaneously.</li>
  * </ul>
  * <p>
  * Reducing a subscriber's frame rate has no effect on the frame rate of the video in
  * other clients.
  *
  * @param {Boolean} value Whether to restrict the Subscriber's video frame rate
  * (<code>true</code>) or not (<code>false</code>).
  *
  * @return {Subscriber} The Subscriber object. This lets you chain method calls, as in the
  * following:
  *
  * <pre>mySubscriber.restrictFrameRate(false).subscribeToAudio(true);</pre>
  *
  * @method #restrictFrameRate
  * @memberOf Subscriber
  */
  this.restrictFrameRate = function(val) {
    OT.debug('OT.Subscriber.restrictFrameRate(' + val + ')');

    logAnalyticsEvent('restrictFrameRate', val.toString(), {streamId: _stream.id});

    if (_session.sessionInfo.p2pEnabled) {
      OT.warn('OT.Subscriber.restrictFrameRate: Cannot restrictFrameRate on a P2P session');
    }

    if (typeof val !== 'boolean') {
      OT.error('OT.Subscriber.restrictFrameRate: expected a boolean value got a ' + typeof val);
    } else {
      _frameRateRestricted = val;
      _stream.setRestrictFrameRate(val);
    }
    return this;
  };

  this.on('styleValueChanged', updateChromeForStyleChange, this);

  this._ = {
    archivingStatus: function(status) {
      if (_chrome) {
        _chrome.archive.setArchiving(status);
      }
    },

    getDataChannel: function(label, options, completion) {
      // @fixme this will fail if it's called before we have a SubscriberPeerConnection.
      // I.e. before we have a publisher connection.
      if (!_peerConnection) {
        completion(
          new OT.$.Error('Cannot create a DataChannel before there is a publisher connection.')
        );

        return;
      }

      _peerConnection.getDataChannel(label, options, completion);
    }
  };

  _state = new OT.SubscribingState(stateChangeFailed);

  OT.debug('OT.Subscriber: subscribe to ' + _stream.id);

  _state.set('Init');

  if (!_stream) {
    // @todo error
    OT.error('OT.Subscriber: No stream parameter.');
    return false;
  }

  _stream.on({
    updated: streamUpdated,
    destroyed: streamDestroyed
  }, this);

  _fromConnectionId = _stream.connection.id;
  _properties.name = _properties.name || _stream.name;
  _properties.classNames = 'OT_root OT_subscriber';

  if (_properties.style) {
    this.setStyle(_properties.style, null, true);
  }
  if (_properties.audioVolume) {
    this.setAudioVolume(_properties.audioVolume);
  }

  _properties.subscribeToAudio = OT.$.castToBoolean(_properties.subscribeToAudio, true);
  _properties.subscribeToVideo = OT.$.castToBoolean(_properties.subscribeToVideo, true);

  _container = new OT.WidgetView(targetElement, _properties);
  this.id = _domId = _container.domId();
  this.element = _container.domElement;

  _createChrome.call(this);

  _startConnectingTime = OT.$.now();

  logAnalyticsEvent('createPeerConnection', 'Attempt', '');

  _isLocalStream = _stream.connection.id === _session.connection.id;

  if (!_properties.testNetwork && _isLocalStream) {
    // Subscribe to yourself edge-case
    var publisher = _session.getPublisherForStream(_stream);
    if (!(publisher && publisher._.webRtcStream())) {
      this.trigger('subscribeComplete', new OT.Error(null, 'InvalidStreamID'));
      return this;
    }

    onRemoteStreamAdded.call(this, publisher._.webRtcStream());
  } else {
    if (_properties.testNetwork) {
      this.setAudioVolume(0);
    }

    _state.set('ConnectingToPeer');

    _peerConnection = new OT.SubscriberPeerConnection(_stream.connection, _session,
      _stream, this, _properties);

    _peerConnection.on({
      disconnected: onDisconnected,
      error: onPeerConnectionFailure,
      remoteStreamAdded: onRemoteStreamAdded,
      remoteStreamRemoved: onRemoteStreamRemoved,
      qos: recordQOS
    }, this);

    // initialize the peer connection AFTER we've added the event listeners
    _peerConnection.init(function(err) {
      if (err) {
        throw err;
      }
    });

    if (OT.$.hasCapabilities('audioOutputLevelStat')) {
      _audioLevelSampler = new OT.GetStatsAudioLevelSampler(_peerConnection, 'out');
    } else if (OT.$.hasCapabilities('webAudioCapableRemoteStream')) {
      _audioLevelSampler = new OT.AnalyserAudioLevelSampler(OT.audioContext());
    }

    if (_audioLevelSampler) {
      var subscriber = this;
      // sample with interval to minimise disturbance on animation loop but dispatch the
      // event with RAF since the main purpose is animation of a meter
      _audioLevelRunner = new OT.IntervalRunner(function() {
        _audioLevelSampler.sample(function(audioOutputLevel) {
          if (audioOutputLevel !== null) {
            OT.$.requestAnimationFrame(function() {
              subscriber.dispatchEvent(
                new OT.AudioLevelUpdatedEvent(audioOutputLevel));
            });
          }
        });
      }, 60);
    }

  }

  logConnectivityEvent('Attempt', {streamId: _stream.id});

  /**
  * Dispatched periodically to indicate the subscriber's audio level. The event is dispatched
  * up to 60 times per second, depending on the browser. The <code>audioLevel</code> property
  * of the event is audio level, from 0 to 1.0. See {@link AudioLevelUpdatedEvent} for more
  * information.
  * <p>
  * The following example adjusts the value of a meter element that shows volume of the
  * subscriber. Note that the audio level is adjusted logarithmically and a moving average
  * is applied:
  * <pre>
  * var movingAvg = null;
  * subscriber.on('audioLevelUpdated', function(event) {
  *   if (movingAvg === null || movingAvg <= event.audioLevel) {
  *     movingAvg = event.audioLevel;
  *   } else {
  *     movingAvg = 0.7 * movingAvg + 0.3 * event.audioLevel;
  *   }
  *
  *   // 1.5 scaling to map the -30 - 0 dBm range to [0,1]
  *   var logLevel = (Math.log(movingAvg) / Math.LN10) / 1.5 + 1;
  *   logLevel = Math.min(Math.max(logLevel, 0), 1);
  *   document.getElementById('subscriberMeter').value = logLevel;
  * });
  * </pre>
  * <p>This example shows the algorithm used by the default audio level indicator displayed
  * in an audio-only Subscriber.
  *
  * @name audioLevelUpdated
  * @event
  * @memberof Subscriber
  * @see AudioLevelUpdatedEvent
  */

  /**
  * Dispatched when the video for the subscriber is disabled.
  * <p>
  * The <code>reason</code> property defines the reason the video was disabled. This can be set to
  * one of the following values:
  * <p>
  *
  * <ul>
  *
  *   <li><code>"publishVideo"</code> &mdash; The publisher stopped publishing video by calling
  *   <code>publishVideo(false)</code>.</li>
  *
  *   <li><code>"quality"</code> &mdash; The OpenTok Media Router stopped sending video
  *   to the subscriber based on stream quality changes. This feature of the OpenTok Media
  *   Router has a subscriber drop the video stream when connectivity degrades. (The subscriber
  *   continues to receive the audio stream, if there is one.)
  *   <p>
  *   Before sending this event, when the Subscriber's stream quality deteriorates to a level
  *   that is low enough that the video stream is at risk of being disabled, the Subscriber
  *   dispatches a <code>videoDisableWarning</code> event.
  *   <p>
  *   If connectivity improves to support video again, the Subscriber object dispatches
  *   a <code>videoEnabled</code> event, and the Subscriber resumes receiving video.
  *   <p>
  *   By default, the Subscriber displays a video disabled indicator when a
  *   <code>videoDisabled</code> event with this reason is dispatched and removes the indicator
  *   when the <code>videoDisabled</code> event with this reason is dispatched. You can control
  *   the display of this icon by calling the <code>setStyle()</code> method of the Subscriber,
  *   setting the <code>videoDisabledDisplayMode</code> property(or you can set the style when
  *   calling the <code>Session.subscribe()</code> method, setting the <code>style</code> property
  *   of the <code>properties</code> parameter).
  *   <p>
  *   This feature is only available in sessions that use the OpenTok Media Router (sessions with
  *   the <a href="http://tokbox.com/opentok/tutorials/create-session/#media-mode">media mode</a>
  *   set to routed), not in sessions with the media mode set to relayed.
  *   <p>
  *   You can disable this audio-only fallback feature, by setting the
  *   <code>audioFallbackEnabled</code> property to <code>false</code> in the options you pass
  *   into the <code>OT.initPublisher()</code> method on the publishing client. (See
  *   <a href="OT.html#initPublisher">OT.initPublisher()</a>.)
  *   </li>
  *
  *   <li><code>"subscribeToVideo"</code> &mdash; The subscriber started or stopped subscribing to
  *   video, by calling <code>subscribeToVideo(false)</code>.
  *   </li>
  *
  * </ul>
  *
  * @see VideoEnabledChangedEvent
  * @see event:videoDisableWarning
  * @see event:videoEnabled
  * @name videoDisabled
  * @event
  * @memberof Subscriber
*/

  /**
  * Dispatched when the OpenTok Media Router determines that the stream quality has degraded
  * and the video will be disabled if the quality degrades more. If the quality degrades further,
  * the Subscriber disables the video and dispatches a <code>videoDisabled</code> event.
  * <p>
  * By default, the Subscriber displays a video disabled warning indicator when this event
  * is dispatched (and the video is disabled). You can control the display of this icon by
  * calling the <code>setStyle()</code> method and setting the
  * <code>videoDisabledDisplayMode</code> property (or you can set the style when calling
  * the <code>Session.subscribe()</code> method and setting the <code>style</code> property
  * of the <code>properties</code> parameter).
  * <p>
  * This feature is only available in sessions that use the OpenTok Media Router (sessions with
  * the <a href="http://tokbox.com/opentok/tutorials/create-session/#media-mode">media mode</a>
  * set to routed), not in sessions with the media mode set to relayed.
  *
  * @see Event
  * @see event:videoDisabled
  * @see event:videoDisableWarningLifted
  * @name videoDisableWarning
  * @event
  * @memberof Subscriber
*/

  /**
  * Dispatched when the OpenTok Media Router determines that the stream quality has improved
  * to the point at which the video being disabled is not an immediate risk. This event is
  * dispatched after the Subscriber object dispatches a <code>videoDisableWarning</code> event.
  * <p>
  * This feature is only available in sessions that use the OpenTok Media Router (sessions with
  * the <a href="http://tokbox.com/opentok/tutorials/create-session/#media-mode">media mode</a>
  * set to routed), not in sessions with the media mode set to relayed.
  *
  * @see Event
  * @see event:videoDisabled
  * @see event:videoDisableWarning
  * @name videoDisableWarningLifted
  * @event
  * @memberof Subscriber
*/

  /**
  * Dispatched when the OpenTok Media Router resumes sending video to the subscriber
  * after video was previously disabled.
  * <p>
  * The <code>reason</code> property defines the reason the video was enabled. This can be set to
  * one of the following values:
  * <p>
  *
  * <ul>
  *
  *   <li><code>"publishVideo"</code> &mdash; The publisher started publishing video by calling
  *   <code>publishVideo(true)</code>.</li>
  *
  *   <li><code>"quality"</code> &mdash; The OpenTok Media Router resumed sending video
  *   to the subscriber based on stream quality changes. This feature of the OpenTok Media
  *   Router has a subscriber drop the video stream when connectivity degrades and then resume
  *   the video stream if the stream quality improves.
  *   <p>
  *   This feature is only available in sessions that use the OpenTok Media Router (sessions with
  *   the <a href="http://tokbox.com/opentok/tutorials/create-session/#media-mode">media mode</a>
  *   set to routed), not in sessions with the media mode set to relayed.
  *   </li>
  *
  *   <li><code>"subscribeToVideo"</code> &mdash; The subscriber started or stopped subscribing to
  *   video, by calling <code>subscribeToVideo(false)</code>.
  *   </li>
  *
  * </ul>
  *
  * <p>
  * To prevent video from resuming, in the <code>videoEnabled</code> event listener,
  * call <code>subscribeToVideo(false)</code> on the Subscriber object.
  *
  * @see VideoEnabledChangedEvent
  * @see event:videoDisabled
  * @name videoEnabled
  * @event
  * @memberof Subscriber
*/

  /**
  * Dispatched when the Subscriber element is removed from the HTML DOM. When this event is
  * dispatched, you may choose to adjust or remove HTML DOM elements related to the subscriber.
  * @see Event
  * @name destroyed
  * @event
  * @memberof Subscriber
*/

  /**
  * Dispatched when the video dimensions of the video change. This can occur when the
  * <code>stream.videoType</code> property is set to <code>"screen"</code> (for a screen-sharing
  * video stream), and the user resizes the window being captured. It can also occur if the video
  * is being published by a mobile device and the user rotates the device (causing the camera
  * orientation to change).
  * @name videoDimensionsChanged
  * @event
  * @memberof Subscriber
*/

};

// tb_require('../helpers/helpers.js')
// tb_require('../helpers/lib/config.js')
// tb_require('./analytics.js')
// tb_require('./events.js')
// tb_require('./error_handling.js')
// tb_require('./system_requirements.js')
// tb_require('./stream.js')
// tb_require('./connection.js')
// tb_require('./environment_loader.js')
// tb_require('./session_info.js')
// tb_require('./messaging/raptor/raptor.js')
// tb_require('./messaging/raptor/session-dispatcher.js')
// tb_require('./qos_testing/webrtc_test.js')
// tb_require('./qos_testing/http_test.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

/**
 * The Session object returned by the <code>OT.initSession()</code> method provides access to
 * much of the OpenTok functionality.
 *
 * @class Session
 * @augments EventDispatcher
 *
 * @property {Capabilities} capabilities A {@link Capabilities} object that includes information
 * about the capabilities of the client. All properties of the <code>capabilities</code> object
 * are undefined until you have connected to a session and the Session object has dispatched the
 * <code>sessionConnected</code> event.
 * @property {Connection} connection The {@link Connection} object for this session. The
 * connection property is only available once the Session object dispatches the sessionConnected
 * event. The Session object asynchronously dispatches a sessionConnected event in response
 * to a successful call to the connect() method. See: <a href="#connect">connect</a> and
 * {@link Connection}.
 * @property {String} sessionId The session ID for this session. You pass this value into the
 * <code>OT.initSession()</code> method when you create the Session object. (Note: a Session
 * object is not connected to the OpenTok server until you call the connect() method of the
 * object and the object dispatches a connected event. See {@link OT.initSession} and
 * {@link connect}).
 *  For more information on sessions and session IDs, see
 * <a href="https://tokbox.com/opentok/tutorials/create-session/">Session creation</a>.
 */
OT.Session = function(apiKey, sessionId) {
  OT.$.eventing(this);

  // Check that the client meets the minimum requirements, if they don't the upgrade
  // flow will be triggered.
  if (!OT.checkSystemRequirements()) {
    OT.upgradeSystemRequirements();
    return;
  }

  if (sessionId == null) {
    sessionId = apiKey;
    apiKey = null;
  }

  this.id = this.sessionId = sessionId;

  var _initialConnection = true,
      _apiKey = apiKey,
      _token,
      _session = this,
      _sessionId = sessionId,
      _socket,
      _widgetId = OT.$.uuid(),
      _connectionId = OT.$.uuid(),
      sessionConnectFailed,
      sessionDisconnectedHandler,
      connectionCreatedHandler,
      connectionDestroyedHandler,
      streamCreatedHandler,
      streamPropertyModifiedHandler,
      streamDestroyedHandler,
      archiveCreatedHandler,
      archiveDestroyedHandler,
      archiveUpdatedHandler,
      init,
      reset,
      disconnectComponents,
      destroyPublishers,
      destroySubscribers,
      connectMessenger,
      getSessionInfo,
      onSessionInfoResponse,
      permittedTo,
      _connectivityAttemptPinger,
      dispatchError;

  var setState = OT.$.statable(this, [
    'disconnected', 'connecting', 'connected', 'disconnecting'
  ], 'disconnected');

  this.connection = null;
  this.connections = new OT.$.Collection();
  this.streams = new OT.$.Collection();
  this.archives = new OT.$.Collection();

  //--------------------------------------
  //  MESSAGE HANDLERS
  //--------------------------------------

  // The duplication of this and sessionConnectionFailed will go away when
  // session and messenger are refactored
  sessionConnectFailed = function(reason, code) {
    setState('disconnected');

    OT.error(reason);

    this.trigger('sessionConnectFailed',
      new OT.Error(code || OT.ExceptionCodes.CONNECT_FAILED, reason));

    OT.handleJsException(reason, code || OT.ExceptionCodes.CONNECT_FAILED, {
      session: this
    });
  };

  sessionDisconnectedHandler = function(event) {
    var reason = event.reason;
    if (reason === 'networkTimedout') {
      reason = 'networkDisconnected';
      this.logEvent('Connect', 'TimeOutDisconnect', {reason: event.reason});
    } else {
      this.logEvent('Connect', 'Disconnected', {reason: event.reason});
    }

    var publicEvent = new OT.SessionDisconnectEvent('sessionDisconnected', reason);

    reset();
    disconnectComponents.call(this, reason);

    var defaultAction = OT.$.bind(function() {
      // Publishers handle preventDefault'ing themselves
      destroyPublishers.call(this, publicEvent.reason);
      // Subscriers don't, destroy 'em if needed
      if (!publicEvent.isDefaultPrevented()) destroySubscribers.call(this, publicEvent.reason);
    }, this);

    this.dispatchEvent(publicEvent, defaultAction);
  };

  connectionCreatedHandler = function(connection) {
    // We don't broadcast events for the symphony connection
    if (connection.id.match(/^symphony\./)) return;

    this.dispatchEvent(new OT.ConnectionEvent(
      OT.Event.names.CONNECTION_CREATED,
      connection
    ));
  };

  connectionDestroyedHandler = function(connection, reason) {
    // We don't broadcast events for the symphony connection
    if (connection.id.match(/^symphony\./)) return;

    // Don't delete the connection if it's ours. This only happens when
    // we're about to receive a session disconnected and session disconnected
    // will also clean up our connection.
    if (connection.id === _socket.id()) return;

    this.dispatchEvent(
      new OT.ConnectionEvent(
        OT.Event.names.CONNECTION_DESTROYED,
        connection,
        reason
      )
    );
  };

  streamCreatedHandler = function(stream) {
    if (stream.connection.id !== this.connection.id) {
      this.dispatchEvent(new OT.StreamEvent(
        OT.Event.names.STREAM_CREATED,
        stream,
        null,
        false
      ));
    }
  };

  streamPropertyModifiedHandler = function(event) {
    var stream = event.target,
        propertyName = event.changedProperty,
        newValue = event.newValue;

    if (propertyName === 'videoDisableWarning' || propertyName === 'audioDisableWarning') {
      return; // These are not public properties, skip top level event for them.
    }

    if (propertyName === 'videoDimensions') {
      newValue = {width: newValue.width, height: newValue.height};
    }

    this.dispatchEvent(new OT.StreamPropertyChangedEvent(
      OT.Event.names.STREAM_PROPERTY_CHANGED,
      stream,
      propertyName,
      event.oldValue,
      newValue
    ));
  };

  streamDestroyedHandler = function(stream, reason) {

    // if the stream is one of ours we delegate handling
    // to the publisher itself.
    if (stream.connection.id === this.connection.id) {
      OT.$.forEach(OT.publishers.where({ streamId: stream.id }), OT.$.bind(function(publisher) {
        publisher._.unpublishFromSession(this, reason);
      }, this));
      return;
    }

    var event = new OT.StreamEvent('streamDestroyed', stream, reason, true);

    var defaultAction = OT.$.bind(function() {
      if (!event.isDefaultPrevented()) {
        // If we are subscribed to any of the streams we should unsubscribe
        OT.$.forEach(OT.subscribers.where({streamId: stream.id}), function(subscriber) {
          if (subscriber.session.id === this.id) {
            if (subscriber.stream) {
              subscriber.destroy('streamDestroyed');
            }
          }
        }, this);
      }
      // @TODO Add a else with a one time warning that this no longer cleans up the publisher
    }, this);

    this.dispatchEvent(event, defaultAction);
  };

  archiveCreatedHandler = function(archive) {
    this.dispatchEvent(new OT.ArchiveEvent('archiveStarted', archive));
  };

  archiveDestroyedHandler = function(archive) {
    this.dispatchEvent(new OT.ArchiveEvent('archiveDestroyed', archive));
  };

  archiveUpdatedHandler = function(event) {
    var archive = event.target,
      propertyName = event.changedProperty,
      newValue = event.newValue;

    if (propertyName === 'status' && newValue === 'stopped') {
      this.dispatchEvent(new OT.ArchiveEvent('archiveStopped', archive));
    } else {
      this.dispatchEvent(new OT.ArchiveEvent('archiveUpdated', archive));
    }
  };

  init = function() {
    _session.token = _token = null;
    setState('disconnected');
    _session.connection = null;
    _session.capabilities = new OT.Capabilities([]);
    _session.connections.destroy();
    _session.streams.destroy();
    _session.archives.destroy();
  };

  // Put ourselves into a pristine state
  reset = function() {
    // reset connection id now so that following calls to testNetwork and connect will share
    // the same new session id. We need to reset here because testNetwork might be call after
    // and it is always called before the session is connected
    // on initial connection we don't reset
    _connectionId = OT.$.uuid();
    init();
  };

  disconnectComponents = function(reason) {
    OT.$.forEach(OT.publishers.where({session: this}), function(publisher) {
      publisher.disconnect(reason);
    });

    OT.$.forEach(OT.subscribers.where({session: this}), function(subscriber) {
      subscriber.disconnect();
    });
  };

  destroyPublishers = function(reason) {
    OT.$.forEach(OT.publishers.where({session: this}), function(publisher) {
      publisher._.streamDestroyed(reason);
    });
  };

  destroySubscribers = function(reason) {
    OT.$.forEach(OT.subscribers.where({session: this}), function(subscriber) {
      subscriber.destroy(reason);
    });
  };

  connectMessenger = function() {
    OT.debug('OT.Session: connecting to Raptor');

    var socketUrl = this.sessionInfo.messagingURL,
        symphonyUrl = OT.properties.symphonyAddresss || this.sessionInfo.symphonyAddress;

    _socket = new OT.Raptor.Socket(_connectionId, _widgetId, socketUrl, symphonyUrl,
      OT.SessionDispatcher(this));

    _socket.connect(_token, this.sessionInfo, OT.$.bind(function(error, sessionState) {
      if (error) {
        _socket = void 0;
        this.logConnectivityEvent('Failure', error);

        sessionConnectFailed.call(this, error.message, error.code);
        return;
      }

      OT.debug('OT.Session: Received session state from Raptor', sessionState);

      this.connection = this.connections.get(_socket.id());
      if (this.connection) {
        this.capabilities = this.connection.permissions;
      }

      setState('connected');

      this.logConnectivityEvent('Success', null, {connectionId: this.connection.id});

      // Listen for our own connection's destroyed event so we know when we've been disconnected.
      this.connection.on('destroyed', sessionDisconnectedHandler, this);

      // Listen for connection updates
      this.connections.on({
        add: connectionCreatedHandler,
        remove: connectionDestroyedHandler
      }, this);

      // Listen for stream updates
      this.streams.on({
        add: streamCreatedHandler,
        remove: streamDestroyedHandler,
        update: streamPropertyModifiedHandler
      }, this);

      this.archives.on({
        add: archiveCreatedHandler,
        remove: archiveDestroyedHandler,
        update: archiveUpdatedHandler
      }, this);

      this.dispatchEvent(
        new OT.SessionConnectEvent(OT.Event.names.SESSION_CONNECTED), OT.$.bind(function() {
          this.connections._triggerAddEvents(); // { id: this.connection.id }
          this.streams._triggerAddEvents(); // { id: this.stream.id }
          this.archives._triggerAddEvents();
        }, this)
      );

    }, this));
  };

  getSessionInfo = function() {
    if (this.is('connecting')) {
      var session = this;
      this.logEvent('SessionInfo', 'Attempt');

      OT.SessionInfo.get(sessionId, _token).then(
        onSessionInfoResponse,
        function(error) {
          session.logConnectivityEvent('Failure', {
            reason:'GetSessionInfo',
            code: (error.code || 'No code'),
            message: error.message
          });

          sessionConnectFailed.call(session,
                error.message + (error.code ? ' (' + error.code + ')' : ''),
                error.code);
        }
      );
    }
  };

  onSessionInfoResponse = OT.$.bind(function(sessionInfo) {
    if (this.is('connecting')) {
      this.logEvent('SessionInfo', 'Success', {
        messagingServer: sessionInfo.messagingServer
      });

      var overrides = OT.properties.sessionInfoOverrides;
      this.sessionInfo = sessionInfo;
      if (overrides != null && typeof overrides === 'object') {
        this.sessionInfo = OT.$.defaults(overrides, this.sessionInfo);
      }
      if (this.sessionInfo.partnerId && this.sessionInfo.partnerId !== _apiKey) {
        this.apiKey = _apiKey = this.sessionInfo.partnerId;

        var reason = 'Authentication Error: The API key does not match the token or session.';

        var payload = {
          code: OT.ExceptionCodes.AUTHENTICATION_ERROR,
          message: reason
        };
        this.logEvent('Failure', 'SessionInfo', payload);

        sessionConnectFailed.call(this, reason, OT.ExceptionCodes.AUTHENTICATION_ERROR);
      } else {
        connectMessenger.call(this);
      }
    }
  }, this);

  // Check whether we have permissions to perform the action.
  permittedTo = OT.$.bind(function(action) {
    return this.capabilities.permittedTo(action);
  }, this);

  // This is a placeholder until error handling can be rewritten
  dispatchError = OT.$.bind(function(code, message, completionHandler) {
    OT.error(code, message);

    if (completionHandler && OT.$.isFunction(completionHandler)) {
      completionHandler.call(null, new OT.Error(code, message));
    }

    OT.handleJsException(message, code, {
      session: this
    });
  }, this);

  this.logEvent = function(action, variation, payload, options) {
    var event = {
      action: action,
      variation: variation,
      payload: payload,
      sessionId: _sessionId,
      partnerId: _apiKey
    };

    event.connectionId = _connectionId;

    if (options) event = OT.$.extend(options, event);
    OT.analytics.logEvent(event);
  };

  /**
   * @typedef {Object} Stats
   * @property {number} bytesSentPerSecond
   * @property {number} bytesReceivedPerSecond
   * @property {number} packetLossRatio
   * @property {number} rtt
   */

  function getTestNetworkConfig(token) {
    return new OT.$.RSVP.Promise(function(resolve, reject) {
      OT.$.getJSON(
        [OT.properties.apiURL, '/v2/partner/', _apiKey, '/session/', _sessionId, '/connection/',
          _connectionId, '/testNetworkConfig'].join(''),
        {
          headers: {'X-TB-TOKEN-AUTH': token}
        },
        function(errorEvent, response) {
          if (errorEvent) {
            var error = JSON.parse(errorEvent.target.responseText);
            if (error.code === -1) {
              reject(new OT.$.Error('Unexpected HTTP error codes ' +
              errorEvent.target.status, '2001'));
            } else if (error.code === 10001 || error.code === 10002) {
              reject(new OT.$.Error(error.message, '1004'));
            } else {
              reject(new OT.$.Error(error.message, error.code));
            }
          } else {
            resolve(response);
          }
        });
    });
  }

  /**
   * @param {string} token
   * @param {OT.Publisher} publisher
   * @param {function(?OT.$.Error, Stats=)} callback
   */
  this.testNetwork = function(token, publisher, callback) {

    // intercept call to callback to log the result
    var origCallback = callback;
    callback = function loggingCallback(error, stats) {
      if (error) {
        _session.logEvent('TestNetwork', 'Failure', {
          failureCode: error.name || error.message || 'unknown'
        });
      } else {
        _session.logEvent('TestNetwork', 'Success', stats);
      }
      origCallback(error, stats);
    };

    _session.logEvent('TestNetwork', 'Attempt', {});

    if (this.isConnected()) {
      callback(new OT.$.Error('Session connected, cannot test network', 1015));
      return;
    }

    var webRtcStreamPromise = new OT.$.RSVP.Promise(
      function(resolve, reject) {
        var webRtcStream = publisher._.webRtcStream();
        if (webRtcStream) {
          resolve(webRtcStream);
        } else {

          var onAccessAllowed = function() {
            unbind();
            resolve(publisher._.webRtcStream());
          };

          var onPublishComplete = function(error) {
            if (error) {
              unbind();
              reject(error);
            }
          };

          var unbind = function() {
            publisher.off('publishComplete', onPublishComplete);
            publisher.off('accessAllowed', onAccessAllowed);
          };

          publisher.on('publishComplete', onPublishComplete);
          publisher.on('accessAllowed', onAccessAllowed);

        }
      });

    var testConfig;
    var webrtcStats;
    OT.$.RSVP.Promise.all([getTestNetworkConfig(token), webRtcStreamPromise])
      .then(function(values) {
        var webRtcStream = values[1];
        testConfig = values[0];
        return OT.webrtcTest({mediaConfig: testConfig.media, localStream: webRtcStream});
      })
      .then(function(stats) {
        OT.debug('Received stats from webrtcTest: ', stats);
        if (stats.bandwidth < testConfig.media.thresholdBitsPerSecond) {
          return OT.$.RSVP.Promise.reject(new OT.$.Error('The detect bandwidth from the WebRTC ' +
          'stage of the test was not sufficient to run the HTTP stage of the test', 1553));
        }

        webrtcStats = stats;
      })
      .then(function() {
        // run the HTTP test only if the PC test was not extended
        if (!webrtcStats.extended) {
          return OT.httpTest({httpConfig: testConfig.http});
        }
      })
      .then(function(httpStats) {
        var stats = {
          uploadBitsPerSecond: httpStats ? httpStats.uploadBandwidth : webrtcStats.bandwidth,
          downloadBitsPerSecond: httpStats ? httpStats.downloadBandwidth : webrtcStats.bandwidth,
          packetLossRatio: webrtcStats.packetLostRatio,
          roundTripTimeMilliseconds: webrtcStats.roundTripTime
        };
        callback(null, stats);
        // IE8 (ES3 JS engine) requires bracket notation for "catch" keyword
      })['catch'](function(error) {
        callback(error);
      });
  };

  this.logConnectivityEvent = function(variation, payload, options) {
    if (variation === 'Attempt' || !_connectivityAttemptPinger) {
      var pingerOptions = {
        action: 'Connect',
        sessionId: _sessionId,
        partnerId: _apiKey
      };
      if (this.connection && this.connection.id) {
        pingerOptions = event.connectionId = this.connection.id;
      } else if (_connectionId) {
        pingerOptions.connectionId = _connectionId;
      }
      _connectivityAttemptPinger = new OT.ConnectivityAttemptPinger(pingerOptions);
    }
    _connectivityAttemptPinger.setVariation(variation);
    this.logEvent('Connect', variation, payload, options);
  };

  /**
  * Connects to an OpenTok session.
  * <p>
  *  Upon a successful connection, the completion handler (the second parameter of the method) is
  *  invoked without an error object passed in. (If there is an error connecting, the completion
  *  handler is invoked with an error object.) Make sure that you have successfully connected to the
  *  session before calling other methods of the Session object.
  * </p>
  *  <p>
  *    The Session object dispatches a <code>connectionCreated</code> event when any client
  *    (including your own) connects to to the session.
  *  </p>
  *
  *  <h5>
  *  Example
  *  </h5>
  *  <p>
  *  The following code initializes a session and sets up an event listener for when the session
  *  connects:
  *  </p>
  *  <pre>
  *  var apiKey = ""; // Replace with your API key. See https://dashboard.tokbox.com/projects
  *  var sessionID = ""; // Replace with your own session ID.
  *                      // See https://dashboard.tokbox.com/projects
  *  var token = ""; // Replace with a generated token that has been assigned the moderator role.
  *                  // See https://dashboard.tokbox.com/projects
  *
  *  var session = OT.initSession(apiKey, sessionID);
  *  session.on("sessionConnected", function(sessionConnectEvent) {
  *      //
  *  });
  *  session.connect(token);
  *  </pre>
  *  <p>
  *  <p>
  *    In this example, the <code>sessionConnectHandler()</code> function is passed an event
  *    object of type {@link SessionConnectEvent}.
  *  </p>
  *
  *  <h5>
  *  Events dispatched:
  *  </h5>
  *
  *  <p>
  *    <code>exception</code> (<a href="ExceptionEvent.html">ExceptionEvent</a>) &#151; Dispatched
  *    by the OT class locally in the event of an error.
  *  </p>
  *  <p>
  *    <code>connectionCreated</code> (<a href="ConnectionEvent.html">ConnectionEvent</a>) &#151;
  *      Dispatched by the Session object on all clients connected to the session.
  *  </p>
  *  <p>
  *    <code>sessionConnected</code> (<a href="SessionConnectEvent.html">SessionConnectEvent</a>)
  *      &#151; Dispatched locally by the Session object when the connection is established.
  *  </p>
  *
  * @param {String} token The session token. You generate a session token using our
  * <a href="https://tokbox.com/opentok/libraries/server/">server-side libraries</a> or the
  * <a href="https://dashboard.tokbox.com/projects">Dashboard</a> page. For more information, see
  * <a href="https://tokbox.com/opentok/tutorials/create-token/">Connection token creation</a>.
  *
  * @param {Function} completionHandler (Optional) A function to be called when the call to the
  * <code>connect()</code> method succeeds or fails. This function takes one parameter &mdash;
  * <code>error</code> (see the <a href="Error.html">Error</a> object).
  * On success, the <code>completionHandler</code> function is not passed any
  * arguments. On error, the function is passed an <code>error</code> object parameter
  * (see the <a href="Error.html">Error</a> object). The
  * <code>error</code> object has two properties: <code>code</code> (an integer) and
  * <code>message</code> (a string), which identify the cause of the failure. The following
  * code adds a <code>completionHandler</code> when calling the <code>connect()</code> method:
  * <pre>
  * session.connect(token, function (error) {
  *   if (error) {
  *       console.log(error.message);
  *   } else {
  *     console.log("Connected to session.");
  *   }
  * });
  * </pre>
  * <p>
  * Note that upon connecting to the session, the Session object dispatches a
  * <code>sessionConnected</code> event in addition to calling the <code>completionHandler</code>.
  * The SessionConnectEvent object, which defines the <code>sessionConnected</code> event,
  * includes <code>connections</code> and <code>streams</code> properties, which
  * list the connections and streams in the session when you connect.
  * </p>
  *
  * @see SessionConnectEvent
  * @method #connect
  * @memberOf Session
*/
  this.connect = function(token) {

    if (arguments.length > 1 &&
      (typeof arguments[0] === 'string' || typeof arguments[0] === 'number') &&
      typeof arguments[1] === 'string') {
      if (apiKey == null) _apiKey = token.toString();
      token = arguments[1];
    }

    // The completion handler is always the last argument.
    var completionHandler = arguments[arguments.length - 1];

    if (this.is('connecting', 'connected')) {
      OT.warn('OT.Session: Cannot connect, the session is already ' + this.state);
      return this;
    }

    init();
    setState('connecting');
    this.token = _token = !OT.$.isFunction(token) && token;

    // Get a new widget ID when reconnecting.
    if (_initialConnection) {
      _initialConnection = false;
    } else {
      _widgetId = OT.$.uuid();
    }

    if (completionHandler && OT.$.isFunction(completionHandler)) {
      this.once('sessionConnected', OT.$.bind(completionHandler, null, null));
      this.once('sessionConnectFailed', completionHandler);
    }

    if (_apiKey == null || OT.$.isFunction(_apiKey)) {
      setTimeout(OT.$.bind(
        sessionConnectFailed,
        this,
        'API Key is undefined. You must pass an API Key to initSession.',
        OT.ExceptionCodes.AUTHENTICATION_ERROR
      ));

      return this;
    }

    this.logConnectivityEvent('Attempt');

    if (!_sessionId || OT.$.isObject(_sessionId) || OT.$.isArray(_sessionId)) {
      var errorMsg;
      if (!_sessionId) {
        errorMsg = 'SessionID is undefined. You must pass a sessionID to initSession.';
      } else {
        errorMsg = 'SessionID is not a string. You must use string as the session ID passed into ' +
          'OT.initSession().';
        _sessionId = _sessionId.toString();
      }
      setTimeout(OT.$.bind(
        sessionConnectFailed,
        this,
        errorMsg,
        OT.ExceptionCodes.INVALID_SESSION_ID
      ));

      this.logConnectivityEvent('Failure', {reason:'ConnectToSession',
        code: OT.ExceptionCodes.INVALID_SESSION_ID,
        message: errorMsg});
      return this;
    }

    this.apiKey = _apiKey = _apiKey.toString();

    // Ugly hack, make sure OT.APIKEY is set
    if (OT.APIKEY.length === 0) {
      OT.APIKEY = _apiKey;
    }

    getSessionInfo.call(this);
    return this;
  };

  /**
  * Disconnects from the OpenTok session.
  *
  * <p>
  * Calling the <code>disconnect()</code> method ends your connection with the session. In the
  * course of terminating your connection, it also ceases publishing any stream(s) you were
  * publishing.
  * </p>
  * <p>
  * Session objects on remote clients dispatch <code>streamDestroyed</code> events for any
  * stream you were publishing. The Session object dispatches a <code>sessionDisconnected</code>
  * event locally. The Session objects on remote clients dispatch <code>connectionDestroyed</code>
  * events, letting other connections know you have left the session. The
  * {@link SessionDisconnectEvent} and {@link StreamEvent} objects that define the
  * <code>sessionDisconnect</code> and <code>connectionDestroyed</code> events each have a
  * <code>reason</code> property. The <code>reason</code> property lets the developer determine
  * whether the connection is being terminated voluntarily and whether any streams are being
  * destroyed as a byproduct of the underlying connection's voluntary destruction.
  * </p>
  * <p>
  * If the session is not currently connected, calling this method causes a warning to be logged.
  * See <a "href=OT.html#setLogLevel">OT.setLogLevel()</a>.
  * </p>
  *
  * <p>
  * <i>Note:</i> If you intend to reuse a Publisher object created using
  * <code>OT.initPublisher()</code> to publish to different sessions sequentially, call either
  * <code>Session.disconnect()</code> or <code>Session.unpublish()</code>. Do not call both.
  * Then call the <code>preventDefault()</code> method of the <code>streamDestroyed</code> or
  * <code>sessionDisconnected</code> event object to prevent the Publisher object from being
  * removed from the page. Be sure to call <code>preventDefault()</code> only if the
  * <code>connection.connectionId</code> property of the Stream object in the event matches the
  * <code>connection.connectionId</code> property of your Session object (to ensure that you are
  * preventing the default behavior for your published streams, not for other streams that you
  * subscribe to).
  * </p>
  *
  * <h5>
  * Events dispatched:
  * </h5>
  * <p>
  * <code>sessionDisconnected</code>
  * (<a href="SessionDisconnectEvent.html">SessionDisconnectEvent</a>)
  * &#151; Dispatched locally when the connection is disconnected.
  * </p>
  * <p>
  * <code>connectionDestroyed</code> (<a href="ConnectionEvent.html">ConnectionEvent</a>) &#151;
  * Dispatched on other clients, along with the <code>streamDestroyed</code> event (as warranted).
  * </p>
  *
  * <p>
  * <code>streamDestroyed</code> (<a href="StreamEvent.html">StreamEvent</a>) &#151;
  * Dispatched on other clients if streams are lost as a result of the session disconnecting.
  * </p>
  *
  * @method #disconnect
  * @memberOf Session
*/
  var disconnect = OT.$.bind(function disconnect(drainSocketBuffer) {
    if (_socket && _socket.isNot('disconnected')) {
      if (_socket.isNot('disconnecting')) {
        setState('disconnecting');
        _socket.disconnect(drainSocketBuffer);
      }
    } else {
      reset();
    }
  }, this);

  this.disconnect = function(drainSocketBuffer) {
    disconnect(drainSocketBuffer !== void 0 ? drainSocketBuffer : true);
  };

  this.destroy = function(reason) {
    this.streams.destroy();
    this.connections.destroy();
    this.archives.destroy();
    disconnect(reason !== 'unloaded');
  };

  /**
  * The <code>publish()</code> method starts publishing an audio-video stream to the session.
  * The audio-video stream is captured from a local microphone and webcam. Upon successful
  * publishing, the Session objects on all connected clients dispatch the
  * <code>streamCreated</code> event.
  * </p>
  *
  * <!--JS-ONLY-->
  * <p>You pass a Publisher object as the one parameter of the method. You can initialize a
  * Publisher object by calling the <a href="OT.html#initPublisher">OT.initPublisher()</a>
  * method. Before calling <code>Session.publish()</code>.
  * </p>
  *
  * <p>This method takes an alternate form: <code>publish([targetElement:String,
  * properties:Object]):Publisher</code> &#151; In this form, you do <i>not</i> pass a Publisher
  * object into the function. Instead, you pass in a <code>targetElement</code> (the ID of the
  * DOM element that the Publisher will replace) and a <code>properties</code> object that
  * defines options for the Publisher (see <a href="OT.html#initPublisher">OT.initPublisher()</a>.)
  * The method returns a new Publisher object, which starts sending an audio-video stream to the
  * session. The remainder of this documentation describes the form that takes a single Publisher
  * object as a parameter.
  *
  * <p>
  *   A local display of the published stream is created on the web page by replacing
  *         the specified element in the DOM with a streaming video display. The video stream
  *         is automatically mirrored horizontally so that users see themselves and movement
  *         in their stream in a natural way. If the width and height of the display do not match
  *         the 4:3 aspect ratio of the video signal, the video stream is cropped to fit the
  *         display.
  * </p>
  *
  * <p>
  *   If calling this method creates a new Publisher object and the OpenTok library does not
  *   have access to the camera or microphone, the web page alerts the user to grant access
  *   to the camera and microphone.
  * </p>
  *
  * <p>
  * The OT object dispatches an <code>exception</code> event if the user's role does not
  * include permissions required to publish. For example, if the user's role is set to subscriber,
  * then they cannot publish. You define a user's role when you create the user token using the
  * <code>generate_token()</code> method of the
  * <a href="https://tokbox.com/opentok/libraries/server/">OpenTok server-side
  * libraries</a> or the <a href="https://dashboard.tokbox.com/projects">Dashboard</a> page.
  * You pass the token string as a parameter of the <code>connect()</code> method of the Session
  * object. See <a href="ExceptionEvent.html">ExceptionEvent</a> and
  * <a href="OT.html#on">OT.on()</a>.
  * </p>
  *     <p>
  *     The application throws an error if the session is not connected.
  *     </p>
  *
  * <h5>Events dispatched:</h5>
  * <p>
  * <code>exception</code> (<a href="ExceptionEvent.html">ExceptionEvent</a>) &#151; Dispatched
  * by the OT object. This can occur when user's role does not allow publishing (the
  * <code>code</code> property of event object is set to 1500); it can also occur if the c
  * onnection fails to connect (the <code>code</code> property of event object is set to 1013).
  * WebRTC is a peer-to-peer protocol, and it is possible that connections will fail to connect.
  * The most common cause for failure is a firewall that the protocol cannot traverse.</li>
  * </p>
  * <p>
  * <code>streamCreated</code> (<a href="StreamEvent.html">StreamEvent</a>) &#151;
  * The stream has been published. The Session object dispatches this on all clients
  * subscribed to the stream, as well as on the publisher's client.
  * </p>
  *
  * <h5>Example</h5>
  *
  * <p>
  *   The following example publishes a video once the session connects:
  * </p>
  * <pre>
  * var sessionId = ""; // Replace with your own session ID.
  *                     // See https://dashboard.tokbox.com/projects
  * var token = ""; // Replace with a generated token that has been assigned the moderator role.
  *                 // See https://dashboard.tokbox.com/projects
  * var session = OT.initSession(apiKey, sessionID);
  * session.on("sessionConnected", function (event) {
  *     var publisherOptions = {width: 400, height:300, name:"Bob's stream"};
  *     // This assumes that there is a DOM element with the ID 'publisher':
  *     publisher = OT.initPublisher('publisher', publisherOptions);
  *     session.publish(publisher);
  * });
  * session.connect(token);
  * </pre>
  *
  * @param {Publisher} publisher A Publisher object, which you initialize by calling the
  * <a href="OT.html#initPublisher">OT.initPublisher()</a> method.
  *
  * @param {Function} completionHandler (Optional) A function to be called when the call to the
  * <code>publish()</code> method succeeds or fails. This function takes one parameter &mdash;
  * <code>error</code>. On success, the <code>completionHandler</code> function is not passed any
  * arguments. On error, the function is passed an <code>error</code> object parameter
  * (see the <a href="Error.html">Error</a> object). The
  * <code>error</code> object has two properties: <code>code</code> (an integer) and
  * <code>message</code> (a string), which identify the cause of the failure. Calling
  * <code>publish()</code> fails if the role assigned to your token is not "publisher" or
  * "moderator"; in this case <code>error.code</code> is set to 1500. Calling
  * <code>publish()</code> also fails the client fails to connect; in this case
  * <code>error.code</code> is set to 1013. The following code adds a
  * <code>completionHandler</code> when calling the <code>publish()</code> method:
  * <pre>
  * session.publish(publisher, null, function (error) {
  *   if (error) {
  *     console.log(error.message);
  *   } else {
  *     console.log("Publishing a stream.");
  *   }
  * });
  * </pre>
  *
  * @returns The Publisher object for this stream.
  *
  * @method #publish
  * @memberOf Session
*/
  this.publish = function(publisher, properties, completionHandler) {
    if (typeof publisher === 'function') {
      completionHandler = publisher;
      publisher = undefined;
    }
    if (typeof properties === 'function') {
      completionHandler = properties;
      properties = undefined;
    }
    if (this.isNot('connected')) {
      OT.analytics.logError(1010, 'OT.exception',
        'We need to be connected before you can publish', null, {
        action: 'Publish',
        variation: 'Failure',
        payload: {
          reason:'unconnected',
          code: OT.ExceptionCodes.NOT_CONNECTED,
          message: 'We need to be connected before you can publish'
        },
        sessionId: _sessionId,
        streamId: (publisher && publisher.stream) ? publisher.stream.id : null,
        partnerId: _apiKey
      });

      if (completionHandler && OT.$.isFunction(completionHandler)) {
        dispatchError(OT.ExceptionCodes.NOT_CONNECTED,
          'We need to be connected before you can publish', completionHandler);
      }

      return null;
    }

    if (!permittedTo('publish')) {
      var errorMessage = 'This token does not allow publishing. The role must be at least ' +
        '`publisher` to enable this functionality';
      var payload = {
        reason: 'permission',
        code: OT.ExceptionCodes.UNABLE_TO_PUBLISH,
        message: errorMessage
      };
      this.logEvent('publish', 'Failure', payload);
      dispatchError(OT.ExceptionCodes.UNABLE_TO_PUBLISH, errorMessage, completionHandler);
      return null;
    }

    // If the user has passed in an ID of a element then we create a new publisher.
    if (!publisher || typeof (publisher) === 'string' || OT.$.isElementNode(publisher)) {
      // Initiate a new Publisher with the new session credentials
      publisher = OT.initPublisher(publisher, properties);

    } else if (publisher instanceof OT.Publisher) {

      // If the publisher already has a session attached to it we can
      if ('session' in publisher && publisher.session && 'sessionId' in publisher.session) {
        // send a warning message that we can't publish again.
        if (publisher.session.sessionId === this.sessionId) {
          OT.warn('Cannot publish ' + publisher.guid() + ' again to ' +
            this.sessionId + '. Please call session.unpublish(publisher) first.');
        } else {
          OT.warn('Cannot publish ' + publisher.guid() + ' publisher already attached to ' +
            publisher.session.sessionId + '. Please call session.unpublish(publisher) first.');
        }
      }

    } else {
      dispatchError(OT.ExceptionCodes.UNABLE_TO_PUBLISH,
        'Session.publish :: First parameter passed in is neither a ' +
        'string nor an instance of the Publisher',
        completionHandler);
      return;
    }

    publisher.once('publishComplete', function() {
      var args = Array.prototype.slice.call(arguments),
          err = args[0];

      if (err) {
        err.message = 'Session.publish :: ' + err.message;
        args[0] = err;

        OT.error(err.code, err.message);
      }

      if (completionHandler && OT.$.isFunction(completionHandler)) {
        completionHandler.apply(null, args);
      }
    });

    // Add publisher reference to the session
    publisher._.publishToSession(this);

    // return the embed publisher
    return publisher;
  };

  /**
  * Ceases publishing the specified publisher's audio-video stream
  * to the session. By default, the local representation of the audio-video stream is
  * removed from the web page. Upon successful termination, the Session object on every
  * connected web page dispatches
  * a <code>streamDestroyed</code> event.
  * </p>
  *
  * <p>
  * To prevent the Publisher from being removed from the DOM, add an event listener for the
  * <code>streamDestroyed</code> event dispatched by the Publisher object and call the
  * <code>preventDefault()</code> method of the event object.
  * </p>
  *
  * <p>
  * <i>Note:</i> If you intend to reuse a Publisher object created using
  * <code>OT.initPublisher()</code> to publish to different sessions sequentially, call
  * either <code>Session.disconnect()</code> or <code>Session.unpublish()</code>. Do not call
  * both. Then call the <code>preventDefault()</code> method of the <code>streamDestroyed</code>
  * or <code>sessionDisconnected</code> event object to prevent the Publisher object from being
  * removed from the page. Be sure to call <code>preventDefault()</code> only if the
  * <code>connection.connectionId</code> property of the Stream object in the event matches the
  * <code>connection.connectionId</code> property of your Session object (to ensure that you are
  * preventing the default behavior for your published streams, not for other streams that you
  * subscribe to).
  * </p>
  *
  * <h5>Events dispatched:</h5>
  *
  * <p>
  * <code>streamDestroyed</code> (<a href="StreamEvent.html">StreamEvent</a>) &#151;
  * The stream associated with the Publisher has been destroyed. Dispatched on by the
  * Publisher on on the Publisher's browser. Dispatched by the Session object on
  * all other connections subscribing to the publisher's stream.
  * </p>
  *
  * <h5>Example</h5>
  *
  * The following example publishes a stream to a session and adds a Disconnect link to the
  * web page. Clicking this link causes the stream to stop being published.
  *
  * <pre>
  * &lt;script&gt;
  *     var apiKey = ""; // Replace with your API key. See https://dashboard.tokbox.com/projects
  *     var sessionID = ""; // Replace with your own session ID.
  *                      // See https://dashboard.tokbox.com/projects
  *     var token = "Replace with the TokBox token string provided to you."
  *     var session = OT.initSession(apiKey, sessionID);
  *     session.on("sessionConnected", function sessionConnectHandler(event) {
  *         // This assumes that there is a DOM element with the ID 'publisher':
  *         publisher = OT.initPublisher('publisher');
  *         session.publish(publisher);
  *     });
  *     session.connect(token);
  *     var publisher;
  *
  *     function unpublish() {
  *         session.unpublish(publisher);
  *     }
  * &lt;/script&gt;
  *
  * &lt;body&gt;
  *
  *     &lt;div id="publisherContainer/&gt;
  *     &lt;br/&gt;
  *
  *     &lt;a href="javascript:unpublish()"&gt;Stop Publishing&lt;/a&gt;
  *
  * &lt;/body&gt;
  *
  * </pre>
  *
  * @see <a href="#publish">publish()</a>
  *
  * @see <a href="StreamEvent.html">streamDestroyed event</a>
  *
  * @param {Publisher} publisher</span> The Publisher object to stop streaming.
  *
  * @method #unpublish
  * @memberOf Session
*/
  this.unpublish = function(publisher) {
    if (!publisher) {
      OT.error('OT.Session.unpublish: publisher parameter missing.');
      return;
    }

    // Unpublish the localMedia publisher
    publisher._.unpublishFromSession(this, 'unpublished');
  };

  /**
  * Subscribes to a stream that is available to the session. You can get an array of
  * available streams from the <code>streams</code> property of the <code>sessionConnected</code>
  * and <code>streamCreated</code> events (see
  * <a href="SessionConnectEvent.html">SessionConnectEvent</a> and
  * <a href="StreamEvent.html">StreamEvent</a>).
  * </p>
  * <p>
  * The subscribed stream is displayed on the local web page by replacing the specified element
  * in the DOM with a streaming video display. If the width and height of the display do not
  * match the 4:3 aspect ratio of the video signal, the video stream is cropped to fit
  * the display. If the stream lacks a video component, a blank screen with an audio indicator
  * is displayed in place of the video stream.
  * </p>
  *
  * <p>
  * The application throws an error if the session is not connected<!--JS-ONLY--> or if the
  * <code>targetElement</code> does not exist in the HTML DOM<!--/JS-ONLY-->.
  * </p>
  *
  * <h5>Example</h5>
  *
  * The following code subscribes to other clients' streams:
  *
  * <pre>
  * var apiKey = ""; // Replace with your API key. See https://dashboard.tokbox.com/projects
  * var sessionID = ""; // Replace with your own session ID.
  *                     // See https://dashboard.tokbox.com/projects
  *
  * var session = OT.initSession(apiKey, sessionID);
  * session.on("streamCreated", function(event) {
  *   subscriber = session.subscribe(event.stream, targetElement);
  * });
  * session.connect(token);
  * </pre>
  *
  * @param {Stream} stream The Stream object representing the stream to which we are trying to
  * subscribe.
  *
  * @param {Object} targetElement (Optional) The DOM element or the <code>id</code> attribute of
  * the existing DOM element used to determine the location of the Subscriber video in the HTML
  * DOM. See the <code>insertMode</code> property of the <code>properties</code> parameter. If
  * you do not specify a <code>targetElement</code>, the application appends a new DOM element
  * to the HTML <code>body</code>.
  *
  * @param {Object} properties This is an object that contains the following properties:
  *    <ul>
  *       <li><code>audioVolume</code> (Number) &#151; The desired audio volume, between 0 and
  *       100, when the Subscriber is first opened (default: 50). After you subscribe to the
  *       stream, you can adjust the volume by calling the
  *       <a href="Subscriber.html#setAudioVolume"><code>setAudioVolume()</code> method</a> of the
  *       Subscriber object. This volume setting affects local playback only; it does not affect
  *       the stream's volume on other clients.</li>
  *
  *       <li>
  *         <code>fitMode</code> (String) &#151; Determines how the video is displayed if the its
  *           dimensions do not match those of the DOM element. You can set this property to one of
  *           the following values:
  *           <p>
  *           <ul>
  *             <li>
  *               <code>"cover"</code> &mdash; The video is cropped if its dimensions do not match
  *               those of the DOM element. This is the default setting for videos that have a
  *               camera as the source (for Stream objects with the <code>videoType</code> property
  *               set to <code>"camera"</code>).
  *             </li>
  *             <li>
  *               <code>"contain"</code> &mdash; The video is letter-boxed if its dimensions do not
  *               match those of the DOM element. This is the default setting for screen-sharing
  *               videos (for Stream objects with the <code>videoType</code> property set to
  *               <code>"screen"</code>).
  *             </li>
  *           </ul>
  *       </li>
  *
  *       <li>
  *       <code>height</code> (Number) &#151; The desired initial height of the displayed
  *       video in the HTML page (default: 198 pixels). You can specify the number of pixels as
  *       either a number (such as 300) or a string ending in "px" (such as "300px"). Or you can
  *       specify a percentage of the size of the parent element, with a string ending in "%"
  *       (such as "100%"). <i>Note:</i> To resize the video, adjust the CSS of the subscriber's
  *       DOM element (the <code>element</code> property of the Subscriber object) or (if the
  *       height is specified as a percentage) its parent DOM element (see
  *       <a href="https://tokbox.com/developer/guides/customize-ui/js/#video_resize_reposition">
  *       Resizing or repositioning a video</a>).
  *       </li>
  *       <li>
  *         <code>insertMode</code> (String) &#151; Specifies how the Subscriber object will
  *         be inserted in the HTML DOM. See the <code>targetElement</code> parameter. This
  *         string can have the following values:
  *         <p>
  *         <ul>
  *           <li><code>"replace"</code> &#151; The Subscriber object replaces contents of the
  *             targetElement. This is the default.</li>
  *           <li><code>"after"</code> &#151; The Subscriber object is a new element inserted
  *             after the targetElement in the HTML DOM. (Both the Subscriber and targetElement
  *             have the same parent element.)</li>
  *           <li><code>"before"</code> &#151; The Subscriber object is a new element inserted
  *             before the targetElement in the HTML DOM. (Both the Subsciber and targetElement
  *             have the same parent element.)</li>
  *           <li><code>"append"</code> &#151; The Subscriber object is a new element added as a
  *             child of the targetElement. If there are other child elements, the Subscriber is
  *             appended as the last child element of the targetElement.</li>
  *         </ul></p>
  *   <p> Do not move the publisher element or its parent elements in the DOM
  *   heirarchy. Use CSS to resize or reposition the publisher video's element
  *   (the <code>element</code> property of the Publisher object) or its parent element (see
  *   <a href="https://tokbox.com/developer/guides/customize-ui/js/#video_resize_reposition">
  *   Resizing or repositioning a video</a>).</p>
  *   </li>
  *   <li>
  *   <code>showControls</code> (Boolean) &#151; Whether to display the built-in user interface
  *   controls for the Subscriber (default: <code>true</code>). These controls include the name
  *   display, the audio level indicator, the speaker control button, the video disabled indicator,
  *   and the video disabled warning icon. You can turn off all user interface controls by setting
  *   this property to <code>false</code>. You can control the display of individual user interface
  *   controls by leaving this property set to <code>true</code> (the default) and setting
  *   individual properties of the <code>style</code> property.
  *   </li>
  *   <li>
  *   <code>style</code> (Object) &#151; An object containing properties that define the initial
  *   appearance of user interface controls of the Subscriber. The <code>style</code> object
  *   includes the following properties:
  *     <ul>
  *       <li><code>audioLevelDisplayMode</code> (String) &mdash; How to display the audio level
  *       indicator. Possible values are: <code>"auto"</code> (the indicator is displayed when the
  *       video is disabled), <code>"off"</code> (the indicator is not displayed), and
  *       <code>"on"</code> (the indicator is always displayed).</li>
  *
  *       <li><code>backgroundImageURI</code> (String) &mdash; A URI for an image to display as
  *       the background image when a video is not displayed. (A video may not be displayed if
  *       you call <code>subscribeToVideo(false)</code> on the Subscriber object). You can pass an
  *       http or https URI to a PNG, JPEG, or non-animated GIF file location. You can also use the
  *       <code>data</code> URI scheme (instead of http or https) and pass in base-64-encrypted
  *       PNG data, such as that obtained from the
  *       <a href="Subscriber.html#getImgData">Subscriber.getImgData()</a> method. For example,
  *       you could set the property to <code>"data:VBORw0KGgoAA..."</code>, where the portion of
  *       the string after <code>"data:"</code> is the result of a call to
  *       <code>Subscriber.getImgData()</code>. If the URL or the image data is invalid, the
  *       property is ignored (the attempt to set the image fails silently).</li>
  *
  *       <li><code>buttonDisplayMode</code> (String) &mdash; How to display the speaker controls
  *       Possible values are: <code>"auto"</code> (controls are displayed when the stream is first
  *       displayed and when the user mouses over the display), <code>"off"</code> (controls are not
  *       displayed), and <code>"on"</code> (controls are always displayed).</li>
  *
  *       <li><code>nameDisplayMode</code> (String) &#151; Whether to display the stream name.
  *       Possible values are: <code>"auto"</code> (the name is displayed when the stream is first
  *       displayed and when the user mouses over the display), <code>"off"</code> (the name is not
  *       displayed), and <code>"on"</code> (the name is always displayed).</li>
  *
  *       <li><code>videoDisabledDisplayMode</code> (String) &#151; Whether to display the video
  *       disabled indicator and video disabled warning icons for a Subscriber. These icons
  *       indicate that the video has been disabled (or is in risk of being disabled for
  *       the warning icon) due to poor stream quality. This style only applies to the Subscriber
  *       object. Possible values are: <code>"auto"</code> (the icons are automatically when the
  *       displayed video is disabled or in risk of being disabled due to poor stream quality),
  *       <code>"off"</code> (do not display the icons), and <code>"on"</code> (display the
  *       icons). The default setting is <code>"auto"</code></li>
  *   </ul>
  *   </li>
  *
  *       <li><code>subscribeToAudio</code> (Boolean) &#151; Whether to initially subscribe to audio
  *       (if available) for the stream (default: <code>true</code>).</li>
  *
  *       <li><code>subscribeToVideo</code> (Boolean) &#151; Whether to initially subscribe to video
  *       (if available) for the stream (default: <code>true</code>).</li>
  *
  *       <li>
  *       <code>width</code> (Number) &#151; The desired initial width of the displayed
  *       video in the HTML page (default: 264 pixels). You can specify the number of pixels as
  *       either a number (such as 400) or a string ending in "px" (such as "400px"). Or you can
  *       specify a percentage of the size of the parent element, with a string ending in "%"
  *       (such as "100%"). <i>Note:</i> To resize the video, adjust the CSS of the subscriber's
  *       DOM element (the <code>element</code> property of the Subscriber object) or (if the
  *       width is specified as a percentage) its parent DOM element (see
  *       <a href="https://tokbox.com/developer/guides/customize-ui/js/#video_resize_reposition">
  *       Resizing or repositioning a video</a>).
  *       </li>
  *
  *    </ul>
  *
  * @param {Function} completionHandler (Optional) A function to be called when the call to the
  * <code>subscribe()</code> method succeeds or fails. This function takes one parameter &mdash;
  * <code>error</code>. On success, the <code>completionHandler</code> function is not passed any
  * arguments. On error, the function is passed an <code>error</code> object, defined by the
  * <a href="Error.html">Error</a> class, has two properties: <code>code</code> (an integer) and
  * <code>message</code> (a string), which identify the cause of the failure. The following
  * code adds a <code>completionHandler</code> when calling the <code>subscribe()</code> method:
  * <pre>
  * session.subscribe(stream, "subscriber", null, function (error) {
  *   if (error) {
  *     console.log(error.message);
  *   } else {
  *     console.log("Subscribed to stream: " + stream.id);
  *   }
  * });
  * </pre>
  *
  * @signature subscribe(stream, targetElement, properties, completionHandler)
  * @returns {Subscriber} The Subscriber object for this stream. Stream control functions
  * are exposed through the Subscriber object.
  * @method #subscribe
  * @memberOf Session
*/
  this.subscribe = function(stream, targetElement, properties, completionHandler) {
    if (typeof targetElement === 'function') {
      completionHandler = targetElement;
      targetElement = undefined;
      properties = undefined;
    }

    if (typeof properties === 'function') {
      completionHandler = properties;
      properties = undefined;
    }

    if (!this.connection || !this.connection.connectionId) {
      dispatchError(OT.ExceptionCodes.UNABLE_TO_SUBSCRIBE,
                    'Session.subscribe :: Connection required to subscribe',
                    completionHandler);
      return;
    }

    if (!stream) {
      dispatchError(OT.ExceptionCodes.UNABLE_TO_SUBSCRIBE,
                    'Session.subscribe :: stream cannot be null',
                    completionHandler);
      return;
    }

    if (!stream.hasOwnProperty('streamId')) {
      dispatchError(OT.ExceptionCodes.UNABLE_TO_SUBSCRIBE,
                    'Session.subscribe :: invalid stream object',
                    completionHandler);
      return;
    }

    var subscriber = new OT.Subscriber(targetElement, OT.$.extend(properties || {}, {
      stream: stream,
      session: this
    }), function(err) {

      if (err) {
        var errorCode,
          errorMessage,
          knownErrorCodes = [400, 403];
        if (!err.code && OT.$.arrayIndexOf(knownErrorCodes, err.code) > -1) {
          errorCode = OT.OT.ExceptionCodes.UNABLE_TO_SUBSCRIBE;
          errorMessage = 'Session.subscribe :: ' + err.message;
        } else {
          errorCode = OT.ExceptionCodes.UNEXPECTED_SERVER_RESPONSE;
          errorMessage = 'Unexpected server response. Try this operation again later.';
        }

        dispatchError(errorCode, errorMessage, completionHandler);

      } else if (completionHandler && OT.$.isFunction(completionHandler)) {
        completionHandler.apply(null, arguments);
      }

    });

    OT.subscribers.add(subscriber);

    return subscriber;

  };

  /**
  * Stops subscribing to a stream in the session. the display of the audio-video stream is
  * removed from the local web page.
  *
  * <h5>Example</h5>
  * <p>
  * The following code subscribes to other clients' streams. For each stream, the code also
  * adds an Unsubscribe link.
  * </p>
  * <pre>
  * var apiKey = ""; // Replace with your API key. See https://dashboard.tokbox.com/projects
  * var sessionID = ""; // Replace with your own session ID.
  *                     // See https://dashboard.tokbox.com/projects
  * var streams = [];
  *
  * var session = OT.initSession(apiKey, sessionID);
  * session.on("streamCreated", function(event) {
  *     var stream = event.stream;
  *     displayStream(stream);
  * });
  * session.connect(token);
  *
  * function displayStream(stream) {
  *     var div = document.createElement('div');
  *     div.setAttribute('id', 'stream' + stream.streamId);
  *
  *     var subscriber = session.subscribe(stream, div);
  *     subscribers.push(subscriber);
  *
  *     var aLink = document.createElement('a');
  *     aLink.setAttribute('href', 'javascript: unsubscribe("' + subscriber.id + '")');
  *     aLink.innerHTML = "Unsubscribe";
  *
  *     var streamsContainer = document.getElementById('streamsContainer');
  *     streamsContainer.appendChild(div);
  *     streamsContainer.appendChild(aLink);
  *
  *     streams = event.streams;
  * }
  *
  * function unsubscribe(subscriberId) {
  *     console.log("unsubscribe called");
  *     for (var i = 0; i &lt; subscribers.length; i++) {
  *         var subscriber = subscribers[i];
  *         if (subscriber.id == subscriberId) {
  *             session.unsubscribe(subscriber);
  *         }
  *     }
  * }
  * </pre>
  *
  * @param {Subscriber} subscriber The Subscriber object to unsubcribe.
  *
  * @see <a href="#subscribe">subscribe()</a>
  *
  * @method #unsubscribe
  * @memberOf Session
*/
  this.unsubscribe = function(subscriber) {
    if (!subscriber) {
      var errorMsg = 'OT.Session.unsubscribe: subscriber cannot be null';
      OT.error(errorMsg);
      throw new Error(errorMsg);
    }

    if (!subscriber.stream) {
      OT.warn('OT.Session.unsubscribe:: tried to unsubscribe a subscriber that had no stream');
      return false;
    }

    OT.debug('OT.Session.unsubscribe: subscriber ' + subscriber.id);

    subscriber.destroy();

    return true;
  };

  /**
  * Returns an array of local Subscriber objects for a given stream.
  *
  * @param {Stream} stream The stream for which you want to find subscribers.
  *
  * @returns {Array} An array of {@link Subscriber} objects for the specified stream.
  *
  * @see <a href="#unsubscribe">unsubscribe()</a>
  * @see <a href="Subscriber.html">Subscriber</a>
  * @see <a href="StreamEvent.html">StreamEvent</a>
  * @method #getSubscribersForStream
  * @memberOf Session
*/
  this.getSubscribersForStream = function(stream) {
    return OT.subscribers.where({streamId: stream.id});
  };

  /**
  * Returns the local Publisher object for a given stream.
  *
  * @param {Stream} stream The stream for which you want to find the Publisher.
  *
  * @returns {Publisher} A Publisher object for the specified stream. Returns
  * <code>null</code> if there is no local Publisher object
  * for the specified stream.
  *
  * @see <a href="#forceUnpublish">forceUnpublish()</a>
  * @see <a href="Subscriber.html">Subscriber</a>
  * @see <a href="StreamEvent.html">StreamEvent</a>
  *
  * @method #getPublisherForStream
  * @memberOf Session
*/
  this.getPublisherForStream = function(stream) {
    var streamId,
        errorMsg;

    if (typeof stream === 'string') {
      streamId = stream;
    } else if (typeof stream === 'object' && stream && stream.hasOwnProperty('id')) {
      streamId = stream.id;
    } else {
      errorMsg = 'Session.getPublisherForStream :: Invalid stream type';
      OT.error(errorMsg);
      throw new Error(errorMsg);
    }

    return OT.publishers.where({streamId: streamId})[0];
  };

  // Private Session API: for internal OT use only
  this._ = {
    jsepCandidateP2p: function(streamId, subscriberId, candidate) {
      return _socket.jsepCandidateP2p(streamId, subscriberId, candidate);
    },

    jsepCandidate: function(streamId, candidate) {
      return _socket.jsepCandidate(streamId, candidate);
    },

    jsepOffer: function(streamId, offerSdp) {
      return _socket.jsepOffer(streamId, offerSdp);
    },

    jsepOfferP2p: function(streamId, subscriberId, offerSdp) {
      return _socket.jsepOfferP2p(streamId, subscriberId, offerSdp);
    },

    jsepAnswer: function(streamId, answerSdp) {
      return _socket.jsepAnswer(streamId, answerSdp);
    },

    jsepAnswerP2p: function(streamId, subscriberId, answerSdp) {
      return _socket.jsepAnswerP2p(streamId, subscriberId, answerSdp);
    },

    // session.on("signal", function(SignalEvent))
    // session.on("signal:{type}", function(SignalEvent))
    dispatchSignal: OT.$.bind(function(fromConnection, type, data) {
      var event = new OT.SignalEvent(type, data, fromConnection);
      event.target = this;

      // signal a "signal" event
      // NOTE: trigger doesn't support defaultAction, and therefore preventDefault.
      this.trigger(OT.Event.names.SIGNAL, event);

      // signal an "signal:{type}" event" if there was a custom type
      if (type) this.dispatchEvent(event);
    }, this),

    subscriberCreate: function(stream, subscriber, channelsToSubscribeTo, completion) {
      return _socket.subscriberCreate(stream.id, subscriber.widgetId,
        channelsToSubscribeTo, completion);
    },

    subscriberDestroy: function(stream, subscriber) {
      return _socket.subscriberDestroy(stream.id, subscriber.widgetId);
    },

    subscriberUpdate: function(stream, subscriber, attributes) {
      return _socket.subscriberUpdate(stream.id, subscriber.widgetId, attributes);
    },

    subscriberChannelUpdate: function(stream, subscriber, channel, attributes) {
      return _socket.subscriberChannelUpdate(stream.id, subscriber.widgetId, channel.id,
        attributes);
    },

    streamCreate: function(name, streamId, audioFallbackEnabled, channels, completion) {
      _socket.streamCreate(
        name,
        streamId,
        audioFallbackEnabled,
        channels,
        OT.Config.get('bitrates', 'min', OT.APIKEY),
        OT.Config.get('bitrates', 'max', OT.APIKEY),
        completion
      );
    },

    streamDestroy: function(streamId) {
      _socket.streamDestroy(streamId);
    },

    streamChannelUpdate: function(stream, channel, attributes) {
      _socket.streamChannelUpdate(stream.id, channel.id, attributes);
    }
  };

  /**
  * Sends a signal to each client or a specified client in the session. Specify a
  * <code>to</code> property of the <code>signal</code> parameter to limit the signal to
  * be sent to a specific client; otherwise the signal is sent to each client connected to
  * the session.
  * <p>
  * The following example sends a signal of type "foo" with a specified data payload ("hello")
  * to all clients connected to the session:
  * <pre>
  * session.signal({
  *     type: "foo",
  *     data: "hello"
  *   },
  *   function(error) {
  *     if (error) {
  *       console.log("signal error: " + error.message);
  *     } else {
  *       console.log("signal sent");
  *     }
  *   }
  * );
  * </pre>
  * <p>
  * Calling this method without specifying a recipient client (by setting the <code>to</code>
  * property of the <code>signal</code> parameter) results in multiple signals sent (one to each
  * client in the session). For information on charges for signaling, see the
  * <a href="http://tokbox.com/pricing">OpenTok pricing</a> page.
  * <p>
  * The following example sends a signal of type "foo" with a data payload ("hello") to a
  * specific client connected to the session:
  * <pre>
  * session.signal({
  *     type: "foo",
  *     to: recipientConnection; // a Connection object
  *     data: "hello"
  *   },
  *   function(error) {
  *     if (error) {
  *       console.log("signal error: " + error.message);
  *     } else {
  *       console.log("signal sent");
  *     }
  *   }
  * );
  * </pre>
  * <p>
  * Add an event handler for the <code>signal</code> event to listen for all signals sent in
  * the session. Add an event handler for the <code>signal:type</code> event to listen for
  * signals of a specified type only (replace <code>type</code>, in <code>signal:type</code>,
  * with the type of signal to listen for). The Session object dispatches these events. (See
  * <a href="#events">events</a>.)
  *
  * @param {Object} signal An object that contains the following properties defining the signal:
  * <ul>
  *   <li><code>data</code> &mdash; (String) The data to send. The limit to the length of data
  *     string is 8kB. Do not set the data string to <code>null</code> or
  *     <code>undefined</code>.</li>
  *   <li><code>to</code> &mdash; (Connection) A <a href="Connection.html">Connection</a>
  *      object corresponding to the client that the message is to be sent to. If you do not
  *      specify this property, the signal is sent to all clients connected to the session.</li>
  *   <li><code>type</code> &mdash; (String) The type of the signal. You can use the type to
  *     filter signals when setting an event handler for the <code>signal:type</code> event
  *     (where you replace <code>type</code> with the type string). The maximum length of the
  *     <code>type</code> string is 128 characters, and it must contain only letters (A-Z and a-z),
  *     numbers (0-9), '-', '_', and '~'.</li>
  *   </li>
  * </ul>
  *
  * <p>Each property is optional. If you set none of the properties, you will send a signal
  * with no data or type to each client connected to the session.</p>
  *
  * @param {Function} completionHandler A function that is called when sending the signal
  * succeeds or fails. This function takes one parameter &mdash; <code>error</code>.
  * On success, the <code>completionHandler</code> function is not passed any
  * arguments. On error, the function is passed an <code>error</code> object, defined by the
  * <a href="Error.html">Error</a> class. The <code>error</code> object has the following
  * properties:
  *
  * <ul>
  *   <li><code>code</code> &mdash; (Number) An error code, which can be one of the following:
  *     <table style="width:100%">
  *         <tr>
  *           <td>400</td> <td>One of the signal properties &mdash; data, type, or to &mdash;
  *                         is invalid.</td>
  *         </tr>
  *         <tr>
  *           <td>404</td> <td>The client specified by the to property is not connected to
  *                        the session.</td>
  *         </tr>
  *         <tr>
  *           <td>413</td> <td>The type string exceeds the maximum length (128 bytes),
  *                        or the data string exceeds the maximum size (8 kB).</td>
  *         </tr>
  *         <tr>
  *           <td>500</td> <td>You are not connected to the OpenTok session.</td>
  *         </tr>
  *      </table>
  *   </li>
  *   <li><code>message</code> &mdash; (String) A description of the error.</li>
  * </ul>
  *
  * <p>Note that the <code>completionHandler</code> success result (<code>error == null</code>)
  * indicates that the options passed into the <code>Session.signal()</code> method are valid
  * and the signal was sent. It does <i>not</i> indicate that the signal was successfully
  * received by any of the intended recipients.
  *
  * @method #signal
  * @memberOf Session
  * @see <a href="#event:signal">signal</a> and <a href="#event:signal:type">signal:type</a> events
*/
  this.signal = function(options, completion) {
    var _options = options,
        _completion = completion;

    if (OT.$.isFunction(_options)) {
      _completion = _options;
      _options = null;
    }

    if (this.isNot('connected')) {
      var notConnectedErrorMsg = 'Unable to send signal - you are not connected to the session.';
      dispatchError(500, notConnectedErrorMsg, _completion);
      return;
    }

    _socket.signal(_options, _completion, this.logEvent);
    if (options && options.data && typeof options.data !== 'string') {
      OT.warn('Signaling of anything other than Strings is deprecated. ' +
              'Please update the data property to be a string.');
    }
  };

  /**
  *   Forces a remote connection to leave the session.
  *
  * <p>
  *   The <code>forceDisconnect()</code> method is normally used as a moderation tool
  *        to remove users from an ongoing session.
  * </p>
  * <p>
  *   When a connection is terminated using the <code>forceDisconnect()</code>,
  *        <code>sessionDisconnected</code>, <code>connectionDestroyed</code> and
  *        <code>streamDestroyed</code> events are dispatched in the same way as they
  *        would be if the connection had terminated itself using the <code>disconnect()</code>
  *        method. However, the <code>reason</code> property of a {@link ConnectionEvent} or
  *        {@link StreamEvent} object specifies <code>"forceDisconnected"</code> as the reason
  *        for the destruction of the connection and stream(s).
  * </p>
  * <p>
  *   While you can use the <code>forceDisconnect()</code> method to terminate your own connection,
  *        calling the <code>disconnect()</code> method is simpler.
  * </p>
  * <p>
  *   The OT object dispatches an <code>exception</code> event if the user's role
  *   does not include permissions required to force other users to disconnect.
  *   You define a user's role when you create the user token using the
  *   <code>generate_token()</code> method using one of the
  *   <a href="https://tokbox.com/opentok/libraries/server/">OpenTok server-side libraries</a> or
  *   or the <a href="https://dashboard.tokbox.com/projects">Dashboard</a> page.
  *   See <a href="ExceptionEvent.html">ExceptionEvent</a> and <a href="OT.html#on">OT.on()</a>.
  * </p>
  * <p>
  *   The application throws an error if the session is not connected.
  * </p>
  *
  * <h5>Events dispatched:</h5>
  *
  * <p>
  *   <code>connectionDestroyed</code> (<a href="ConnectionEvent.html">ConnectionEvent</a>) &#151;
  *     On clients other than which had the connection terminated.
  * </p>
  * <p>
  *   <code>exception</code> (<a href="ExceptionEvent.html">ExceptionEvent</a>) &#151;
  *     The user's role does not allow forcing other user's to disconnect (<code>event.code =
  *     1530</code>),
  *   or the specified stream is not publishing to the session (<code>event.code = 1535</code>).
  * </p>
  * <p>
  *   <code>sessionDisconnected</code>
  *   (<a href="SessionDisconnectEvent.html">SessionDisconnectEvent</a>) &#151;
  *     On the client which has the connection terminated.
  * </p>
  * <p>
  *   <code>streamDestroyed</code> (<a href="StreamEvent.html">StreamEvent</a>) &#151;
  *     If streams are stopped as a result of the connection ending.
  * </p>
  *
  * @param {Connection} connection The connection to be disconnected from the session.
  * This value can either be a <a href="Connection.html">Connection</a> object or a connection
  * ID (which can be obtained from the <code>connectionId</code> property of the Connection object).
  *
  * @param {Function} completionHandler (Optional) A function to be called when the call to the
  * <code>forceDiscononnect()</code> method succeeds or fails. This function takes one parameter
  * &mdash; <code>error</code>. On success, the <code>completionHandler</code> function is
  * not passed any arguments. On error, the function is passed an <code>error</code> object
  * parameter. The <code>error</code> object, defined by the <a href="Error.html">Error</a>
  * class, has two properties: <code>code</code> (an integer)
  * and <code>message</code> (a string), which identify the cause of the failure.
  * Calling <code>forceDisconnect()</code> fails if the role assigned to your
  * token is not "moderator"; in this case <code>error.code</code> is set to 1520. The following
  * code adds a <code>completionHandler</code> when calling the <code>forceDisconnect()</code>
  * method:
  * <pre>
  * session.forceDisconnect(connection, function (error) {
  *   if (error) {
  *       console.log(error);
  *     } else {
  *       console.log("Connection forced to disconnect: " + connection.id);
  *     }
  *   });
  * </pre>
  *
  * @method #forceDisconnect
  * @memberOf Session
*/

  this.forceDisconnect = function(connectionOrConnectionId, completionHandler) {
    if (this.isNot('connected')) {
      var notConnectedErrorMsg = 'Cannot call forceDisconnect(). You are not ' +
                                 'connected to the session.';
      dispatchError(OT.ExceptionCodes.NOT_CONNECTED, notConnectedErrorMsg, completionHandler);
      return;
    }

    var connectionId = (
      typeof connectionOrConnectionId === 'string' ?
      connectionOrConnectionId :
      connectionOrConnectionId.id
    );

    var invalidParameterErrorMsg = (
      'Invalid Parameter. Check that you have passed valid parameter values into the method call.'
    );

    if (!connectionId) {
      dispatchError(
        OT.ExceptionCodes.INVALID_PARAMETER,
        invalidParameterErrorMsg,
        completionHandler
      );

      return;
    }

    var notPermittedErrorMsg = 'This token does not allow forceDisconnect. ' +
      'The role must be at least `moderator` to enable this functionality';

    if (!permittedTo('forceDisconnect')) {
      // if this throws an error the handleJsException won't occur
      dispatchError(
        OT.ExceptionCodes.UNABLE_TO_FORCE_DISCONNECT,
        notPermittedErrorMsg,
        completionHandler
      );

      return;
    }

    _socket.forceDisconnect(connectionId, function(err) {
      if (err) {
        dispatchError(
          OT.ExceptionCodes.INVALID_PARAMETER,
          invalidParameterErrorMsg,
          completionHandler
        );
      } else if (completionHandler && OT.$.isFunction(completionHandler)) {
        completionHandler.apply(null, arguments);
      }
    });
  };

  /**
  * Forces the publisher of the specified stream to stop publishing the stream.
  *
  * <p>
  * Calling this method causes the Session object to dispatch a <code>streamDestroyed</code>
  * event on all clients that are subscribed to the stream (including the client that is
  * publishing the stream). The <code>reason</code> property of the StreamEvent object is
  * set to <code>"forceUnpublished"</code>.
  * </p>
  * <p>
  * The OT object dispatches an <code>exception</code> event if the user's role
  * does not include permissions required to force other users to unpublish.
  * You define a user's role when you create the user token using the <code>generate_token()</code>
  * method using the
  * <a href="https://tokbox.com/opentok/libraries/server/">OpenTok server-side libraries</a> or the
  * <a href="https://dashboard.tokbox.com/projects">Dashboard</a> page.
  * You pass the token string as a parameter of the <code>connect()</code> method of the Session
  * object. See <a href="ExceptionEvent.html">ExceptionEvent</a> and
  * <a href="OT.html#on">OT.on()</a>.
  * </p>
  *
  * <h5>Events dispatched:</h5>
  *
  * <p>
  *   <code>exception</code> (<a href="ExceptionEvent.html">ExceptionEvent</a>) &#151;
  *     The user's role does not allow forcing other users to unpublish.
  * </p>
  * <p>
  *   <code>streamDestroyed</code> (<a href="StreamEvent.html">StreamEvent</a>) &#151;
  *     The stream has been unpublished. The Session object dispatches this on all clients
  *     subscribed to the stream, as well as on the publisher's client.
  * </p>
  *
  * @param {Stream} stream The stream to be unpublished.
  *
  * @param {Function} completionHandler (Optional) A function to be called when the call to the
  * <code>forceUnpublish()</code> method succeeds or fails. This function takes one parameter
  * &mdash; <code>error</code>. On success, the <code>completionHandler</code> function is
  * not passed any arguments. On error, the function is passed an <code>error</code> object
  * parameter. The <code>error</code> object, defined by the <a href="Error.html">Error</a>
  * class, has two properties: <code>code</code> (an integer)
  * and <code>message</code> (a string), which identify the cause of the failure. Calling
  * <code>forceUnpublish()</code> fails if the role assigned to your token is not "moderator";
  * in this case <code>error.code</code> is set to 1530. The following code adds a
  * <code>completionHandler</code> when calling the <code>forceUnpublish()</code> method:
  * <pre>
  * session.forceUnpublish(stream, function (error) {
  *   if (error) {
  *       console.log(error);
  *     } else {
  *       console.log("Connection forced to disconnect: " + connection.id);
  *     }
  *   });
  * </pre>
  *
  * @method #forceUnpublish
  * @memberOf Session
*/
  this.forceUnpublish = function(streamOrStreamId, completionHandler) {
    if (this.isNot('connected')) {
      var notConnectedErrorMsg = 'Cannot call forceUnpublish(). You are not ' +
                                 'connected to the session.';
      dispatchError(OT.ExceptionCodes.NOT_CONNECTED, notConnectedErrorMsg, completionHandler);
      return;
    }

    var notPermittedErrorMsg = 'This token does not allow forceUnpublish. ' +
      'The role must be at least `moderator` to enable this functionality';

    if (permittedTo('forceUnpublish')) {
      var stream = typeof streamOrStreamId === 'string' ?
        this.streams.get(streamOrStreamId) : streamOrStreamId;

      _socket.forceUnpublish(stream.id, function(err) {
        if (err) {
          dispatchError(OT.ExceptionCodes.UNABLE_TO_FORCE_UNPUBLISH,
            notPermittedErrorMsg, completionHandler);
        } else if (completionHandler && OT.$.isFunction(completionHandler)) {
          completionHandler.apply(null, arguments);
        }
      });
    } else {
      // if this throws an error the handleJsException won't occur
      dispatchError(OT.ExceptionCodes.UNABLE_TO_FORCE_UNPUBLISH,
        notPermittedErrorMsg, completionHandler);
    }
  };

  this.isConnected = function() {
    return this.is('connected');
  };

  this.capabilities = new OT.Capabilities([]);

  /**
   * Dispatched when an archive recording of the session starts.
   *
   * @name archiveStarted
   * @event
   * @memberof Session
   * @see ArchiveEvent
   * @see <a href="http://www.tokbox.com/opentok/tutorials/archiving">Archiving overview</a>.
   */

  /**
   * Dispatched when an archive recording of the session stops.
   *
   * @name archiveStopped
   * @event
   * @memberof Session
   * @see ArchiveEvent
   * @see <a href="http://www.tokbox.com/opentok/tutorials/archiving">Archiving overview</a>.
   */

  /**
   * Dispatched when a new client (including your own) has connected to the session, and for
   * every client in the session when you first connect. (The Session object also dispatches
   * a <code>sessionConnected</code> event when your local client connects.)
   *
   * @name connectionCreated
   * @event
   * @memberof Session
   * @see ConnectionEvent
   * @see <a href="OT.html#initSession">OT.initSession()</a>
   */

  /**
   * A client, other than your own, has disconnected from the session.
   * @name connectionDestroyed
   * @event
   * @memberof Session
   * @see ConnectionEvent
   */

  /**
   * The page has connected to an OpenTok session. This event is dispatched asynchronously
   * in response to a successful call to the <code>connect()</code> method of a Session
   * object. Before calling the <code>connect()</code> method, initialize the session by
   * calling the <code>OT.initSession()</code> method. For a code example and more details,
   * see <a href="#connect">Session.connect()</a>.
   * @name sessionConnected
   * @event
   * @memberof Session
   * @see SessionConnectEvent
   * @see <a href="#connect">Session.connect()</a>
   * @see <a href="OT.html#initSession">OT.initSession()</a>
   */

  /**
   * The client has disconnected from the session. This event may be dispatched asynchronously
   * in response to a successful call to the <code>disconnect()</code> method of the Session object.
   * The event may also be disptached if a session connection is lost inadvertantly, as in the case
   * of a lost network connection.
   * <p>
   * The default behavior is that all Subscriber objects are unsubscribed and removed from the
   * HTML DOM. Each Subscriber object dispatches a <code>destroyed</code> event when the element is
   * removed from the HTML DOM. If you call the <code>preventDefault()</code> method in the event
   * listener for the <code>sessionDisconnect</code> event, the default behavior is prevented, and
   * you can, optionally, clean up Subscriber objects using your own code.
  *
   * @name sessionDisconnected
   * @event
   * @memberof Session
   * @see <a href="#disconnect">Session.disconnect()</a>
   * @see <a href="#disconnect">Session.forceDisconnect()</a>
   * @see SessionDisconnectEvent
   */

  /**
   * A new stream, published by another client, has been created on this session. For streams
   * published by your own client, the Publisher object dispatches a <code>streamCreated</code>
   * event. For a code example and more details, see {@link StreamEvent}.
   * @name streamCreated
   * @event
   * @memberof Session
   * @see StreamEvent
   * @see <a href="Session.html#publish">Session.publish()</a>
   */

  /**
   * A stream from another client has stopped publishing to the session.
   * <p>
   * The default behavior is that all Subscriber objects that are subscribed to the stream are
   * unsubscribed and removed from the HTML DOM. Each Subscriber object dispatches a
   * <code>destroyed</code> event when the element is removed from the HTML DOM. If you call the
   * <code>preventDefault()</code> method in the event listener for the
   * <code>streamDestroyed</code> event, the default behavior is prevented and you can clean up
   * Subscriber objects using your own code. See
   * <a href="Session.html#getSubscribersForStream">Session.getSubscribersForStream()</a>.
   * <p>
   * For streams published by your own client, the Publisher object dispatches a
   * <code>streamDestroyed</code> event.
   * <p>
   * For a code example and more details, see {@link StreamEvent}.
   * @name streamDestroyed
   * @event
   * @memberof Session
   * @see StreamEvent
   */

  /**
   * Defines an event dispatched when property of a stream has changed. This can happen in
   * in the following conditions:
   * <p>
   * <ul>
   *   <li> A stream has started or stopped publishing audio or video (see
   *     <a href="Publisher.html#publishAudio">Publisher.publishAudio()</a> and
   *     <a href="Publisher.html#publishVideo">Publisher.publishVideo()</a>). Note
   *     that a subscriber's video can be disabled or enabled for reasons other than
   *     the publisher disabling or enabling it. A Subscriber object dispatches
   *     <code>videoDisabled</code> and <code>videoEnabled</code> events in all
   *     conditions that cause the subscriber's stream to be disabled or enabled.
   *   </li>
   *   <li> The <code>videoDimensions</code> property of the Stream object has
   *     changed (see <a href="Stream.html#properties">Stream.videoDimensions</a>).
   *   </li>
   *   <li> The <code>videoType</code> property of the Stream object has changed.
   *     This can happen in a stream published by a mobile device. (See
   *     <a href="Stream.html#properties">Stream.videoType</a>.)
   *   </li>
   * </ul>
   *
   * @name streamPropertyChanged
   * @event
   * @memberof Session
   * @see StreamPropertyChangedEvent
   * @see <a href="Publisher.html#publishAudio">Publisher.publishAudio()</a>
   * @see <a href="Publisher.html#publishVideo">Publisher.publishVideo()</a>
   * @see <a href="Stream.html#"hasAudio>Stream.hasAudio</a>
   * @see <a href="Stream.html#"hasVideo>Stream.hasVideo</a>
   * @see <a href="Stream.html#"videoDimensions>Stream.videoDimensions</a>
   * @see <a href="Subscriber.html#event:videoDisabled">Subscriber videoDisabled event</a>
   * @see <a href="Subscriber.html#event:videoEnabled">Subscriber videoEnabled event</a>
   */

  /**
   * A signal was received from the session. The <a href="SignalEvent.html">SignalEvent</a>
   * class defines this event object. It includes the following properties:
   * <ul>
   *   <li><code>data</code> &mdash; (String) The data string sent with the signal (if there
   *       is one).</li>
   *   <li><code>from</code> &mdash; (<a href="Connection.html">Connection</a>) The Connection
   *       corresponding to the client that sent with the signal.</li>
   *   <li><code>type</code> &mdash; (String) The type assigned to the signal (if there is
   *       one).</li>
   * </ul>
   * <p>
   * You can register to receive all signals sent in the session, by adding an event handler
   * for the <code>signal</code> event. For example, the following code adds an event handler
   * to process all signals sent in the session:
   * <pre>
   * session.on("signal", function(event) {
   *   console.log("Signal sent from connection: " + event.from.id);
   *   console.log("Signal data: " + event.data);
   * });
   * </pre>
   * <p>You can register for signals of a specfied type by adding an event handler for the
   * <code>signal:type</code> event (replacing <code>type</code> with the actual type string
   * to filter on).
   *
   * @name signal
   * @event
   * @memberof Session
   * @see <a href="Session.html#signal">Session.signal()</a>
   * @see SignalEvent
   * @see <a href="#event:signal:type">signal:type</a> event
   */

  /**
   * A signal of the specified type was received from the session. The
   * <a href="SignalEvent.html">SignalEvent</a> class defines this event object.
   * It includes the following properties:
   * <ul>
   *   <li><code>data</code> &mdash; (String) The data string sent with the signal.</li>
   *   <li><code>from</code> &mdash; (<a href="Connection.html">Connection</a>) The Connection
   *   corresponding to the client that sent with the signal.</li>
   *   <li><code>type</code> &mdash; (String) The type assigned to the signal (if there is one).
   *   </li>
   * </ul>
   * <p>
   * You can register for signals of a specfied type by adding an event handler for the
   * <code>signal:type</code> event (replacing <code>type</code> with the actual type string
   * to filter on). For example, the following code adds an event handler for signals of
   * type "foo":
   * <pre>
   * session.on("signal:foo", function(event) {
   *   console.log("foo signal sent from connection " + event.from.id);
   *   console.log("Signal data: " + event.data);
   * });
   * </pre>
   * <p>
   * You can register to receive <i>all</i> signals sent in the session, by adding an event
   * handler for the <code>signal</code> event.
   *
   * @name signal:type
   * @event
   * @memberof Session
   * @see <a href="Session.html#signal">Session.signal()</a>
   * @see SignalEvent
   * @see <a href="#event:signal">signal</a> event
   */
};

// tb_require('../helpers/helpers.js')
// tb_require('../helpers/lib/get_user_media.js')
// tb_require('../helpers/lib/widget_view.js')
// tb_require('./analytics.js')
// tb_require('./events.js')
// tb_require('./system_requirements.js')
// tb_require('./stylable_component.js')
// tb_require('./stream.js')
// tb_require('./connection.js')
// tb_require('./publishing_state.js')
// tb_require('./environment_loader.js')
// tb_require('./audio_context.js')
// tb_require('./chrome/chrome.js')
// tb_require('./chrome/backing_bar.js')
// tb_require('./chrome/name_panel.js')
// tb_require('./chrome/mute_button.js')
// tb_require('./chrome/archiving.js')
// tb_require('./chrome/audio_level_meter.js')
// tb_require('./peer_connection/publisher_peer_connection.js')
// tb_require('./screensharing/register.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT, Promise */

// The default constraints
var defaultConstraints = {
  audio: true,
  video: true
};

/**
 * The Publisher object  provides the mechanism through which control of the
 * published stream is accomplished. Calling the <code>OT.initPublisher()</code> method
 * creates a Publisher object. </p>
 *
 *  <p>The following code instantiates a session, and publishes an audio-video stream
 *  upon connection to the session: </p>
 *
 *  <pre>
 *  var apiKey = ""; // Replace with your API key. See https://dashboard.tokbox.com/projects
 *  var sessionID = ""; // Replace with your own session ID.
 *                      // See https://dashboard.tokbox.com/projects
 *  var token = ""; // Replace with a generated token that has been assigned the moderator role.
 *                  // See https://dashboard.tokbox.com/projects
 *
 *  var session = OT.initSession(apiKey, sessionID);
 *  session.on("sessionConnected", function(event) {
 *    // This example assumes that a DOM element with the ID 'publisherElement' exists
 *    var publisherProperties = {width: 400, height:300, name:"Bob's stream"};
 *    publisher = OT.initPublisher('publisherElement', publisherProperties);
 *    session.publish(publisher);
 *  });
 *  session.connect(token);
 *  </pre>
 *
 *      <p>This example creates a Publisher object and adds its video to a DOM element
 *      with the ID <code>publisherElement</code> by calling the <code>OT.initPublisher()</code>
 *      method. It then publishes a stream to the session by calling
 *      the <code>publish()</code> method of the Session object.</p>
 *
 * @property {Boolean} accessAllowed Whether the user has granted access to the camera
 * and microphone. The Publisher object dispatches an <code>accessAllowed</code> event when
 * the user grants access. The Publisher object dispatches an <code>accessDenied</code> event
 * when the user denies access.
 * @property {Element} element The HTML DOM element containing the Publisher.
 * @property {String} id The DOM ID of the Publisher.
 * @property {Stream} stream The {@link Stream} object corresponding the the stream of
 * the Publisher.
 * @property {Session} session The {@link Session} to which the Publisher belongs.
 *
 * @see <a href="OT.html#initPublisher">OT.initPublisher</a>
 * @see <a href="Session.html#publish">Session.publish()</a>
 *
 * @class Publisher
 * @augments EventDispatcher
 */
OT.Publisher = function(options) {
  // Check that the client meets the minimum requirements, if they don't the upgrade
  // flow will be triggered.
  if (!OT.checkSystemRequirements()) {
    OT.upgradeSystemRequirements();
    return;
  }

  var _guid = OT.Publisher.nextId(),
      _domId,
      _widgetView,
      _targetElement,
      _stream,
      _streamId,
      _webRTCStream,
      _session,
      _peerConnections = {},
      _loaded = false,
      _publishStartTime,
      _microphone,
      _chrome,
      _audioLevelMeter,
      _properties,
      _validResolutions,
      _validFrameRates = [1, 7, 15, 30],
      _prevStats,
      _state,
      _iceServers,
      _audioLevelCapable = OT.$.hasCapabilities('webAudio'),
      _audioLevelSampler,
      _isScreenSharing = options && (
        options.videoSource === 'screen' ||
        options.videoSource === 'window' ||
        options.videoSource === 'tab' ||
        options.videoSource === 'browser' ||
        options.videoSource === 'application'
      ),
      _connectivityAttemptPinger,
      _enableSimulcast,
      _publisher = this;

  _properties = OT.$.defaults(options || {}, {
    publishAudio: _isScreenSharing ? false : true,
    publishVideo: true,
    mirror: _isScreenSharing ? false : true,
    showControls: true,
    fitMode: _isScreenSharing ? 'contain' : 'cover',
    audioFallbackEnabled: _isScreenSharing ? false : true,
    maxResolution: _isScreenSharing ? { width: 1920, height: 1920 } : undefined
  });

  _validResolutions = {
    '320x240': {width: 320, height: 240},
    '640x480': {width: 640, height: 480},
    '1280x720': {width: 1280, height: 720}
  };

  _prevStats = {
    timeStamp: OT.$.now()
  };

  OT.$.eventing(this);

  if (!_isScreenSharing && _audioLevelCapable) {
    _audioLevelSampler = new OT.AnalyserAudioLevelSampler(OT.audioContext());

    var audioLevelRunner = new OT.IntervalRunner(function() {
      _audioLevelSampler.sample(function(audioInputLevel) {
        OT.$.requestAnimationFrame(function() {
          _publisher.dispatchEvent(
            new OT.AudioLevelUpdatedEvent(audioInputLevel));
        });
      });
    }, 60);

    this.on({
      'audioLevelUpdated:added': function(count) {
        if (count === 1) {
          audioLevelRunner.start();
        }
      },
      'audioLevelUpdated:removed': function(count) {
        if (count === 0) {
          audioLevelRunner.stop();
        }
      }
    });
  }

  /// Private Methods
  var logAnalyticsEvent = function(action, variation, payload, throttle) {
        OT.analytics.logEvent({
          action: action,
          variation: variation,
          payload: payload,
          sessionId: _session ? _session.sessionId : null,
          connectionId: _session &&
            _session.isConnected() ? _session.connection.connectionId : null,
          partnerId: _session ? _session.apiKey : OT.APIKEY,
          streamId: _streamId
        }, throttle);
      },

      logConnectivityEvent = function(variation, payload) {
        if (variation === 'Attempt' || !_connectivityAttemptPinger) {
          _connectivityAttemptPinger = new OT.ConnectivityAttemptPinger({
            action: 'Publish',
            sessionId: _session ? _session.sessionId : null,
            connectionId: _session &&
              _session.isConnected() ? _session.connection.connectionId : null,
            partnerId: _session ? _session.apiKey : OT.APIKEY,
            streamId: _streamId
          });
        }
        if (variation === 'Failure' && payload.reason !== 'Non-fatal') {
          // We don't want to log an invalid sequence in this case because it was a
          // non-fatal failure
          _connectivityAttemptPinger.setVariation(variation);
        }
        logAnalyticsEvent('Publish', variation, payload);
      },

      recordQOS = OT.$.bind(function(connection, parsedStats) {
        var QoSBlob = {
          streamType: 'WebRTC',
          sessionId: _session ? _session.sessionId : null,
          connectionId: _session && _session.isConnected() ?
            _session.connection.connectionId : null,
          partnerId: _session ? _session.apiKey : OT.APIKEY,
          streamId: _streamId,
          width: _widgetView ? Number(OT.$.width(_widgetView.domElement).replace('px', ''))
            : undefined,
          height: _widgetView ? Number(OT.$.height(_widgetView.domElement).replace('px', ''))
            : undefined,
          version: OT.properties.version,
          mediaServerName: _session ? _session.sessionInfo.messagingServer : null,
          p2pFlag: _session ? _session.sessionInfo.p2pEnabled : false,
          duration: _publishStartTime ? new Date().getTime() - _publishStartTime.getTime() : 0,
          remoteConnectionId: connection.id,
          scalableVideo: !!_enableSimulcast
        };
        OT.analytics.logQOS(OT.$.extend(QoSBlob, parsedStats));
        this.trigger('qos', parsedStats);
      }, this),

      // Returns the video dimensions. Which could either be the ones that
      // the developer specific in the videoDimensions property, or just
      // whatever the video element reports.
      //
      // If all else fails then we'll just default to 640x480
      //
      getVideoDimensions = function() {
        var streamWidth;
        var streamHeight;

        // We set the streamWidth and streamHeight to be the minimum of the requested
        // resolution and the actual resolution.
        if (_properties.videoDimensions) {
          streamWidth = Math.min(_properties.videoDimensions.width,
            _targetElement.videoWidth() || 640);
          streamHeight = Math.min(_properties.videoDimensions.height,
            _targetElement.videoHeight() || 480);
        } else {
          streamWidth = _targetElement.videoWidth() || 640;
          streamHeight = _targetElement.videoHeight() || 480;
        }

        return {
          width: streamWidth,
          height: streamHeight
        };
      },

      /// Private Events

      stateChangeFailed = function(changeFailed) {
        OT.error('Publisher State Change Failed: ', changeFailed.message);
        OT.debug(changeFailed);
      },

      onLoaded = OT.$.bind(function() {
        if (_state.isDestroyed()) {
          // The publisher was destroyed before loading finished
          return;
        }

        OT.debug('OT.Publisher.onLoaded');

        _state.set('MediaBound');

        // If we have a session and we haven't created the stream yet then
        // wait until that is complete before hiding the loading spinner
        _widgetView.loading(this.session ? !_stream : false);

        _loaded = true;

        createChrome.call(this);

        this.trigger('initSuccess');
        this.trigger('loaded', this);
      }, this),

      onLoadFailure = OT.$.bind(function(reason) {
        var errorCode = OT.ExceptionCodes.P2P_CONNECTION_FAILED;
        var payload = {
          reason: 'Publisher PeerConnection Error: ',
          code: errorCode,
          message: reason
        };
        logConnectivityEvent('Failure', payload);

        _state.set('Failed');
        this.trigger('publishComplete', new OT.Error(errorCode,
          'Publisher PeerConnection Error: ' + reason));

        OT.handleJsException('Publisher PeerConnection Error: ' + reason,
          OT.ExceptionCodes.P2P_CONNECTION_FAILED, {
          session: _session,
          target: this
        });
      }, this),

      onStreamAvailable = OT.$.bind(function(webOTStream) {
        OT.debug('OT.Publisher.onStreamAvailable');

        _state.set('BindingMedia');

        cleanupLocalStream();
        _webRTCStream = webOTStream;

        _microphone = new OT.Microphone(_webRTCStream, !_properties.publishAudio);
        this.publishVideo(_properties.publishVideo &&
          _webRTCStream.getVideoTracks().length > 0);

        this.accessAllowed = true;
        this.dispatchEvent(new OT.Event(OT.Event.names.ACCESS_ALLOWED, false));

        var videoContainerOptions = {
          muted: true,
          error: onVideoError
        };

        _targetElement = _widgetView.bindVideo(_webRTCStream,
                                          videoContainerOptions,
                                          function(err) {
          if (err) {
            onLoadFailure(err);
            return;
          }

          onLoaded();
        });

        if (_audioLevelSampler && _webRTCStream.getAudioTracks().length > 0) {
          _audioLevelSampler.webRTCStream(_webRTCStream);
        }

      }, this),

      onStreamAvailableError = OT.$.bind(function(error) {
        OT.error('OT.Publisher.onStreamAvailableError ' + error.name + ': ' + error.message);

        _state.set('Failed');
        this.trigger('publishComplete', new OT.Error(OT.ExceptionCodes.UNABLE_TO_PUBLISH,
            error.message));

        if (_widgetView) _widgetView.destroy();

        var payload = {
          reason: 'GetUserMedia',
          code: OT.ExceptionCodes.UNABLE_TO_PUBLISH,
          message: 'Publisher failed to access camera/mic: ' + error.message
        };
        logConnectivityEvent('Failure', payload);

        OT.handleJsException(payload.reason,
          payload.code, {
          session: _session,
          target: this
        });
      }, this),

      onScreenSharingError = OT.$.bind(function(error) {
        OT.error('OT.Publisher.onScreenSharingError ' + error.message);
        _state.set('Failed');

        this.trigger('publishComplete', new OT.Error(OT.ExceptionCodes.UNABLE_TO_PUBLISH,
          'Screensharing: ' + error.message));

        var payload = {
          reason: 'ScreenSharing',
          message:error.message
        };
        logConnectivityEvent('Failure', payload);
      }, this),

      // The user has clicked the 'deny' button the the allow access dialog
      // (or it's set to always deny)
      onAccessDenied = OT.$.bind(function(error) {
        if (_isScreenSharing) {
          if (window.location.protocol !== 'https:') {
            /**
             * in http:// the browser will deny permission without asking the
             * user. There is also no way to tell if it was denied by the
             * user, or prevented from the browser.
             */
            error.message += ' Note: https:// is required for screen sharing.';
          }
        }

        OT.error('OT.Publisher.onStreamAvailableError Permission Denied');

        _state.set('Failed');
        var errorMessage = 'Publisher Access Denied: Permission Denied' +
            (error.message ? ': ' + error.message : '');
        var errorCode = OT.ExceptionCodes.UNABLE_TO_PUBLISH;
        this.trigger('publishComplete', new OT.Error(errorCode, errorMessage));

        var payload = {
          reason: 'GetUserMedia',
          code: errorCode,
          message: errorMessage
        };
        logConnectivityEvent('Failure', payload);

        this.dispatchEvent(new OT.Event(OT.Event.names.ACCESS_DENIED));
      }, this),

      onAccessDialogOpened = OT.$.bind(function() {
        logAnalyticsEvent('accessDialog', 'Opened');

        this.dispatchEvent(new OT.Event(OT.Event.names.ACCESS_DIALOG_OPENED, true));
      }, this),

      onAccessDialogClosed = OT.$.bind(function() {
        logAnalyticsEvent('accessDialog', 'Closed');

        this.dispatchEvent(new OT.Event(OT.Event.names.ACCESS_DIALOG_CLOSED, false));
      }, this),

      onVideoError = OT.$.bind(function(errorCode, errorReason) {
        OT.error('OT.Publisher.onVideoError');

        var message = errorReason + (errorCode ? ' (' + errorCode + ')' : '');
        logAnalyticsEvent('stream', null, {reason:'Publisher while playing stream: ' + message});

        _state.set('Failed');

        if (_state.isAttemptingToPublish()) {
          this.trigger('publishComplete', new OT.Error(OT.ExceptionCodes.UNABLE_TO_PUBLISH,
              message));
        } else {
          this.trigger('error', message);
        }

        OT.handleJsException('Publisher error playing stream: ' + message,
        OT.ExceptionCodes.UNABLE_TO_PUBLISH, {
          session: _session,
          target: this
        });
      }, this),

      onPeerDisconnected = OT.$.bind(function(peerConnection) {
        OT.debug('OT.Subscriber has been disconnected from the Publisher\'s PeerConnection');

        this.cleanupSubscriber(peerConnection.remoteConnection().id);
      }, this),

      onPeerConnectionFailure = OT.$.bind(function(code, reason, peerConnection, prefix) {
        var errorCode;
        if (prefix === 'ICEWorkflow') {
          errorCode = OT.ExceptionCodes.PUBLISHER_ICE_WORKFLOW_FAILED;
        } else {
          errorCode = OT.ExceptionCodes.UNABLE_TO_PUBLISH;
        }
        var payload = {
          reason: prefix ? prefix : 'PeerConnectionError',
          code: errorCode,
          message: (prefix ? prefix : '') + ':Publisher PeerConnection with connection ' +
            (peerConnection && peerConnection.remoteConnection &&
            peerConnection.remoteConnection().id) + ' failed: ' + reason,
          hasRelayCandidates: peerConnection.hasRelayCandidates()
        };
        if (_state.isPublishing()) {
          // We're already publishing so this is a Non-fatal failure, must be p2p and one of our
          // peerconnections failed
          payload.reason = 'Non-fatal';
        } else {
          this.trigger('publishComplete', new OT.Error(OT.ExceptionCodes.UNABLE_TO_PUBLISH,
              payload.message));
        }
        logConnectivityEvent('Failure', payload);

        OT.handleJsException('Publisher PeerConnection Error: ' + reason, errorCode, {
          session: _session,
          target: this
        });

        // We don't call cleanupSubscriber as it also logs a
        // disconnected analytics event, which we don't want in this
        // instance. The duplication is crufty though and should
        // be tidied up.

        delete _peerConnections[peerConnection.remoteConnection().id];
      }, this),

      /// Private Helpers

      // Assigns +stream+ to this publisher. The publisher listens
      // for a bunch of events on the stream so it can respond to
      // changes.
      assignStream = OT.$.bind(function(stream) {
        this.stream = _stream = stream;
        _stream.on('destroyed', this.disconnect, this);

        _state.set('Publishing');
        _widgetView.loading(!_loaded);
        _publishStartTime = new Date();

        this.trigger('publishComplete', null, this);

        this.dispatchEvent(new OT.StreamEvent('streamCreated', stream, null, false));

        var payload = {
          streamType: 'WebRTC'
        };
        logConnectivityEvent('Success', payload);
      }, this),

      // Clean up our LocalMediaStream
      cleanupLocalStream = function() {
        if (_webRTCStream) {
          // Stop revokes our access cam and mic access for this instance
          // of localMediaStream.
          if (_webRTCStream.stop) {
            // Older spec
            _webRTCStream.stop();
          } else {
            // Newer spec
            var tracks = _webRTCStream.getAudioTracks().concat(_webRTCStream.getVideoTracks());
            tracks.forEach(function(track) {
              track.stop();
            });
          }

          _webRTCStream = null;
        }
      },

      createPeerConnectionForRemote = OT.$.bind(function(remoteConnection, completion) {
        var peerConnection = _peerConnections[remoteConnection.id];

        if (!peerConnection) {
          var startConnectingTime = OT.$.now();

          logAnalyticsEvent('createPeerConnection', 'Attempt');

          // Cleanup our subscriber when they disconnect
          remoteConnection.on('destroyed',
            OT.$.bind(this.cleanupSubscriber, this, remoteConnection.id));

          // Calculate the number of streams to use. 1 for normal, >1 for Simulcast
          var numberOfSimulcastStreams = 1;

          _enableSimulcast = false;
          if (OT.$.env.name === 'Chrome' && !_isScreenSharing &&
            !_session.sessionInfo.p2pEnabled &&
            _properties.constraints.video) {
            // We only support simulcast on Chrome, and when not using
            // screensharing.
            _enableSimulcast = _session.sessionInfo.simulcast;

            if (_enableSimulcast === void 0) {
              // If there is no session wide preference then allow the
              // developer to choose.
              _enableSimulcast = options && options._enableSimulcast;
            }
          }

          if (_enableSimulcast) {
            var streamDimensions = getVideoDimensions();
            // HD and above gets three streams. Otherwise they get 2.
            if (streamDimensions.width > 640 &&
                streamDimensions.height > 480) {
              numberOfSimulcastStreams = 3;
            } else {
              numberOfSimulcastStreams = 2;
            }
          }

          peerConnection = _peerConnections[remoteConnection.id] = new OT.PublisherPeerConnection(
            remoteConnection,
            _session,
            _streamId,
            _webRTCStream,
            _properties.channels,
            numberOfSimulcastStreams
          );

          peerConnection.on({
            connected: function() {
              var payload = {
                pcc: parseInt(OT.$.now() - startConnectingTime, 10),
                hasRelayCandidates: peerConnection.hasRelayCandidates()
              };
              logAnalyticsEvent('createPeerConnection', 'Success', payload);
            },
            disconnected: onPeerDisconnected,
            error: onPeerConnectionFailure,
            qos: recordQOS
          }, this);

          peerConnection.init(_iceServers, completion);
        } else {
          OT.$.callAsync(function() {
            completion(undefined, peerConnection);
          });
        }
      }, this),

      /// Chrome

      // If mode is false, then that is the mode. If mode is true then we'll
      // definitely display  the button, but we'll defer the model to the
      // Publishers buttonDisplayMode style property.
      chromeButtonMode = function(mode) {
        if (mode === false) return 'off';

        var defaultMode = this.getStyle('buttonDisplayMode');

        // The default model is false, but it's overridden by +mode+ being true
        if (defaultMode === false) return 'on';

        // defaultMode is either true or auto.
        return defaultMode;
      },

      updateChromeForStyleChange = function(key, value) {
        if (!_chrome) return;

        switch (key) {
          case 'nameDisplayMode':
            _chrome.name.setDisplayMode(value);
            _chrome.backingBar.setNameMode(value);
            break;

          case 'showArchiveStatus':
            logAnalyticsEvent('showArchiveStatus', 'styleChange', {mode: value ? 'on' : 'off'});
            _chrome.archive.setShowArchiveStatus(value);
            break;

          case 'buttonDisplayMode':
            _chrome.muteButton.setDisplayMode(value);
            _chrome.backingBar.setMuteMode(value);
            break;

          case 'audioLevelDisplayMode':
            _chrome.audioLevel.setDisplayMode(value);
            break;

          case 'backgroundImageURI':
            _widgetView.setBackgroundImageURI(value);
        }
      },

      createChrome = function() {

        if (!this.getStyle('showArchiveStatus')) {
          logAnalyticsEvent('showArchiveStatus', 'createChrome', {mode: 'off'});
        }

        var widgets = {
          backingBar: new OT.Chrome.BackingBar({
            nameMode: !_properties.name ? 'off' : this.getStyle('nameDisplayMode'),
            muteMode: chromeButtonMode.call(this, this.getStyle('buttonDisplayMode'))
          }),

          name: new OT.Chrome.NamePanel({
            name: _properties.name,
            mode: this.getStyle('nameDisplayMode')
          }),

          muteButton: new OT.Chrome.MuteButton({
            muted: _properties.publishAudio === false,
            mode: chromeButtonMode.call(this, this.getStyle('buttonDisplayMode'))
          }),

          archive: new OT.Chrome.Archiving({
            show: this.getStyle('showArchiveStatus'),
            archiving: false
          })
        };

        if (_audioLevelCapable) {
          var audioLevelTransformer = new OT.AudioLevelTransformer();

          var audioLevelUpdatedHandler = function(evt) {
            _audioLevelMeter.setValue(audioLevelTransformer.transform(evt.audioLevel));
          };

          _audioLevelMeter = new OT.Chrome.AudioLevelMeter({
            mode: this.getStyle('audioLevelDisplayMode'),
            onActivate: function() {
              _publisher.on('audioLevelUpdated', audioLevelUpdatedHandler);
            },
            onPassivate: function() {
              _publisher.off('audioLevelUpdated', audioLevelUpdatedHandler);
            }
          });

          widgets.audioLevel = _audioLevelMeter;
        }

        _chrome = new OT.Chrome({
          parent: _widgetView.domElement
        }).set(widgets).on({
          muted: OT.$.bind(this.publishAudio, this, false),
          unmuted: OT.$.bind(this.publishAudio, this, true)
        });

        updateAudioLevelMeterDisplay();
      },

      reset = OT.$.bind(function() {
        if (_chrome) {
          _chrome.destroy();
          _chrome = null;
        }

        this.disconnect();

        _microphone = null;

        if (_targetElement) {
          _targetElement.destroy();
          _targetElement = null;
        }

        cleanupLocalStream();

        if (_widgetView) {
          _widgetView.destroy();
          _widgetView = null;
        }

        if (_session) {
          this._.unpublishFromSession(_session, 'reset');
        }

        this.id = _domId = null;
        this.stream = _stream = null;
        _loaded = false;

        this.session = _session = null;

        if (!_state.isDestroyed()) _state.set('NotPublishing');
      }, this);

  OT.StylableComponent(this, {
    showArchiveStatus: true,
    nameDisplayMode: 'auto',
    buttonDisplayMode: 'auto',
    audioLevelDisplayMode: _isScreenSharing ? 'off' : 'auto',
    backgroundImageURI: null
  }, _properties.showControls, function(payload) {
    logAnalyticsEvent('SetStyle', 'Publisher', payload, 0.1);
  });

  var updateAudioLevelMeterDisplay = function() {
    if (_audioLevelMeter && _publisher.getStyle('audioLevelDisplayMode') === 'auto') {
      if (!_properties.publishVideo && _properties.publishAudio) {
        _audioLevelMeter.show();
      } else {
        _audioLevelMeter.hide();
      }
    }
  };

  var setAudioOnly = function(audioOnly) {
    if (_widgetView) {
      _widgetView.audioOnly(audioOnly);
      _widgetView.showPoster(audioOnly);
    }

    updateAudioLevelMeterDisplay();
  };

  this.publish = function(targetElement) {
    OT.debug('OT.Publisher: publish');

    if (_state.isAttemptingToPublish() || _state.isPublishing()) {
      reset();
    }
    _state.set('GetUserMedia');

    if (!_properties.constraints) {
      _properties.constraints = OT.$.clone(defaultConstraints);

      if (_isScreenSharing) {
        if (_properties.audioSource != null) {
          OT.warn('Invalid audioSource passed to Publisher - when using screen sharing no ' +
            'audioSource may be used');
        }
        _properties.audioSource = null;
      }

      if (_properties.audioSource === null || _properties.audioSource === false) {
        _properties.constraints.audio = false;
        _properties.publishAudio = false;
      } else {
        if (typeof _properties.audioSource === 'object') {
          if (_properties.audioSource.deviceId != null) {
            _properties.audioSource = _properties.audioSource.deviceId;
          } else {
            OT.warn('Invalid audioSource passed to Publisher. Expected either a device ID');
          }
        }

        if (_properties.audioSource) {
          if (typeof _properties.constraints.audio !== 'object') {
            _properties.constraints.audio = {};
          }
          if (!_properties.constraints.audio.mandatory) {
            _properties.constraints.audio.mandatory = {};
          }
          if (!_properties.constraints.audio.optional) {
            _properties.constraints.audio.optional = [];
          }
          _properties.constraints.audio.mandatory.sourceId =
            _properties.audioSource;
        }
      }

      if (_properties.videoSource === null || _properties.videoSource === false) {
        _properties.constraints.video = false;
        _properties.publishVideo = false;
      } else {

        if (typeof _properties.videoSource === 'object' &&
          _properties.videoSource.deviceId == null) {
          OT.warn('Invalid videoSource passed to Publisher. Expected either a device ' +
            'ID or device.');
          _properties.videoSource = null;
        }

        var _setupVideoDefaults = function() {
          if (typeof _properties.constraints.video !== 'object') {
            _properties.constraints.video = {};
          }
          if (!_properties.constraints.video.mandatory) {
            _properties.constraints.video.mandatory = {};
          }
          if (!_properties.constraints.video.optional) {
            _properties.constraints.video.optional = [];
          }
        };

        if (_properties.videoSource) {
          _setupVideoDefaults();

          var mandatory = _properties.constraints.video.mandatory;

          // _isScreenSharing is handled by the extension helpers
          if (!_isScreenSharing) {
            if (_properties.videoSource.deviceId != null) {
              mandatory.sourceId = _properties.videoSource.deviceId;
            } else {
              mandatory.sourceId = _properties.videoSource;
            }
          }
        }

        if (_properties.resolution) {
          if (!_validResolutions.hasOwnProperty(_properties.resolution)) {
            OT.warn('Invalid resolution passed to the Publisher. Got: ' +
              _properties.resolution + ' expecting one of "' +
              OT.$.keys(_validResolutions).join('","') + '"');
          } else {
            _properties.videoDimensions = _validResolutions[_properties.resolution];
            _setupVideoDefaults();
            if (OT.$.env.name === 'Chrome') {
              _properties.constraints.video.optional =
                _properties.constraints.video.optional.concat([
                  {minWidth: _properties.videoDimensions.width},
                  {maxWidth: _properties.videoDimensions.width},
                  {minHeight: _properties.videoDimensions.height},
                  {maxHeight: _properties.videoDimensions.height}
                ]);
            }
            // We do not support this in Firefox yet
          }
        }

        if (_properties.maxResolution) {
          _setupVideoDefaults();
          if (_properties.maxResolution.width > 1920) {
            OT.warn('Invalid maxResolution passed to the Publisher. maxResolution.width must ' +
              'be less than or equal to 1920');
            _properties.maxResolution.width = 1920;
          }
          if (_properties.maxResolution.height > 1920) {
            OT.warn('Invalid maxResolution passed to the Publisher. maxResolution.height must ' +
              'be less than or equal to 1920');
            _properties.maxResolution.height = 1920;
          }

          _properties.videoDimensions = _properties.maxResolution;

          if (OT.$.env.name === 'Chrome') {
            _setupVideoDefaults();
            _properties.constraints.video.mandatory.maxWidth =
              _properties.videoDimensions.width;
            _properties.constraints.video.mandatory.maxHeight =
              _properties.videoDimensions.height;
          }
          // We do not support this in Firefox yet
        }

        if (_properties.frameRate !== void 0 &&
          OT.$.arrayIndexOf(_validFrameRates, _properties.frameRate) === -1) {
          OT.warn('Invalid frameRate passed to the publisher got: ' +
          _properties.frameRate + ' expecting one of ' + _validFrameRates.join(','));
          delete _properties.frameRate;
        } else if (_properties.frameRate) {
          _setupVideoDefaults();
          _properties.constraints.video.optional =
            _properties.constraints.video.optional.concat([
              { minFrameRate: _properties.frameRate },
              { maxFrameRate: _properties.frameRate }
            ]);
        }

      }

    } else {
      OT.warn('You have passed your own constraints not using ours');
    }

    if (_properties.style) {
      this.setStyle(_properties.style, null, true);
    }

    if (_properties.name) {
      _properties.name = _properties.name.toString();
    }

    _properties.classNames = 'OT_root OT_publisher';

    // Defer actually creating the publisher DOM nodes until we know
    // the DOM is actually loaded.
    OT.onLoad(function() {
      _widgetView = new OT.WidgetView(targetElement, _properties);
      _publisher.id = _domId = _widgetView.domId();
      _publisher.element = _widgetView.domElement;

      _widgetView.on('videoDimensionsChanged', function(oldValue, newValue) {
        if (_stream) {
          _stream.setVideoDimensions(newValue.width, newValue.height);
        }
        _publisher.dispatchEvent(
          new OT.VideoDimensionsChangedEvent(_publisher, oldValue, newValue)
        );
      });

      _widgetView.on('mediaStopped', function() {
        var event = new OT.MediaStoppedEvent(_publisher);

        _publisher.dispatchEvent(event, function() {
          if (!event.isDefaultPrevented()) {
            if (_session) {
              _publisher._.unpublishFromSession(_session, 'mediaStopped');
            } else {
              _publisher.destroy('mediaStopped');
            }
          }
        });
      });

      OT.$.waterfall([
        function(cb) {
          if (_isScreenSharing) {
            OT.checkScreenSharingCapability(function(response) {
              if (!response.supported) {
                onScreenSharingError(
                  new Error('Screen Sharing is not supported in this browser')
                );
              } else if (response.extensionRegistered === false) {
                onScreenSharingError(
                  new Error('Screen Sharing support in this browser requires an extension, but ' +
                    'one has not been registered.')
                );
              } else if (response.extensionInstalled === false) {
                onScreenSharingError(
                  new Error('Screen Sharing support in this browser requires an extension, but ' +
                    'the extension is not installed.')
                );
              } else {

                var helper = OT.pickScreenSharingHelper();

                if (helper.proto.getConstraintsShowsPermissionUI) {
                  onAccessDialogOpened();
                }

                helper.instance.getConstraints(options.videoSource, _properties.constraints,
                  function(err, constraints) {
                  if (helper.proto.getConstraintsShowsPermissionUI) {
                    onAccessDialogClosed();
                  }
                  if (err) {
                    if (err.message === 'PermissionDeniedError') {
                      onAccessDenied(err);
                    } else {
                      onScreenSharingError(err);
                    }
                  } else {
                    _properties.constraints = constraints;
                    cb();
                  }
                });
              }
            });
          } else {
            OT.$.shouldAskForDevices(function(devices) {
              if (!devices.video) {
                OT.warn('Setting video constraint to false, there are no video sources');
                _properties.constraints.video = false;
              }
              if (!devices.audio) {
                OT.warn('Setting audio constraint to false, there are no audio sources');
                _properties.constraints.audio = false;
              }
              cb();
            });
          }
        },

        function() {

          if (_state.isDestroyed()) {
            return;
          }

          OT.$.getUserMedia(
            _properties.constraints,
            onStreamAvailable,
            onStreamAvailableError,
            onAccessDialogOpened,
            onAccessDialogClosed,
            onAccessDenied
          );
        }

      ]);

    }, this);

    return this;
  };

  /**
  * Starts publishing audio (if it is currently not being published)
  * when the <code>value</code> is <code>true</code>; stops publishing audio
  * (if it is currently being published) when the <code>value</code> is <code>false</code>.
  *
  * @param {Boolean} value Whether to start publishing audio (<code>true</code>)
  * or not (<code>false</code>).
  *
  * @see <a href="OT.html#initPublisher">OT.initPublisher()</a>
  * @see <a href="Stream.html#hasAudio">Stream.hasAudio</a>
  * @see StreamPropertyChangedEvent
  * @method #publishAudio
  * @memberOf Publisher
*/
  this.publishAudio = function(value) {
    _properties.publishAudio = value;

    if (_microphone) {
      _microphone.muted(!value);
    }

    if (_chrome) {
      _chrome.muteButton.muted(!value);
    }

    if (_session && _stream) {
      _stream.setChannelActiveState('audio', value);
    }

    updateAudioLevelMeterDisplay();

    return this;
  };

  /**
  * Starts publishing video (if it is currently not being published)
  * when the <code>value</code> is <code>true</code>; stops publishing video
  * (if it is currently being published) when the <code>value</code> is <code>false</code>.
  *
  * @param {Boolean} value Whether to start publishing video (<code>true</code>)
  * or not (<code>false</code>).
  *
  * @see <a href="OT.html#initPublisher">OT.initPublisher()</a>
  * @see <a href="Stream.html#hasVideo">Stream.hasVideo</a>
  * @see StreamPropertyChangedEvent
  * @method #publishVideo
  * @memberOf Publisher
*/
  this.publishVideo = function(value) {
    var oldValue = _properties.publishVideo;
    _properties.publishVideo = value;

    if (_session && _stream && _properties.publishVideo !== oldValue) {
      _stream.setChannelActiveState('video', value);
    }

    // We currently do this event if the value of publishVideo has not changed
    // This is because the state of the video tracks enabled flag may not match
    // the value of publishVideo at this point. This will be tidied up shortly.
    if (_webRTCStream) {
      var videoTracks = _webRTCStream.getVideoTracks();
      for (var i = 0, num = videoTracks.length; i < num; ++i) {
        videoTracks[i].setEnabled(value);
      }
    }

    setAudioOnly(!value);

    return this;
  };

  /**
  * Deletes the Publisher object and removes it from the HTML DOM.
  * <p>
  * The Publisher object dispatches a <code>destroyed</code> event when the DOM
  * element is removed.
  * </p>
  * @method #destroy
  * @memberOf Publisher
  * @return {Publisher} The Publisher.
  */
  this.destroy = function(/* unused */ reason, quiet) {
    if (_state.isAttemptingToPublish()) {
      if (_connectivityAttemptPinger) {
        _connectivityAttemptPinger.stop();
        _connectivityAttemptPinger = null;
      }
      if (_session) {
        logConnectivityEvent('Canceled');
      }
    }

    if (_state.isDestroyed()) return;
    _state.set('Destroyed');

    reset();

    if (quiet !== true) {
      this.dispatchEvent(
        new OT.DestroyedEvent(
          OT.Event.names.PUBLISHER_DESTROYED,
          this,
          reason
        ),
        OT.$.bind(this.off, this)
      );
    }

    return this;
  };

  /**
  * @methodOf Publisher
  * @private
  */
  this.disconnect = function() {
    // Close the connection to each of our subscribers
    for (var fromConnectionId in _peerConnections) {
      this.cleanupSubscriber(fromConnectionId);
    }
  };

  this.cleanupSubscriber = function(fromConnectionId) {
    var pc = _peerConnections[fromConnectionId];

    if (pc) {
      pc.destroy();
      delete _peerConnections[fromConnectionId];

      logAnalyticsEvent('disconnect', 'PeerConnection',
        {subscriberConnection: fromConnectionId});
    }
  };

  this.processMessage = function(type, fromConnection, message) {
    OT.debug('OT.Publisher.processMessage: Received ' + type + ' from ' + fromConnection.id);
    OT.debug(message);

    switch (type) {
      case 'unsubscribe':
        this.cleanupSubscriber(message.content.connection.id);
        break;

      default:
        createPeerConnectionForRemote(fromConnection, function(err, peerConnection) {
          if (err) {
            throw err;
          }

          peerConnection.processMessage(type, message);
        });
    }
  };

  /**
  * Returns the base-64-encoded string of PNG data representing the Publisher video.
  *
  *   <p>You can use the string as the value for a data URL scheme passed to the src parameter of
  *   an image file, as in the following:</p>
  *
  * <pre>
  *  var imgData = publisher.getImgData();
  *
  *  var img = document.createElement("img");
  *  img.setAttribute("src", "data:image/png;base64," + imgData);
  *  var imgWin = window.open("about:blank", "Screenshot");
  *  imgWin.document.write("&lt;body&gt;&lt;/body&gt;");
  *  imgWin.document.body.appendChild(img);
  * </pre>
  *
  * @method #getImgData
  * @memberOf Publisher
  * @return {String} The base-64 encoded string. Returns an empty string if there is no video.
  */

  this.getImgData = function() {
    if (!_loaded) {
      OT.error('OT.Publisher.getImgData: Cannot getImgData before the Publisher is publishing.');

      return null;
    }

    return _targetElement.imgData();
  };

  // API Compatibility layer for Flash Publisher, this could do with some tidyup.
  this._ = {
    publishToSession: OT.$.bind(function(session) {
      // Add session property to Publisher
      this.session = _session = session;
      _streamId = OT.$.uuid();
      var createStream = function() {
        // Bail if this.session is gone, it means we were unpublished
        // before createStream could finish.
        if (!_session) return;

        _state.set('PublishingToSession');

        var onStreamRegistered = OT.$.bind(function(err, streamId, message) {
          if (err) {
            // @todo we should respect err.code here and translate it to the local
            // client equivalent.
            var errorCode,
              errorMessage,
              knownErrorCodes = [403, 404, 409];
            if (err.code && OT.$.arrayIndexOf(knownErrorCodes, err.code) > -1) {
              errorCode = OT.ExceptionCodes.UNABLE_TO_PUBLISH;
              errorMessage = err.message;
            } else {
              errorCode = OT.ExceptionCodes.UNEXPECTED_SERVER_RESPONSE;
              errorMessage = 'Unexpected server response. Try this operation again later.';
            }

            var payload = {
              reason: 'Publish',
              code: errorCode,
              message: errorMessage
            };
            logConnectivityEvent('Failure', payload);
            if (_state.isAttemptingToPublish()) {
              this.trigger('publishComplete', new OT.Error(errorCode, errorMessage));
            }

            OT.handleJsException(err.message,
              errorCode, {
              session: _session,
              target: this
            });

            return;
          }

          this.streamId = _streamId = streamId;
          _iceServers = OT.Raptor.parseIceServers(message);
        }, this);

        var streamDimensions = getVideoDimensions();
        var streamChannels = [];

        if (!(_properties.videoSource === null || _properties.videoSource === false)) {
          streamChannels.push(new OT.StreamChannel({
            id: 'video1',
            type: 'video',
            active: _properties.publishVideo,
            orientation: OT.VideoOrientation.ROTATED_NORMAL,
            frameRate: _properties.frameRate,
            width: streamDimensions.width,
            height: streamDimensions.height,
            source: _isScreenSharing ? 'screen' : 'camera',
            fitMode: _properties.fitMode
          }));
        }

        if (!(_properties.audioSource === null || _properties.audioSource === false)) {
          streamChannels.push(new OT.StreamChannel({
            id: 'audio1',
            type: 'audio',
            active: _properties.publishAudio
          }));
        }

        session._.streamCreate(_properties.name || '', _streamId,
          _properties.audioFallbackEnabled, streamChannels, onStreamRegistered);
      };

      if (_loaded) createStream.call(this);
      else this.on('initSuccess', createStream, this);

      logConnectivityEvent('Attempt', {
        streamType: 'WebRTC',
        dataChannels: _properties.channels
      });

      return this;
    }, this),

    unpublishFromSession: OT.$.bind(function(session, reason) {
      if (!_session || session.id !== _session.id) {
        OT.warn('The publisher ' + _guid + ' is trying to unpublish from a session ' +
          session.id + ' it is not attached to (it is attached to ' +
          (_session && _session.id || 'no session') + ')');
        return this;
      }

      if (session.isConnected() && this.stream) {
        session._.streamDestroy(this.stream.id);
      }

      // Disconnect immediately, rather than wait for the WebSocket to
      // reply to our destroyStream message.
      this.disconnect();
      if (_state.isAttemptingToPublish()) {
        logConnectivityEvent('Canceled');
      }
      this.session = _session = null;

      // We're back to being a stand-alone publisher again.
      if (!_state.isDestroyed()) _state.set('MediaBound');

      if (_connectivityAttemptPinger) {
        _connectivityAttemptPinger.stop();
        _connectivityAttemptPinger = null;
      }
      logAnalyticsEvent('unpublish', 'Success', {sessionId: session.id});

      this._.streamDestroyed(reason);

      return this;
    }, this),

    streamDestroyed: OT.$.bind(function(reason) {
      if (OT.$.arrayIndexOf(['reset'], reason) < 0) {
        var event = new OT.StreamEvent('streamDestroyed', _stream, reason, true);
        var defaultAction = OT.$.bind(function() {
          if (!event.isDefaultPrevented()) {
            this.destroy();
          }
        }, this);
        this.dispatchEvent(event, defaultAction);
      }
    }, this),

    archivingStatus: OT.$.bind(function(status) {
      if (_chrome) {
        _chrome.archive.setArchiving(status);
      }

      return status;
    }, this),

    webRtcStream: function() {
      return _webRTCStream;
    },

    switchTracks: function() {
      return new Promise(function(resolve, reject) {
        OT.$.getUserMedia(
          _properties.constraints,
          function(newStream) {

            cleanupLocalStream();
            _webRTCStream = newStream;

            _microphone = new OT.Microphone(_webRTCStream, !_properties.publishAudio);

            var videoContainerOptions = {
              muted: true,
              error: onVideoError
            };

            _targetElement = _widgetView.bindVideo(_webRTCStream, videoContainerOptions,
              function(err) {
                if (err) {
                  onLoadFailure(err);
                  reject(err);
                }
              });

            if (_audioLevelSampler && _webRTCStream.getAudioTracks().length > 0) {
              _audioLevelSampler.webRTCStream(_webRTCStream);
            }

            var replacePromises = [];

            Object.keys(_peerConnections).forEach(function(connectionId) {
              var peerConnection = _peerConnections[connectionId];
              peerConnection.getSenders().forEach(function(sender) {
                if (sender.track.kind === 'audio' && newStream.getAudioTracks().length) {
                  replacePromises.push(sender.replaceTrack(newStream.getAudioTracks()[0]));
                } else if (sender.track.kind === 'video' && newStream.getVideoTracks().length) {
                  replacePromises.push(sender.replaceTrack(newStream.getVideoTracks()[0]));
                }
              });
            });

            Promise.all(replacePromises).then(resolve, reject);
          },
          function(error) {
            onStreamAvailableError(error);
            reject(error);
          },
          onAccessDialogOpened,
          onAccessDialogClosed,
          function(error) {
            onAccessDenied(error);
            reject(error);
          });
      });
    },

    /**
     * @param {string=} windowId
     */
    switchAcquiredWindow: function(windowId) {

      if (OT.$.env.name !== 'Firefox' || OT.$.env.version < 38) {
        throw new Error('switchAcquiredWindow is an experimental method and is not supported by' +
        'the current platform');
      }

      if (typeof windowId !== 'undefined') {
        _properties.constraints.video.browserWindow = windowId;
      }

      logAnalyticsEvent('SwitchAcquiredWindow', 'Attempt', {
        constraints: _properties.constraints
      });

      var switchTracksPromise = _publisher._.switchTracks();

      // "listening" promise completion just for analytics
      switchTracksPromise.then(function() {
        logAnalyticsEvent('SwitchAcquiredWindow', 'Success', {
          constraints: _properties.constraints
        });
      }, function(error) {
        logAnalyticsEvent('SwitchAcquiredWindow', 'Failure', {
          error: error,
          constraints: _properties.constraints
        });
      });

      return switchTracksPromise;
    },

    getDataChannel: function(label, options, completion) {
      var pc = _peerConnections[OT.$.keys(_peerConnections)[0]];

      // @fixme this will fail if it's called before we have a PublisherPeerConnection.
      // I.e. before we have a subscriber.
      if (!pc) {
        completion(new OT.$.Error('Cannot create a DataChannel before there is a subscriber.'));
        return;
      }

      pc.getDataChannel(label, options, completion);
    }
  };

  this.detectDevices = function() {
    OT.warn('Fixme: Haven\'t implemented detectDevices');
  };

  this.detectMicActivity = function() {
    OT.warn('Fixme: Haven\'t implemented detectMicActivity');
  };

  this.getEchoCancellationMode = function() {
    OT.warn('Fixme: Haven\'t implemented getEchoCancellationMode');
    return 'fullDuplex';
  };

  this.setMicrophoneGain = function() {
    OT.warn('Fixme: Haven\'t implemented setMicrophoneGain');
  };

  this.getMicrophoneGain = function() {
    OT.warn('Fixme: Haven\'t implemented getMicrophoneGain');
    return 0.5;
  };

  this.setCamera = function() {
    OT.warn('Fixme: Haven\'t implemented setCamera');
  };

  this.setMicrophone = function() {
    OT.warn('Fixme: Haven\'t implemented setMicrophone');
  };

  // Platform methods:

  this.guid = function() {
    return _guid;
  };

  this.videoElement = function() {
    return _targetElement.domElement();
  };

  this.setStream = assignStream;

  this.isWebRTC = true;

  this.isLoading = function() {
    return _widgetView && _widgetView.loading();
  };

  /**
  * Returns the width, in pixels, of the Publisher video. This may differ from the
  * <code>resolution</code> property passed in as the <code>properties</code> property
  * the options passed into the <code>OT.initPublisher()</code> method, if the browser
  * does not support the requested resolution.
  *
  * @method #videoWidth
  * @memberOf Publisher
  * @return {Number} the width, in pixels, of the Publisher video.
  */
  this.videoWidth = function() {
    return _targetElement.videoWidth();
  };

  /**
  * Returns the height, in pixels, of the Publisher video. This may differ from the
  * <code>resolution</code> property passed in as the <code>properties</code> property
  * the options passed into the <code>OT.initPublisher()</code> method, if the browser
  * does not support the requested resolution.
  *
  * @method #videoHeight
  * @memberOf Publisher
  * @return {Number} the height, in pixels, of the Publisher video.
  */
  this.videoHeight = function() {
    return _targetElement.videoHeight();
  };

  // Make read-only: element, guid, _.webRtcStream

  this.on('styleValueChanged', updateChromeForStyleChange, this);
  _state = new OT.PublishingState(stateChangeFailed);

  this.accessAllowed = false;

  /**
  * Dispatched when the user has clicked the Allow button, granting the
  * app access to the camera and microphone. The Publisher object has an
  * <code>accessAllowed</code> property which indicates whether the user
  * has granted access to the camera and microphone.
  * @see Event
  * @name accessAllowed
  * @event
  * @memberof Publisher
*/

  /**
  * Dispatched when the user has clicked the Deny button, preventing the
  * app from having access to the camera and microphone.
  * @see Event
  * @name accessDenied
  * @event
  * @memberof Publisher
*/

  /**
  * Dispatched when the Allow/Deny dialog box is opened. (This is the dialog box in which
  * the user can grant the app access to the camera and microphone.)
  * @see Event
  * @name accessDialogOpened
  * @event
  * @memberof Publisher
*/

  /**
  * Dispatched when the Allow/Deny box is closed. (This is the dialog box in which the
  * user can grant the app access to the camera and microphone.)
  * @see Event
  * @name accessDialogClosed
  * @event
  * @memberof Publisher
*/

  /**
  * Dispatched periodically to indicate the publisher's audio level. The event is dispatched
  * up to 60 times per second, depending on the browser. The <code>audioLevel</code> property
  * of the event is audio level, from 0 to 1.0. See {@link AudioLevelUpdatedEvent} for more
  * information.
  * <p>
  * The following example adjusts the value of a meter element that shows volume of the
  * publisher. Note that the audio level is adjusted logarithmically and a moving average
  * is applied:
  * <p>
  * <pre>
  * var movingAvg = null;
  * publisher.on('audioLevelUpdated', function(event) {
  *   if (movingAvg === null || movingAvg <= event.audioLevel) {
  *     movingAvg = event.audioLevel;
  *   } else {
  *     movingAvg = 0.7 * movingAvg + 0.3 * event.audioLevel;
  *   }
  *
  *   // 1.5 scaling to map the -30 - 0 dBm range to [0,1]
  *   var logLevel = (Math.log(movingAvg) / Math.LN10) / 1.5 + 1;
  *   logLevel = Math.min(Math.max(logLevel, 0), 1);
  *   document.getElementById('publisherMeter').value = logLevel;
  * });
  * </pre>
  * <p>This example shows the algorithm used by the default audio level indicator displayed
  * in an audio-only Publisher.
  *
  * @name audioLevelUpdated
  * @event
  * @memberof Publisher
  * @see AudioLevelUpdatedEvent
  */

  /**
   * The publisher has started streaming to the session.
   * @name streamCreated
   * @event
   * @memberof Publisher
   * @see StreamEvent
   * @see <a href="Session.html#publish">Session.publish()</a>
   */

  /**
   * The publisher has stopped streaming to the session. The default behavior is that
   * the Publisher object is removed from the HTML DOM). The Publisher object dispatches a
   * <code>destroyed</code> event when the element is removed from the HTML DOM. If you call the
   * <code>preventDefault()</code> method of the event object in the event listener, the default
   * behavior is prevented, and you can, optionally, retain the Publisher for reuse or clean it up
   * using your own code.
   * @name streamDestroyed
   * @event
   * @memberof Publisher
   * @see StreamEvent
   */

  /**
  * Dispatched when the Publisher element is removed from the HTML DOM. When this event
  * is dispatched, you may choose to adjust or remove HTML DOM elements related to the publisher.
  * @name destroyed
  * @event
  * @memberof Publisher
*/

  /**
  * Dispatched when the video dimensions of the video change. This can only occur in when the
  * <code>stream.videoType</code> property is set to <code>"screen"</code> (for a screen-sharing
  * video stream), and the user resizes the window being captured.
  * @name videoDimensionsChanged
  * @event
  * @memberof Publisher
*/

  /**
   * The user has stopped screen-sharing for the published stream. This event is only dispatched
   * for screen-sharing video streams.
   * @name mediaStopped
   * @event
   * @memberof Publisher
   * @see StreamEvent
   */
};

// Helper function to generate unique publisher ids
OT.Publisher.nextId = OT.$.uuid;

// tb_require('../../conf/properties.js')
// tb_require('../ot.js')
// tb_require('./session.js')
// tb_require('./publisher.js')

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global OT */

/**
* The first step in using the OpenTok API is to call the <code>OT.initSession()</code>
* method. Other methods of the OT object check for system requirements and set up error logging.
*
* @class OT
*/

/**
* <p class="mSummary">
* Initializes and returns the local session object for a specified session ID.
* </p>
* <p>
* You connect to an OpenTok session using the <code>connect()</code> method
* of the Session object returned by the <code>OT.initSession()</code> method.
* Note that calling <code>OT.initSession()</code> does not initiate communications
* with the cloud. It simply initializes the Session object that you can use to
* connect (and to perform other operations once connected).
* </p>
*
*  <p>
*    For an example, see <a href="Session.html#connect">Session.connect()</a>.
*  </p>
*
* @method OT.initSession
* @memberof OT
* @param {String} apiKey Your OpenTok API key (see <a href="https://dashboard.tokbox.com">the
* OpenTok dashboard</a>).
* @param {String} sessionId The session ID identifying the OpenTok session. For more
* information, see <a href="https://tokbox.com/opentok/tutorials/create-session/">Session
* creation</a>.
* @returns {Session} The session object through which all further interactions with
* the session will occur.
*/
OT.initSession = function(apiKey, sessionId) {

  if (sessionId == null) {
    sessionId = apiKey;
    apiKey = null;
  }

  // Allow buggy legacy behavior to succeed, where the client can connect if sessionId
  // is an array containing one element (the session ID), but fix it so that sessionId
  // is stored as a string (not an array):
  if (OT.$.isArray(sessionId) && sessionId.length === 1) {
    sessionId = sessionId[0];
  }

  var session = OT.sessions.get(sessionId);

  if (!session) {
    session = new OT.Session(apiKey, sessionId);
    OT.sessions.add(session);
  }

  return session;
};

/**
* <p class="mSummary">
*   Initializes and returns a Publisher object. You can then pass this Publisher
*   object to <code>Session.publish()</code> to publish a stream to a session.
* </p>
* <p>
*   <i>Note:</i> If you intend to reuse a Publisher object created using
*   <code>OT.initPublisher()</code> to publish to different sessions sequentially,
*   call either <code>Session.disconnect()</code> or <code>Session.unpublish()</code>.
*   Do not call both. Then call the <code>preventDefault()</code> method of the
*   <code>streamDestroyed</code> or <code>sessionDisconnected</code> event object to prevent the
*   Publisher object from being removed from the page.
* </p>
*
* @param {Object} targetElement (Optional) The DOM element or the <code>id</code> attribute of the
* existing DOM element used to determine the location of the Publisher video in the HTML DOM. See
* the <code>insertMode</code> property of the <code>properties</code> parameter. If you do not
* specify a <code>targetElement</code>, the application appends a new DOM element to the HTML
* <code>body</code>.
*
* <p>
*       The application throws an error if an element with an ID set to the
*       <code>targetElement</code> value does not exist in the HTML DOM.
* </p>
*
* @param {Object} properties (Optional) This object contains the following properties (each of which
* are optional):
* </p>
* <ul>
* <li>
*   <strong>audioFallbackEnabled</strong> (String) &#151; Whether the stream will use the
*   audio-fallback feature (<code>true</code>) or not (<code>false</code>). The audio-fallback
*   feature is available in sessions that use the the OpenTok Media Router. With the audio-fallback
*   feature enabled (the default), when the server determines that a stream's quality has degraded
*   significantly for a specific subscriber, it disables the video in that subscriber in order to
*   preserve audio quality. For streams that use a camera as a video source, the default setting is
*   <code>true</code> (the audio-fallback feature is enabled). The default setting is
*   <code>false</code> (the audio-fallback feature is disabled) for screen-sharing streams, which
*   have the <code>videoSource</code> set to <code>"screen"</code> in the
*   <code>OT.initPublisher()</code> options. For more information, see the Subscriber
*   <a href="Subscriber.html#event:videoDisabled">videoDisabled</a> event and
*   <a href="http://tokbox.com/opentok/tutorials/create-session/#media-mode">the OpenTok Media
*   Router and media modes</a>.
* </li>
* <li>
*   <strong>audioSource</strong> (String) &#151; The ID of the audio input device (such as a
*    microphone) to be used by the publisher. You can obtain a list of available devices, including
*    audio input devices, by calling the <a href="#getDevices">OT.getDevices()</a> method. Each
*    device listed by the method has a unique device ID. If you pass in a device ID that does not
*    match an existing audio input device, the call to <code>OT.initPublisher()</code> fails with an
*    error (error code 1500, "Unable to Publish") passed to the completion handler function.
*    <p>
*    If you set this property to <code>null</code> or <code>false</code>, the browser does not
*    request access to the microphone, and no audio is published.
*    </p>
* </li>
* <li>
*   <strong>fitMode</strong> (String) &#151; Determines how the video is displayed if the its
*     dimensions do not match those of the DOM element. You can set this property to one of the
*     following values:
*     <p>
*     <ul>
*       <li>
*         <code>"cover"</code> &mdash; The video is cropped if its dimensions do not match those of
*         the DOM element. This is the default setting for videos publishing a camera feed.
*       </li>
*       <li>
*         <code>"contain"</code> &mdash; The video is letter-boxed if its dimensions do not match
*         those of the DOM element. This is the default setting for screen-sharing videos.
*       </li>
*     </ul>
* </li>
* <li>
*   <strong>frameRate</strong> (Number) &#151; The desired frame rate, in frames per second,
*   of the video. Valid values are 30, 15, 7, and 1. The published stream will use the closest
*   value supported on the publishing client. The frame rate can differ slightly from the value
*   you set, depending on the browser of the client. And the video will only use the desired
*   frame rate if the client configuration supports it.
*   <br><br><p>If the publisher specifies a frame rate, the actual frame rate of the video stream
*   is set as the <code>frameRate</code> property of the Stream object, though the actual frame rate
*   will vary based on changing network and system conditions. If the developer does not specify a
*   frame rate, this property is undefined.
*   <p>
*   For sessions that use the OpenTok Media Router (sessions with
*   the <a href="http://tokbox.com/opentok/tutorials/create-session/#media-mode">media mode</a>
*   set to routed, lowering the frame rate or lowering the resolution reduces
*   the maximum bandwidth the stream can use. However, in sessions with the media mode set to
*   relayed, lowering the frame rate or resolution may not reduce the stream's bandwidth.
*   </p>
*   <p>
*   You can also restrict the frame rate of a Subscriber's video stream. To restrict the frame rate
*   a Subscriber, call the <code>restrictFrameRate()</code> method of the subscriber, passing in
*   <code>true</code>.
*   (See <a href="Subscriber.html#restrictFrameRate">Subscriber.restrictFrameRate()</a>.)
*   </p>
* </li>
* <li>
*   <strong>height</strong> (Number) &#151; The desired initial height of the displayed Publisher
*   video in the HTML page (default: 198 pixels). You can specify the number of pixels as either
*   a number (such as 300) or a string ending in "px" (such as "300px"). Or you can specify a
*   percentage of the size of the parent element, with a string ending in "%" (such as "100%").
*   <i>Note:</i> To resize the publisher video, adjust the CSS of the publisher's DOM element
*   (the <code>element</code> property of the Publisher object) or (if the height is specified as
*   a percentage) its parent DOM element (see
*   <a href="https://tokbox.com/developer/guides/customize-ui/js/#video_resize_reposition">Resizing
*   or repositioning a video</a>).
* </li>
* <li>
*   <strong>insertMode</strong> (String) &#151; Specifies how the Publisher object will be
*   inserted in the HTML DOM. See the <code>targetElement</code> parameter. This string can
*   have the following values:
*   <p>
*   <ul>
*     <li><code>"replace"</code> &#151; The Publisher object replaces contents of the
*       targetElement. This is the default.</li>
*     <li><code>"after"</code> &#151; The Publisher object is a new element inserted after
*       the targetElement in the HTML DOM. (Both the Publisher and targetElement have the
*       same parent element.)</li>
*     <li><code>"before"</code> &#151; The Publisher object is a new element inserted before
*       the targetElement in the HTML DOM. (Both the Publisher and targetElement have the same
*       parent element.)</li>
*     <li><code>"append"</code> &#151; The Publisher object is a new element added as a child
*       of the targetElement. If there are other child elements, the Publisher is appended as
*       the last child element of the targetElement.</li>
*   </ul></p>
*   <p> Do not move the publisher element or its parent elements in the DOM
*   heirarchy. Use CSS to resize or reposition the publisher video's element
*   (the <code>element</code> property of the Publisher object) or its parent element (see
*   <a href="https://tokbox.com/developer/guides/customize-ui/js/#video_resize_reposition">Resizing
*   or repositioning a video</a>.</p>
* </li>
* <li>
*   <strong>maxResolution</strong> (Object) &#151; Sets the maximum resoultion to stream.
*   This setting only applies to when the <code>videoSource</code> property is set to
*   <code>"screen"</code> (when the publisher is screen-sharing). The resolution of the
*   stream will match the captured screen region unless the region is greater than the
*   <code>maxResolution</code> setting. Set this to an object that has two properties:
*   <code>width</code> and <code>height</code> (both numbers). The maximum value for each of
*   the <code>width</code> and <code>height</code> properties is 1920, and the minimum value
*   is 10.
* </li>
* <li>
*   <strong>mirror</strong> (Boolean) &#151; Whether the publisher's video image
*   is mirrored in the publisher's page. The default value is <code>true</code>
*   (the video image is mirrored), except when the <code>videoSource</code> property is set
*   to <code>"screen"</code> (in which case the default is <code>false</code>). This property
*   does not affect the display on subscribers' views of the video.
* </li>
* <li>
*   <strong>name</strong> (String) &#151; The name for this stream. The name appears at
*   the bottom of Subscriber videos. The default value is "" (an empty string). Setting
*   this to a string longer than 1000 characters results in an runtime exception.
* </li>
* <li>
*   <strong>publishAudio</strong> (Boolean) &#151; Whether to initially publish audio
*   for the stream (default: <code>true</code>). This setting applies when you pass
*   the Publisher object in a call to the <code>Session.publish()</code> method.
* </li>
* <li>
*   <strong>publishVideo</strong> (Boolean) &#151; Whether to initially publish video
*   for the stream (default: <code>true</code>). This setting applies when you pass
*   the Publisher object in a call to the <code>Session.publish()</code> method.
* </li>
* <li>
*   <strong>resolution</strong> (String) &#151; The desired resolution of the video. The format
*   of the string is <code>"widthxheight"</code>, where the width and height are represented in
*   pixels. Valid values are <code>"1280x720"</code>, <code>"640x480"</code>, and
*   <code>"320x240"</code>. The published video will only use the desired resolution if the
*   client configuration supports it.
*   <br><br><p>
*   The requested resolution of a video stream is set as the <code>videoDimensions.width</code> and
*   <code>videoDimensions.height</code> properties of the Stream object.
*   </p>
*   <p>
*   The default resolution for a stream (if you do not specify a resolution) is 640x480 pixels.
*   If the client system cannot support the resolution you requested, the the stream will use the
*   next largest setting supported.
*   </p>
*   <p>
*   The actual resolution used by the Publisher is returned by the <code>videoHeight()</code> and
*   <code>videoWidth()</code> methods of the Publisher object. The actual resolution of a
*   Subscriber video stream is returned by the <code>videoHeight()</code> and
*   <code>videoWidth()</code> properties of the Subscriber object. These may differ from the values
*   of the <code>resolution</code> property passed in as the <code>properties</code> property of the
*   <code>OT.initPublisher()</code> method, if the browser does not support the requested
*   resolution.
*   </p>
*   <p>
*   For sessions that use the OpenTok Media Router (sessions with the
*   <a href="http://tokbox.com/opentok/tutorials/create-session/#media-mode">media mode</a>
*   set to routed, lowering the frame rate or lowering the resolution reduces the maximum bandwidth
*   the stream can use. However, in sessions that have the media mode set to relayed, lowering the
*   frame rate or resolution may not reduce the stream's bandwidth.
*   </p>
* </li>
* <li>
*   <strong>showControls</strong> (Boolean) &#151; Whether to display the built-in user interface
*   controls (default: <code>true</code>) for the Publisher. These controls include the name
*   display, the audio level indicator, and the microphone control button. You can turn off all user
*   interface controls by setting this property to <code>false</code>. You can control the display
*   of individual user interface controls by leaving this property set to <code>true</code> (the
*   default) and setting individual properties of the <code>style</code> property.
* </li>
* <li>
*   <strong>style</strong> (Object) &#151; An object containing properties that define the initial
*   appearance of user interface controls of the Publisher. The <code>style</code> object includes
*   the following properties:
*     <ul>
*       <li><code>audioLevelDisplayMode</code> (String) &mdash; How to display the audio level
*       indicator. Possible values are: <code>"auto"</code> (the indicator is displayed when the
*       video is disabled), <code>"off"</code> (the indicator is not displayed), and
*       <code>"on"</code> (the indicator is always displayed).</li>
*
*       <li><code>backgroundImageURI</code> (String) &mdash; A URI for an image to display as
*       the background image when a video is not displayed. (A video may not be displayed if
*       you call <code>publishVideo(false)</code> on the Publisher object). You can pass an http
*       or https URI to a PNG, JPEG, or non-animated GIF file location. You can also use the
*       <code>data</code> URI scheme (instead of http or https) and pass in base-64-encrypted
*       PNG data, such as that obtained from the
*       <a href="Publisher.html#getImgData">Publisher.getImgData()</a> method. For example,
*       you could set the property to <code>"data:VBORw0KGgoAA..."</code>, where the portion of the
*       string after <code>"data:"</code> is the result of a call to
*       <code>Publisher.getImgData()</code>. If the URL or the image data is invalid, the property
*       is ignored (the attempt to set the image fails silently).</li>
*
*       <li><code>buttonDisplayMode</code> (String) &mdash; How to display the microphone controls
*       Possible values are: <code>"auto"</code> (controls are displayed when the stream is first
*       displayed and when the user mouses over the display), <code>"off"</code> (controls are not
*       displayed), and <code>"on"</code> (controls are always displayed).</li>
*
*       <li><code>nameDisplayMode</code> (String) &#151; Whether to display the stream name.
*       Possible values are: <code>"auto"</code> (the name is displayed when the stream is first
*       displayed and when the user mouses over the display), <code>"off"</code> (the name is not
*       displayed), and <code>"on"</code> (the name is always displayed).</li>
*   </ul>
* </li>
* <li>
*   <strong>videoSource</strong> (String) &#151; The ID of the video input device (such as a
*    camera) to be used by the publisher. You can obtain a list of available devices, including
*    video input devices, by calling the <a href="#getDevices">OT.getDevices()</a> method. Each
*    device listed by the method has a unique device ID. If you pass in a device ID that does not
*    match an existing video input device, the call to <code>OT.initPublisher()</code> fails with an
*    error (error code 1500, "Unable to Publish") passed to the completion handler function.
*    <p>
*    If you set this property to <code>null</code> or <code>false</code>, the browser does not
*    request access to the camera, and no video is published. In a voice-only call, set this
*    property to <code>null</code> or <code>false</code> for each Publisher.
*    </p>
*   <p>
*     To publish a screen-sharing stream, set this property to <code>"application"</code>,
*    <code>"screen"</code>, or <code>"window"</code>. Call
*    <a href="OT.html#checkScreenSharingCapability">OT.checkScreenSharingCapability()</a> to check
*    if screen sharing is supported. When you set the <code>videoSource</code> property to
*    <code>"screen"</code>, the following are default values for other properties:
*    <code>audioFallbackEnabled == false</code>,
*    <code>maxResolution == {width: 1920, height: 1920}</code>, <code>mirror == false</code>,
*    <code>scaleMode == "fit"</code>. Also, the default <code>scaleMode</code> setting for
*    subscribers to the stream is <code>"fit"</code>.
* </li>
* <li>
*   <strong>width</strong> (Number) &#151; The desired initial width of the displayed Publisher
*   video in the HTML page (default: 264 pixels). You can specify the number of pixels as either
*   a number (such as 400) or a string ending in "px" (such as "400px"). Or you can specify a
*   percentage of the size of the parent element, with a string ending in "%" (such as "100%").
*   <i>Note:</i> To resize the publisher video, adjust the CSS of the publisher's DOM element
*   (the <code>element</code> property of the Publisher object) or (if the width is specified as
*   a percentage) its parent DOM element (see
*   <a href="https://tokbox.com/developer/guides/customize-ui/js/#video_resize_reposition">Resizing
*   or repositioning a video</a>).
* </li>
* </ul>
* @param {Function} completionHandler (Optional) A function to be called when the method succeeds
* or fails in initializing a Publisher object. This function takes one parameter &mdash;
* <code>error</code>. On success, the <code>error</code> object is set to <code>null</code>. On
* failure, the <code>error</code> object has two properties: <code>code</code> (an integer) and
* <code>message</code> (a string), which identify the cause of the failure. The method succeeds
* when the user grants access to the camera and microphone. The method fails if the user denies
* access to the camera and microphone. The <code>completionHandler</code> function is called
* before the Publisher dispatches an <code>accessAllowed</code> (success) event or an
* <code>accessDenied</code> (failure) event.
* <p>
* The following code adds a <code>completionHandler</code> when calling the
* <code>OT.initPublisher()</code> method:
* </p>
* <pre>
* var publisher = OT.initPublisher('publisher', null, function (error) {
*   if (error) {
*     console.log(error);
*   } else {
*     console.log("Publisher initialized.");
*   }
* });
* </pre>
*
* @returns {Publisher} The Publisher object.
* @see <a href="Session#publish>Session.publish()</a>
* @method OT.initPublisher
* @memberof OT
*/
OT.initPublisher = function(targetElement, properties, completionHandler) {
  OT.debug('OT.initPublisher(' + targetElement + ')');

  // To support legacy (apikey, targetElement, properties) users
  // we check to see if targetElement is actually an apikey. Which we ignore.
  if (typeof targetElement === 'string' && !document.getElementById(targetElement)) {
    targetElement = properties;
    properties = completionHandler;
    completionHandler = arguments[3];
  }

  if (typeof targetElement === 'function') {
    completionHandler = targetElement;
    properties = undefined;
    targetElement = undefined;
  } else if (OT.$.isObject(targetElement) && !(OT.$.isElementNode(targetElement))) {
    completionHandler = properties;
    properties = targetElement;
    targetElement = undefined;
  }

  if (typeof properties === 'function') {
    completionHandler = properties;
    properties = undefined;
  }

  var publisher = new OT.Publisher(properties);
  OT.publishers.add(publisher);

  var triggerCallback = function triggerCallback() {
    if (completionHandler && OT.$.isFunction(completionHandler)) {
      completionHandler.apply(null, arguments);
    }
  },

  removeInitSuccessAndCallComplete = function removeInitSuccessAndCallComplete(err) {
    publisher.off('publishComplete', removeHandlersAndCallComplete);
    triggerCallback(err);
  },

  removeHandlersAndCallComplete = function removeHandlersAndCallComplete(err) {
    publisher.off('initSuccess', removeInitSuccessAndCallComplete);

    // We're only handling the error case here as we're just
    // initing the publisher, not actually attempting to publish.
    if (err) triggerCallback(err);
  };

  publisher.once('initSuccess', removeInitSuccessAndCallComplete);
  publisher.once('publishComplete', removeHandlersAndCallComplete);

  publisher.publish(targetElement);

  return publisher;
};

/**
 * Enumerates the audio input devices (such as microphones) and video input devices
 * (cameras) available to the browser.
 * <p>
 * The array of devices is passed in as the <code>devices</code> parameter of
 * the <code>callback</code> function passed into the method.
 * <p>
 * This method is only available in Chrome and Internet Explorer. In Firefox, the callback function
 * is passed an error object.
 *
 * @param callback {Function} The callback function invoked when the list of devices
 * devices is available. This function takes two parameters:
 * <ul>
 *   <li><code>error</code> &mdash; This is set to an error object when
 *   there is an error in calling this method; it is set to <code>null</code>
 *   when the call succeeds.</li>
 *
 *   <li><p><code>devices</code> &mdash; An array of objects corresponding to
 *   available microphones and cameras. Each object has three properties: <code>kind</code>,
 *   <code>deviceId</code>, and <code>label</code>, each of which are strings.
 *   <p>
 *   The <code>kind</code> property is set to <code>"audioInput"</code> for audio input
 *   devices or <code>"videoInput"</code> for video input devices.
 *   <p>
 *   The <code>deviceId</code> property is a unique ID for the device. You can pass
 *   the <code>deviceId</code> in as the <code>audioSource</code> or <code>videoSource</code>
 *   property of the the <code>options</code> parameter of the
 *   <a href="#initPublisher">OT.initPublisher()</a> method.
 *   <p>
 *   The <code>label</code> property identifies the device. The <code>label</code>
 *   property is set to an empty string if the user has not previously granted access to
 *   a camera and microphone. In HTTP, the user must have granted access to a camera and
 *   microphone in the current page (for example, in response to a call to
 *   <code>OT.initPublisher()</code>). In HTTPS, the user must have previously granted access
 *   to the camera and microphone in the current page or in a page previously loaded from the
 *   domain.</li>
 * </ul>
 *
 * @see <a href="#initPublisher">OT.initPublisher()</a>
 * @method OT.getDevices
 * @memberof OT
 */
OT.getDevices = function(callback) {
  OT.$.getMediaDevices(callback);
};

/**
* Report that your app experienced an issue. You can use the issue ID with
* <a href="http://tokbox.com/developer/tools/Inspector">Inspector</a> or when discussing
* an issue with the TokBox support team.
*
* @param completionHandler {Function} A function that is called when the call to this method
* succeeds or fails. This function has two parameters. The first parameter is an
* <a href="Error.html">Error</a> object that is set when the call to the <code>reportIssue()</code>
* method fails (for example, if the client is not connected to the network) or <code>null</code>
* when the call to the <code>reportIssue()</code> method succeeds. The second parameter is set to
* the report ID (a unique string) when the call succeeds.
*
* @method OT.reportIssue
* @memberof OT
*/
OT.reportIssue = function(completionHandler) {
  var reportIssueId = OT.$.uuid();
  var sessionCount = OT.sessions.length();
  var completedLogEventCount = 0;
  var errorReported = false;

  function logEventCompletionHandler(error) {
    if (error) {
      if (completionHandler && !errorReported) {
        var reportIssueError = new OT.Error(OT.ExceptionCodes.REPORT_ISSUE_ERROR,
          'Error calling OT.reportIssue(). Check the client\'s network connection.');
        completionHandler(reportIssueError);
        errorReported = true;
      }
    } else {
      completedLogEventCount++;
      if (completedLogEventCount >= sessionCount && completionHandler && !errorReported) {
        completionHandler(null, reportIssueId);
      }
    }
  }

  var eventOptions = {
    action: 'ReportIssue',
    payload: {
      reportIssueId: reportIssueId
    }
  };

  if (sessionCount === 0) {
    OT.analytics.logEvent(eventOptions, null, logEventCompletionHandler);
  } else {
    OT.sessions.forEach(function(session) {
      var individualSessionEventOptions = OT.$.extend({
        sessionId: session.sessionId,
        partnerId: session.isConnected() ? session.sessionInfo.partnerId : null
      }, eventOptions);
      OT.analytics.logEvent(individualSessionEventOptions, null, logEventCompletionHandler);
    });
  }
};

OT.components = {};

/**
 * This method is deprecated. Use <a href="#on">on()</a> or <a href="#once">once()</a> instead.
 *
 * <p>
 * Registers a method as an event listener for a specific event.
 * </p>
 *
 * <p>
 * The OT object dispatches one type of event &#151; an <code>exception</code> event. The
 * following code adds an event listener for the <code>exception</code> event:
 * </p>
 *
 * <pre>
 * OT.addEventListener("exception", exceptionHandler);
 *
 * function exceptionHandler(event) {
 *    alert("exception event. \n  code == " + event.code + "\n  message == " + event.message);
 * }
 * </pre>
 *
 * <p>
 *   If a handler is not registered for an event, the event is ignored locally. If the event
 *   listener function does not exist, the event is ignored locally.
 * </p>
 * <p>
 *   Throws an exception if the <code>listener</code> name is invalid.
 * </p>
 *
 * @param {String} type The string identifying the type of event.
 *
 * @param {Function} listener The function to be invoked when the OT object dispatches the event.
 * @see <a href="#on">on()</a>
 * @see <a href="#once">once()</a>
 * @memberof OT
 * @method addEventListener
 *
 */

/**
 * This method is deprecated. Use <a href="#off">off()</a> instead.
 *
 * <p>
 * Removes an event listener for a specific event.
 * </p>
 *
 * <p>
 *   Throws an exception if the <code>listener</code> name is invalid.
 * </p>
 *
 * @param {String} type The string identifying the type of event.
 *
 * @param {Function} listener The event listener function to remove.
 *
 * @see <a href="#off">off()</a>
 * @memberof OT
 * @method removeEventListener
 */

/**
* Adds an event handler function for one or more events.
*
* <p>
* The OT object dispatches one type of event &#151; an <code>exception</code> event. The following
* code adds an event
* listener for the <code>exception</code> event:
* </p>
*
* <pre>
* OT.on("exception", function (event) {
*   // This is the event handler.
* });
* </pre>
*
* <p>You can also pass in a third <code>context</code> parameter (which is optional) to define the
* value of
* <code>this</code> in the handler method:</p>
*
* <pre>
* OT.on("exception",
*   function (event) {
*     // This is the event handler.
*   }),
*   session
* );
* </pre>
*
* <p>
* If you do not add a handler for an event, the event is ignored locally.
* </p>
*
* @param {String} type The string identifying the type of event.
* @param {Function} handler The handler function to process the event. This function takes the event
* object as a parameter.
* @param {Object} context (Optional) Defines the value of <code>this</code> in the event handler
* function.
*
* @memberof OT
* @method on
* @see <a href="#off">off()</a>
* @see <a href="#once">once()</a>
* @see <a href="#events">Events</a>
*/

/**
* Adds an event handler function for an event. Once the handler is called, the specified handler
* method is
* removed as a handler for this event. (When you use the <code>OT.on()</code> method to add an event
* handler, the handler
* is <i>not</i> removed when it is called.) The <code>OT.once()</code> method is the equivilent of
* calling the <code>OT.on()</code>
* method and calling <code>OT.off()</code> the first time the handler is invoked.
*
* <p>
* The following code adds a one-time event handler for the <code>exception</code> event:
* </p>
*
* <pre>
* OT.once("exception", function (event) {
*   console.log(event);
* }
* </pre>
*
* <p>You can also pass in a third <code>context</code> parameter (which is optional) to define the
* value of
* <code>this</code> in the handler method:</p>
*
* <pre>
* OT.once("exception",
*   function (event) {
*     // This is the event handler.
*   },
*   session
* );
* </pre>
*
* <p>
* The method also supports an alternate syntax, in which the first parameter is an object that is a
* hash map of
* event names and handler functions and the second parameter (optional) is the context for this in
* each handler:
* </p>
* <pre>
* OT.once(
*   {exeption: function (event) {
*     // This is the event handler.
*     }
*   },
*   session
* );
* </pre>
*
* @param {String} type The string identifying the type of event. You can specify multiple event
* names in this string,
* separating them with a space. The event handler will process the first occurence of the events.
* After the first event,
* the handler is removed (for all specified events).
* @param {Function} handler The handler function to process the event. This function takes the event
* object as a parameter.
* @param {Object} context (Optional) Defines the value of <code>this</code> in the event handler
* function.
*
* @memberof OT
* @method once
* @see <a href="#on">on()</a>
* @see <a href="#once">once()</a>
* @see <a href="#events">Events</a>
*/

/**
 * Removes an event handler.
 *
 * <p>Pass in an event name and a handler method, the handler is removed for that event:</p>
 *
 * <pre>OT.off("exceptionEvent", exceptionEventHandler);</pre>
 *
 * <p>If you pass in an event name and <i>no</i> handler method, all handlers are removed for that
 * events:</p>
 *
 * <pre>OT.off("exceptionEvent");</pre>
 *
 * <p>
 * The method also supports an alternate syntax, in which the first parameter is an object that is a
 * hash map of
 * event names and handler functions and the second parameter (optional) is the context for matching
 * handlers:
 * </p>
 * <pre>
 * OT.off(
 *   {
 *     exceptionEvent: exceptionEventHandler
 *   },
 *   this
 * );
 * </pre>
 *
 * @param {String} type (Optional) The string identifying the type of event. You can use a space to
 * specify multiple events, as in "eventName1 eventName2 eventName3". If you pass in no
 * <code>type</code> value (or other arguments), all event handlers are removed for the object.
 * @param {Function} handler (Optional) The event handler function to remove. If you pass in no
 * <code>handler</code>, all event handlers are removed for the specified event <code>type</code>.
 * @param {Object} context (Optional) If you specify a <code>context</code>, the event handler is
 * removed for all specified events and handlers that use the specified context.
 *
 * @memberof OT
 * @method off
 * @see <a href="#on">on()</a>
 * @see <a href="#once">once()</a>
 * @see <a href="#events">Events</a>
 */

/**
 * Dispatched by the OT class when the app encounters an exception.
 * Note that you set up an event handler for the <code>exception</code> event by calling the
 * <code>OT.on()</code> method.
 *
 * @name exception
 * @event
 * @borrows ExceptionEvent#message as this.message
 * @memberof OT
 * @see ExceptionEvent
 */

// tb_require('./helpers/lib/css_loader.js')
// tb_require('./ot/system_requirements.js')
// tb_require('./ot/session.js')
// tb_require('./ot/publisher.js')
// tb_require('./ot/subscriber.js')
// tb_require('./ot/archive.js')
// tb_require('./ot/connection.js')
// tb_require('./ot/stream.js')
// We want this to be included at the end, just before footer.js

/* jshint globalstrict: true, strict: false, undef: true, unused: true,
          trailing: true, browser: true, smarttabs:true */
/* global loadCSS, define */

// Tidy up everything on unload
OT.onUnload(function() {
  OT.publishers.destroy();
  OT.subscribers.destroy();
  OT.sessions.destroy('unloaded');
});

loadCSS(OT.properties.cssURL);

// Register as a named AMD module, since TokBox could be concatenated with other
// files that may use define, but not via a proper concatenation script that
// understands anonymous AMD modules. A named AMD is safest and most robust
// way to register. Uppercase TB is used because AMD module names are
// derived from file names, and OpenTok is normally delivered in an uppercase
// file name.
if (typeof define === 'function' && define.amd) {
  define('TB', [], function() { return TB; });
}

// tb_require('./postscript.js')

/* jshint ignore:start */
})(window, window.OT);
/* jshint ignore:end */

})(window || exports);