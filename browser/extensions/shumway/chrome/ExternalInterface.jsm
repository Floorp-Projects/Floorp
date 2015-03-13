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

function ExternalInterface(window, embedTag, callback) {
  this.window = window;
  this.embedTag = embedTag;
  this.callback = callback;
  this.externalComInitialized = false;

  initExternalCom(window, embedTag, callback);
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
        initExternalCom(parentWindow, embedTag, this.callback);
        return;
      case 'getId':
        return embedTag.id;
      case 'eval':
        return parentWindow.__flash__eval(data.expression);
      case 'call':
        return parentWindow.__flash__call(data.request);
      case 'register':
        return embedTag.__flash__registerCallback(data.functionName);
      case 'unregister':
        return embedTag.__flash__unregisterCallback(data.functionName);
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

function initExternalCom(wrappedWindow, wrappedObject, onExternalCallback) {
  var traceExternalInterface = getBoolPref('shumway.externalInterface.trace', false);
  if (!wrappedWindow.__flash__initialized) {
    wrappedWindow.__flash__initialized = true;
    wrappedWindow.__flash__toXML = function __flash__toXML(obj) {
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
    wrappedWindow.__flash__eval = function (expr) {
      traceExternalInterface && this.console.log('__flash__eval: ' + expr);
      // allowScriptAccess protects page from unwanted swf scripts,
      // we can execute script in the page context without restrictions.
      var result = this.eval(expr);
      traceExternalInterface && this.console.log('__flash__eval (result): ' + result);
      return result;
    }.bind(wrappedWindow);
    wrappedWindow.__flash__call = function (expr) {
      traceExternalInterface && this.console.log('__flash__call (ignored): ' + expr);
    };
  }
  wrappedObject.__flash__registerCallback = function (functionName) {
    traceExternalInterface && wrappedWindow.console.log('__flash__registerCallback: ' + functionName);
    Components.utils.exportFunction(function () {
      var args = Array.prototype.slice.call(arguments, 0);
      traceExternalInterface && wrappedWindow.console.log('__flash__callIn: ' + functionName);
      var result;
      if (onExternalCallback) {
        result = onExternalCallback({functionName: functionName, args: args});
        traceExternalInterface && wrappedWindow.console.log('__flash__callIn (result): ' + result);
      }
      return wrappedWindow.eval(result);
    }, this, { defineAs: functionName });
  };
  wrappedObject.__flash__unregisterCallback = function (functionName) {
    traceExternalInterface && wrappedWindow.console.log('__flash__unregisterCallback: ' + functionName);
    delete this[functionName];
  };
}
