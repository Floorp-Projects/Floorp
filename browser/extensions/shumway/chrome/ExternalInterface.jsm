/*
 * Copyright 2015 Mozilla Foundation
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
 */

var EXPORTED_SYMBOLS = ['ExternalInterface'];

Components.utils.import('resource://gre/modules/Services.jsm');

// Helper class to forward external interface messages between chrome code and
// content window / object element.
function ExternalInterface(window, embedTag, callback) {
  this.window = window;
  this.embedTag = embedTag;
  this.callback = callback;
  this.externalComInitialized = false;
}
ExternalInterface.prototype = {
  processAction: function (data) {
    var parentWindow = this.window;
    var embedTag = this.embedTag;

    switch (data.action) {
      case 'init':
        if (this.externalComInitialized)
          return;

        this.externalComInitialized = true;
        this.initExternalCom(parentWindow, embedTag, this.callback);
        return;
      case 'getId':
        return embedTag.id;
      case 'eval':
        return this.__flash__eval(data.expression);
      case 'call':
        return this.__flash__call(data.request);
      case 'register':
        return this.__flash__registerCallback(data.functionName);
      case 'unregister':
        return this.__flash__unregisterCallback(data.functionName);
    }
  },

  initExternalCom: function (window, embedTag, onExternalCallback) {
    var traceExternalInterface = getBoolPref('shumway.externalInterface.trace', false);
    // Initialize convenience functions. Notice that these functions are
    // exposed to the content via Cu.exportFunction, so be careful (e.g. don't
    // use `this` pointer).
    this.__flash__toXML = function __flash__toXML(obj) {
      switch (typeof obj) {
        case 'boolean':
          return obj ? '<true/>' : '<false/>';
        case 'number':
          return '<number>' + obj + '</number>';
        case 'object':
          if (obj === null) {
            return '<null/>';
          }
          if ('hasOwnProperty' in obj && obj.hasOwnProperty('length')) {
            // array
            var xml = '<array>';
            for (var i = 0; i < obj.length; i++) {
              xml += '<property id="' + i + '">' + __flash__toXML(obj[i]) + '</property>';
            }
            return xml + '</array>';
          }
          var xml = '<object>';
          for (var i in obj) {
            xml += '<property id="' + i + '">' + __flash__toXML(obj[i]) + '</property>';
          }
          return xml + '</object>';
        case 'string':
          return '<string>' + obj.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;') + '</string>';
        case 'undefined':
          return '<undefined/>';
      }
    };
    this.__flash__eval = function (expr) {
      traceExternalInterface && window.console.log('__flash__eval: ' + expr);
      // allowScriptAccess protects page from unwanted swf scripts,
      // we can execute script in the page context without restrictions.
      var result = window.eval(expr);
      traceExternalInterface && window.console.log('__flash__eval (result): ' + result);
      return result;
    };
    this.__flash__call = function (expr) {
      traceExternalInterface && window.console.log('__flash__call (ignored): ' + expr);
    };

    this.__flash__registerCallback = function (functionName) {
      traceExternalInterface && window.console.log('__flash__registerCallback: ' + functionName);
      Components.utils.exportFunction(function () {
        var args = Array.prototype.slice.call(arguments, 0);
        traceExternalInterface && window.console.log('__flash__callIn: ' + functionName);
        var result;
        if (onExternalCallback) {
          result = onExternalCallback({functionName: functionName, args: args});
          traceExternalInterface && window.console.log('__flash__callIn (result): ' + result);
        }
        return window.eval(result);
      }, embedTag.wrappedJSObject, {defineAs: functionName});
    };
    this.__flash__unregisterCallback = function (functionName) {
      traceExternalInterface && window.console.log('__flash__unregisterCallback: ' + functionName);
      delete embedTag.wrappedJSObject[functionName];
    };

    this.exportInterfaceFunctions();
  },

  exportInterfaceFunctions: function () {
    var window = this.window;
    // We need to export window functions only once.
    if (!window.wrappedJSObject.__flash__initialized) {
      window.wrappedJSObject.__flash__initialized = true;
      // JavaScript eval'ed code use those functions.
      // TODO find the case when JavaScript code patches those
      // __flash__toXML will be called by the eval'ed code.
      Components.utils.exportFunction(this.__flash__toXML, window.wrappedJSObject,
        {defineAs: '__flash__toXML'});
      // The functions below are used for compatibility and are not called by chrome code.
      Components.utils.exportFunction(this.__flash__eval, window.wrappedJSObject,
        {defineAs: '__flash__eval'});
      Components.utils.exportFunction(this.__flash__call, window.wrappedJSObject,
        {defineAs: '__flash__call'});
    }
  }
};

function getBoolPref(pref, def) {
  try {
    return Services.prefs.getBoolPref(pref);
  } catch (ex) {
    return def;
  }
}
