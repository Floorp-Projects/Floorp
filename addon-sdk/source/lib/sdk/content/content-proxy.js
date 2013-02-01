/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* Trick the linker in order to avoid error on `Components.interfaces` usage.
   We are tricking the linker with `require('./content-proxy.js')` from
   worjer.js in order to ensure shipping this file! But then the linker think
   that this file is going to be used as a CommonJS module where we forbid usage
   of `Components`.
*/
let Ci = Components['interfaces'];

/**
 * Access key that allows privileged code to unwrap proxy wrappers through 
 * valueOf:
 *   let xpcWrapper = proxyWrapper.valueOf(UNWRAP_ACCESS_KEY);
 * This key should only be used by proxy unit test.
 */
 const UNWRAP_ACCESS_KEY = {};


 /**
 * Returns a closure that wraps arguments before calling the given function,
 * which can be given to native functions that accept a function, such that when
 * the closure is called, the given function is called with wrapped arguments.
 *
 * @param fun {Function}
 *        the function for which to create a closure wrapping its arguments
 * @param obj {Object}
 *        target object from which `fun` comes from
 *        (optional, for debugging purpose)
 * @param name {String}
 *        name of the attribute from which `fun` is binded on `obj`
 *        (optional, for debugging purpose)
 *
 * Example:
 *   function contentScriptListener(event) {}
 *   let wrapper = ContentScriptFunctionWrapper(contentScriptListener);
 *   xray.addEventListener("...", wrapper, false);
 * -> Allow to `event` to be wrapped
 */
function ContentScriptFunctionWrapper(fun, obj, name) {
  if ("___proxy" in fun && typeof fun.___proxy == "function")
    return fun.___proxy;
  
  let wrappedFun = function () {
    let args = [];
    for (let i = 0, l = arguments.length; i < l; i++)
      args.push(wrap(arguments[i]));
    
    //console.log("Called from native :"+obj+"."+name);
    //console.log(">args "+arguments.length);
    //console.log(fun);
    
    // Native code can execute this callback with `this` being the wrapped 
    // function. For example, window.mozRequestAnimationFrame.
    if (this == wrappedFun)
      return fun.apply(fun, args);
    
    return fun.apply(wrap(this), args);
  };
  
  Object.defineProperty(fun, "___proxy", {value : wrappedFun,
                                          writable : false,
                                          enumerable : false,
                                          configurable : false});
  
  return wrappedFun;
}

/**
 * Returns a closure that unwraps arguments before calling the `fun` function,
 * which can be used to build a wrapper for a native function that accepts
 * wrapped arguments, since native function only accept unwrapped arguments.
 *
 * @param fun {Function}
 *        the function to wrap
 * @param originalObject {Object}
 *        target object from which `fun` comes from
 *        (optional, for debugging purpose)
 * @param name {String}
 *        name of the attribute from which `fun` is binded on `originalObject`
 *        (optional, for debugging purpose)
 *
 * Example:
 *   wrapper.appendChild = NativeFunctionWrapper(xray.appendChild, xray);
 *   wrapper.appendChild(anotherWrapper);
 * -> Allow to call xray.appendChild with unwrapped version of anotherWrapper
 */
function NativeFunctionWrapper(fun, originalObject, name) {
  return function () {
    let args = [];
    let obj = this && typeof this.valueOf == "function" ?
              this.valueOf(UNWRAP_ACCESS_KEY) : this;

    for (let i = 0, l = arguments.length; i < l; i++)
      args.push( unwrap(arguments[i], obj, name) );
    
    //if (name != "toString")
    //console.log(">>calling native ["+(name?name:'#closure#')+"]: \n"+fun.apply+"\n"+obj+"\n("+args.join(', ')+")\nthis :"+obj+"from:"+originalObject+"\n");
    
    // Need to use Function.prototype.apply.apply because XMLHttpRequest 
    // is a function (typeof return 'function') and fun.apply is null :/
    let unwrapResult = Function.prototype.apply.apply(fun, [obj, args]);
    let result = wrap(unwrapResult, obj, name);
    
    //console.log("<< "+rr+" -> "+r);
    
    return result;
  };
}

/*
 * Unwrap a JS value that comes from the content script.
 * Mainly converts proxy wrapper to XPCNativeWrapper.
 */
function unwrap(value, obj, name) {
  //console.log("unwrap : "+value+" ("+name+")");
  if (!value)
    return value;
  let type = typeof value;  
  
  // In case of proxy, unwrap them recursively 
  // (it should not be recursive, just in case of)
  if (["object", "function"].indexOf(type) !== -1 && 
      "__isWrappedProxy" in value) {
    while("__isWrappedProxy" in value)
      value = value.valueOf(UNWRAP_ACCESS_KEY);
    return value;
  }
  
  // In case of functions we need to return a wrapper that converts native 
  // arguments applied to this function into proxies.
  if (type == "function")
    return ContentScriptFunctionWrapper(value, obj, name);
  
  // We must wrap objects coming from content script too, as they may have
  // a function that will be called by a native method.
  // For example:
  //   addEventListener(..., { handleEvent: function(event) {} }, ...);
  if (type == "object")
    return ContentScriptObjectWrapper(value);

  if (["string", "number", "boolean"].indexOf(type) !== -1)
    return value;
  //console.log("return non-wrapped to native : "+typeof value+" -- "+value);
  return value;
}

/**
 * Returns an XrayWrapper proxy object that allow to wrap any of its function
 * though `ContentScriptFunctionWrapper`. These proxies are given to
 * XrayWrappers in order to automatically wrap values when they call a method
 * of these proxies. So that they are only used internaly and content script,
 * nor web page have ever access to them. As a conclusion, we can consider
 * this code as being safe regarding web pages overload.
 *
 *
 * @param obj {Object}
 *        object coming from content script context to wrap
 *
 * Example:
 *   let myListener = { handleEvent: function (event) {} };
 *   node.addEventListener("click", myListener, false);
 *   `event` has to be wrapped, so handleEvent has to be wrapped using
 *   `ContentScriptFunctionWrapper` function.
 *   In order to do so, we build this new kind of proxies.
 */
function ContentScriptObjectWrapper(obj) {
  if ("___proxy" in obj && typeof obj.___proxy == "object")
    return obj.___proxy;

  function valueOf(key) {
    if (key === UNWRAP_ACCESS_KEY)
      return obj;
    return this;
  }

  let proxy = Proxy.create({
    // Fundamental traps
    getPropertyDescriptor:  function(name) {
      return Object.getOwnPropertyDescriptor(obj, name);
    },
    defineProperty: function(name, desc) {
      return Object.defineProperty(obj, name, desc);
    },
    getOwnPropertyNames: function () {
      return Object.getOwnPropertyNames(obj);
    },
    delete: function(name) {
      return delete obj[name];
    },
    // derived traps
    has: function(name) {
      return name === "__isXrayWrapperProxy" ||
             name in obj;
    },
    hasOwn: function(name) {
      return Object.prototype.hasOwnProperty.call(obj, name);
    },
    get: function(receiver, name) {
      if (name == "valueOf")
        return valueOf;
      let value = obj[name];
      if (!value)
        return value;

      return unwrap(value);
    },
    set: function(receiver, name, val) {
      obj[name] = val;
      return true;
    },
    enumerate: function() {
      var result = [];
      for each (let name in obj) {
        result.push(name);
      };
      return result;
    },
    keys: function() {
      return Object.keys(obj);
    }
  });

  Object.defineProperty(obj, "___proxy", {value : proxy,
                                          writable : false,
                                          enumerable : false,
                                          configurable : false});

  return proxy;
}

// List of all existing typed arrays.
//   Can be found here:
//   http://mxr.mozilla.org/mozilla-central/source/js/src/jsapi.cpp#1790
const typedArraysCtor = [
  ArrayBuffer,
  Int8Array,
  Uint8Array,
  Int16Array,
  Uint16Array,
  Int32Array,
  Uint32Array,
  Float32Array,
  Float64Array,
  Uint8ClampedArray
];

/*
 * Wrap a JS value coming from the document by building a proxy wrapper.
 */
function wrap(value, obj, name, debug) {
  if (!value)
    return value;
  let type = typeof value;
  if (type == "object") {
    // Bug 671016: Typed arrays don't need to be proxified.
    // We avoid checking the whole constructor list on all objects
    // by doing this check only on non-extensible objects:
    if (!Object.isExtensible(value) &&
        typedArraysCtor.indexOf(value.constructor) !== -1)
      return value;

    // Bug 715755: do not proxify COW wrappers
    // These wrappers throw an exception when trying to access
    // any attribute that is not in a white list
    try {
      ("nonExistantAttribute" in value);
    }
    catch(e) {
      if (e.message.indexOf("Permission denied to access property") !== -1)
        return value;
    }

    // We may have a XrayWrapper proxy.
    // For example:
    //   let myListener = { handleEvent: function () {} };
    //   node.addEventListener("click", myListener, false);
    // When native code want to call handleEvent,
    // we go though ContentScriptFunctionWrapper that calls `wrap(this)`
    // `this` is the XrayWrapper proxy of myListener.
    // We return this object without building a CS proxy as it is already
    // a value coming from the CS.
    if ("__isXrayWrapperProxy" in value)
      return value.valueOf(UNWRAP_ACCESS_KEY);

    // Unwrap object before wrapping it.
    // It should not happen with CS proxy objects.
    while("__isWrappedProxy" in value) {
      value = value.valueOf(UNWRAP_ACCESS_KEY);
    }

    if (XPCNativeWrapper.unwrap(value) !== value)
      return getProxyForObject(value);
    // In case of Event, HTMLCollection or NodeList or ???
    // XPCNativeWrapper.unwrap(value) === value
    // but it's still a XrayWrapper so let's build a proxy
    return getProxyForObject(value);
  }
  if (type == "function") {
    if (XPCNativeWrapper.unwrap(value) !== value
        || (typeof value.toString === "function" && 
            value.toString().match(/\[native code\]/))) {
      return getProxyForFunction(value, NativeFunctionWrapper(value, obj, name));
    }
    return value;
  }
  if (type == "string")
    return value;
  if (type == "number")
    return value;
  if (type == "boolean")
    return value;
  //console.log("return non-wrapped to wrapped : "+value);
  return value;
}

/* 
 * Wrap an object from the document to a proxy wrapper
 */
function getProxyForObject(obj) {
  if (typeof obj != "object") {
    let msg = "tried to proxify something other than an object: " + typeof obj;
    console.warn(msg);
    throw msg;
  }
  if ("__isWrappedProxy" in obj) {
    return obj;
  }
  // Check if there is a proxy cached on this wrapper,
  // but take care of prototype ___proxy attribute inheritance!
  if (obj && obj.___proxy && obj.___proxy.valueOf(UNWRAP_ACCESS_KEY) === obj) {
    return obj.___proxy;
  }
  
  let proxy = Proxy.create(handlerMaker(obj));
  
  Object.defineProperty(obj, "___proxy", {value : proxy,
                                          writable : false,
                                          enumerable : false,
                                          configurable : false});
  return proxy;
}

/* 
 * Wrap a function from the document to a proxy wrapper
 */
function getProxyForFunction(fun, callTrap) {
  if (typeof fun != "function") {
    let msg = "tried to proxify something other than a function: " + typeof fun;
    console.warn(msg);
    throw msg;
  }
  if ("__isWrappedProxy" in fun)
    return obj;
  if ("___proxy" in fun)
    return fun.___proxy;
  
  let proxy = Proxy.createFunction(handlerMaker(fun), callTrap);
  
  Object.defineProperty(fun, "___proxy", {value : proxy,
                                          writable : false,
                                          enumerable : false,
                                          configurable : false});
  
  return proxy;
}

/* 
 * Check if a DOM attribute name is an event name.
 */
function isEventName(id) {
  if (id.indexOf("on") != 0 || id.length == 2) 
    return false;
  // Taken from:
  // http://mxr.mozilla.org/mozilla-central/source/dom/base/nsDOMClassInfo.cpp#7616
  switch (id[2]) {
    case 'a' :
      return (id == "onabort" ||
              id == "onafterscriptexecute" ||
              id == "onafterprint");
    case 'b' :
      return (id == "onbeforeunload" ||
              id == "onbeforescriptexecute" ||
              id == "onblur" ||
              id == "onbeforeprint");
    case 'c' :
      return (id == "onchange"       ||
              id == "onclick"        ||
              id == "oncontextmenu"  ||
              id == "oncopy"         ||
              id == "oncut"          ||
              id == "oncanplay"      ||
              id == "oncanplaythrough");
    case 'd' :
      return (id == "ondblclick"     || 
              id == "ondrag"         ||
              id == "ondragend"      ||
              id == "ondragenter"    ||
              id == "ondragleave"    ||
              id == "ondragover"     ||
              id == "ondragstart"    ||
              id == "ondrop"         ||
              id == "ondurationchange");
    case 'e' :
      return (id == "onerror" ||
              id == "onemptied" ||
              id == "onended");
    case 'f' :
      return id == "onfocus";
    case 'h' :
      return id == "onhashchange";
    case 'i' :
      return (id == "oninput" ||
              id == "oninvalid");
    case 'k' :
      return (id == "onkeydown"      ||
              id == "onkeypress"     ||
              id == "onkeyup");
    case 'l' :
      return (id == "onload"           ||
              id == "onloadeddata"     ||
              id == "onloadedmetadata" ||
              id == "onloadstart");
    case 'm' :
      return (id == "onmousemove"    ||
              id == "onmouseout"     ||
              id == "onmouseover"    ||
              id == "onmouseup"      ||
              id == "onmousedown"    ||
              id == "onmessage");
    case 'p' :
      return (id == "onpaint"        ||
              id == "onpageshow"     ||
              id == "onpagehide"     ||
              id == "onpaste"        ||
              id == "onpopstate"     ||
              id == "onpause"        ||
              id == "onplay"         ||
              id == "onplaying"      ||
              id == "onprogress");
    case 'r' :
      return (id == "onreadystatechange" ||
              id == "onreset"            ||
              id == "onresize"           ||
              id == "onratechange");
    case 's' :
      return (id == "onscroll"       ||
              id == "onselect"       ||
              id == "onsubmit"       || 
              id == "onseeked"       ||
              id == "onseeking"      ||
              id == "onstalled"      ||
              id == "onsuspend");
    case 't':
      return id == "ontimeupdate" 
      /* 
        // TODO: Make it work for mobile version
        ||
        (nsDOMTouchEvent::PrefEnabled() &&
         (id == "ontouchstart" ||
          id == "ontouchend" ||
          id == "ontouchmove" ||
          id == "ontouchenter" ||
          id == "ontouchleave" ||
          id == "ontouchcancel"))*/;
      
    case 'u' :
      return id == "onunload";
    case 'v':
      return id == "onvolumechange";
    case 'w':
      return id == "onwaiting";
    }
  
  return false;
}

// XrayWrappers miss some attributes.
// Here is a list of functions that return a value when it detects a miss:
const NODES_INDEXED_BY_NAME = ["IMG", "FORM", "APPLET", "EMBED", "OBJECT"];
const xRayWrappersMissFixes = [

  // Fix bug with XPCNativeWrapper on HTMLCollection
  // We can only access array item once, then it's undefined :o
  function (obj, name) {
    let i = parseInt(name);
    if (obj.toString().match(/HTMLCollection|NodeList/) && 
        i >= 0 && i < obj.length) {
      return wrap(XPCNativeWrapper(obj.wrappedJSObject[name]), obj, name);
    }
    return null;
  },

  // Trap access to document["form name"] 
  // that may refer to an existing form node
  // http://mxr.mozilla.org/mozilla-central/source/dom/base/nsDOMClassInfo.cpp#9285
  function (obj, name) {
    if ("nodeType" in obj && obj.nodeType == 9) {
      let node = obj.wrappedJSObject[name];
      // List of supported tag:
      // http://mxr.mozilla.org/mozilla-central/source/content/html/content/src/nsGenericHTMLElement.cpp#1267
      if (node && NODES_INDEXED_BY_NAME.indexOf(node.tagName) != -1)
        return wrap(XPCNativeWrapper(node));
    }
    return null;
  },

  // Trap access to window["frame name"] and window.frames[i]
  // that refer to an (i)frame internal window object
  // http://mxr.mozilla.org/mozilla-central/source/dom/base/nsDOMClassInfo.cpp#6824
  function (obj, name) {
    if (typeof obj == "object" && "document" in obj) {
      // Ensure that we are on a window object
      try {
        obj.QueryInterface(Ci.nsIDOMWindow);
      }
      catch(e) {
        return null;
      }

      // Integer case:
      let i = parseInt(name);
      if (i >= 0 && i in obj) {
        return wrap(XPCNativeWrapper(obj[i]));
      }

      // String name case:
      if (name in obj.wrappedJSObject) {
        let win = obj.wrappedJSObject[name];
        let nodes = obj.document.getElementsByName(name);
        for (let i = 0, l = nodes.length; i < l; i++) {
          let node = nodes[i];
          if ("contentWindow" in node && node.contentWindow == win)
            return wrap(node.contentWindow);
        }
      }
    }
    return null;
  },

  // Trap access to form["node name"]
  // http://mxr.mozilla.org/mozilla-central/source/dom/base/nsDOMClassInfo.cpp#9477
  function (obj, name) {
    if (typeof obj == "object" && "tagName" in obj && obj.tagName == "FORM") {
      let match = obj.wrappedJSObject[name];
      let nodes = obj.ownerDocument.getElementsByName(name);
      for (let i = 0, l = nodes.length; i < l; i++) {
        let node = nodes[i];
        if (node == match)
          return wrap(node);
      }
    }
    return null;
  }

];

// XrayWrappers have some buggy methods.
// Here is the list of them with functions returning some replacement 
// for a given object `obj`:
const xRayWrappersMethodsFixes = {
  // postMessage method is checking the Javascript global
  // and it expects it to be a window object.
  // But in our case, the global object is our sandbox global object.
  // See nsGlobalWindow::CallerInnerWindow():
  // http://mxr.mozilla.org/mozilla-central/source/dom/base/nsGlobalWindow.cpp#5893
  // nsCOMPtr<nsPIDOMWindow> win = do_QueryWrappedNative(wrapper);
  //   win is null
  postMessage: function (obj) {
    // Ensure that we are on a window object
    try {
      obj.QueryInterface(Ci.nsIDOMWindow);
    }
    catch(e) {
      return null;
    }

    // Create a wrapper that is going to call `postMessage` through `eval`
    let f = function postMessage(message, targetOrigin) {
      let jscode = "this.postMessage(";
      if (typeof message != "string")
        jscode += JSON.stringify(message);
      else
        jscode += "'" + message.toString().replace(/['\\]/g,"\\$&") + "'";

      targetOrigin = targetOrigin.toString().replace(/['\\]/g,"\\$&");

      jscode += ", '" + targetOrigin + "')";
      return this.wrappedJSObject.eval(jscode);
    };
    return getProxyForFunction(f, NativeFunctionWrapper(f));
  },
  
  // Fix mozMatchesSelector uses that is broken on XrayWrappers
  // when we use document.documentElement.mozMatchesSelector.call(node, expr)
  // It's only working if we call mozMatchesSelector on the node itself.
  // SEE BUG 658909: mozMatchesSelector returns incorrect results with XrayWrappers
  mozMatchesSelector: function (obj) {
    // Ensure that we are on an object to expose this buggy method
    try {
      // Bug 707576 removed nsIDOMNSElement.
      // Can be simplified as soon as Firefox 11 become the minversion
      obj.QueryInterface("nsIDOMElement" in Ci ? Ci.nsIDOMElement :
                                                 Ci.nsIDOMNSElement);
    }
    catch(e) {
      return null;
    }
    // We can't use `wrap` function as `f` is not a native function,
    // so wrap it manually:
    let f = function mozMatchesSelector(selectors) {
      return this.mozMatchesSelector(selectors);
    };

    return getProxyForFunction(f, NativeFunctionWrapper(f));
  },

  // Bug 679054: History API doesn't work with Proxy objects. We have to pass
  // regular JS objects on `pushState` and `replaceState` methods.
  // In addition, the first argument has to come from the same compartment.
  pushState: function (obj) {
    // Ensure that we are on an object that expose History API
    try {
      obj.QueryInterface(Ci.nsIDOMHistory);
    }
    catch(e) {
      return null;
    }
    let f = function fix() {
      // Call native method with JSON objects
      // (need to convert `arguments` to an array via `slice`)
      return this.pushState.apply(this, JSON.parse(JSON.stringify(Array.slice(arguments))));
    };

    return getProxyForFunction(f, NativeFunctionWrapper(f));
  },
  replaceState: function (obj) {
    // Ensure that we are on an object that expose History API
    try {
      obj.QueryInterface(Ci.nsIDOMHistory);
    }
    catch(e) {
      return null;
    }
    let f = function fix() {
      // Call native method with JSON objects
      // (need to convert `arguments` to an array via `slice`)
      return this.replaceState.apply(this, JSON.parse(JSON.stringify(Array.slice(arguments))));
    };

    return getProxyForFunction(f, NativeFunctionWrapper(f));
  },

  // Bug 769006: nsIDOMMutationObserver.observe fails with proxy as options
  // attributes
  observe: function observe(obj) {
    // Ensure that we are on a DOMMutation object
    try {
      // nsIDOMMutationObserver starts with FF14
      if ("nsIDOMMutationObserver" in Ci)
        obj.QueryInterface(Ci.nsIDOMMutationObserver);
      else
        return null;
    }
    catch(e) {
      return null;
    }
    return function nsIDOMMutationObserverObserveFix(target, options) {
      // Gets native/unwrapped this
      let self = this && typeof this.valueOf == "function" ?
                 this.valueOf(UNWRAP_ACCESS_KEY) : this;
      // Unwrap the xraywrapper target out of JS proxy
      let targetXray = unwrap(target);
      // But do not wrap `options` through ContentScriptObjectWrapper
      let result = wrap(self.observe(targetXray, options));
      // Finally wrap result into JS proxies
      return wrap(result);
    };
  }
};

/* 
 * Generate handler for proxy wrapper
 */
function handlerMaker(obj) {
  // Overloaded attributes dictionary
  let overload = {};
  // Expando attributes dictionary (i.e. onclick, onfocus, on* ...)
  let expando = {};
  // Cache of methods overloaded to fix XrayWrapper bug
  let methodFixes = {};
  return {
    // Fundamental traps
    getPropertyDescriptor:  function(name) {
      return Object.getOwnPropertyDescriptor(obj, name);
    },
    defineProperty: function(name, desc) {
      return Object.defineProperty(obj, name, desc);
    },
    getOwnPropertyNames: function () {
      return Object.getOwnPropertyNames(obj);
    },
    delete: function(name) {
      delete expando[name];
      delete overload[name];
      return delete obj[name];
    },
    
    // derived traps
    has: function(name) {
      if (name == "___proxy") return false;
      if (isEventName(name)) {
        // XrayWrappers throw exception when we try to access expando attributes
        // even on "name in wrapper". So avoid doing it!
        return name in expando;
      }
      return name in obj || name in overload || name == "__isWrappedProxy" ||
             undefined !== this.get(null, name);
    },
    hasOwn: function(name) {
      return Object.prototype.hasOwnProperty.call(obj, name);
    },
    get: function(receiver, name) {
      if (name == "___proxy")
        return undefined;
      
      // Overload toString in order to avoid returning "[XrayWrapper [object HTMLElement]]"
      // or "[object Function]" for function's Proxy
      if (name == "toString") {
        // Bug 714778: we should not pass obj.wrappedJSObject.toString
        // in order to avoid sharing its proxy between two contents scripts.
        // (not that `unwrappedObj` can be equal to `obj` when `obj` isn't
        // an xraywrapper)
        let unwrappedObj = XPCNativeWrapper.unwrap(obj);
        return wrap(function () {
          return unwrappedObj.toString.call(
                   this.valueOf(UNWRAP_ACCESS_KEY), arguments);
        }, obj, name);
      }

      // Offer a way to retrieve XrayWrapper from a proxified node through `valueOf`
      if (name == "valueOf")
        return function (key) {
          if (key === UNWRAP_ACCESS_KEY)
            return obj;
          return this;
        };
      
      // Return overloaded value if there is one.
      // It allows to overload native methods like addEventListener that
      // are not saved, even on the wrapper itself.
      // (And avoid some methods like toSource from being returned here! [__proto__ test])
      if (name in overload &&
          overload[name] != Object.getPrototypeOf(overload)[name] &&
          name != "__proto__") {
        return overload[name];
      }
      
      // Catch exceptions thrown by XrayWrappers when we try to access on* 
      // attributes like onclick, onfocus, ...
      if (isEventName(name)) {
        //console.log("expando:"+obj+" - "+obj.nodeType);
        return name in expando ? expando[name].original : undefined;
      }
      
      // Overload some XrayWrappers method in order to fix its bugs
      if (name in methodFixes && 
          methodFixes[name] != Object.getPrototypeOf(methodFixes)[name] &&
          name != "__proto__")
        return methodFixes[name];
      if (Object.keys(xRayWrappersMethodsFixes).indexOf(name) !== -1) {
        let fix = xRayWrappersMethodsFixes[name](obj);
        if (fix)
          return methodFixes[name] = fix;
      }
      
      let o = obj[name];
      
      // XrayWrapper miss some attributes, try to catch these and return a value
      if (!o) {
        for each(let atttributeFixer in xRayWrappersMissFixes) {
          let fix = atttributeFixer(obj, name);
          if (fix)
            return fix;
        }
      }

      // Generic case
      return wrap(o, obj, name);
      
    },
    
    set: function(receiver, name, val) {

      if (isEventName(name)) {
        //console.log("SET on* attribute : " + name + " / " + val + "/" + obj);
        let shortName = name.replace(/^on/,"");
        
        // Unregister previously set listener
        if (expando[name]) {
          obj.removeEventListener(shortName, expando[name], true);
          delete expando[name];
        }
        
        // Only accept functions
        if (typeof val != "function")
          return false;
        
        // Register a new listener
        let original = val;
        val = ContentScriptFunctionWrapper(val);
        expando[name] = val;
        val.original = original;
        obj.addEventListener(name.replace(/^on/, ""), val, true);
        return true;
      }
      
      obj[name] = val;
      
      // Handle native method not overloaded on XrayWrappers:
      //   obj.addEventListener = val; -> obj.addEventlistener = native method
      // And, XPCNativeWrapper bug where nested values appear to be wrapped:
      // obj.customNestedAttribute = val -> obj.customNestedAttribute !== val
      //                                    obj.customNestedAttribute = "waive wrapper something"
      // SEE BUG 658560: Fix identity problem with CrossOriginWrappers
      // TODO: check that DOM can't be updated by the document itself and so overloaded value becomes wrong
      //       but I think such behavior is limited to primitive type
      if ((typeof val == "function" || typeof val == "object") && name) {
        overload[name] = val;
      }
      
      return true;
    },
    
    enumerate: function() {
      var result = [];
      for each (let name in Object.keys(obj)) {
        result.push(name);
      };
      return result;
    },
    
    keys: function() {
      return Object.keys(obj);
    }
  };
};


/* 
 * Wrap an object from the document to a proxy wrapper.
 */
function create(object) {
  if ("wrappedJSObject" in object)
    object = object.wrappedJSObject;
  let xpcWrapper = XPCNativeWrapper(object);
  // If we can't build an XPCNativeWrapper, it doesn't make sense to build
  // a proxy. All proxy code is based on having such wrapper that store
  // different JS attributes set.
  // (we can't build XPCNativeWrapper when object is from the same
  // principal/domain)
  if (object === xpcWrapper) {
    return object;
  }
  return getProxyForObject(xpcWrapper);
}
