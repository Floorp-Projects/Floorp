/*
 * Copyright 2014 Mozilla Foundation
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

var EXPORTED_SYMBOLS = ['RtmpUtils'];

Components.utils.import('resource://gre/modules/Services.jsm');

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

var RtmpUtils = {
  get isRtmpEnabled() {
    try {
      return Services.prefs.getBoolPref('shumway.rtmp.enabled');
    } catch (ex) {
      return false;
    }
  },

  createSocket: function (sandbox, params) {
    var host = params.host, port = params.port, ssl = params.ssl;

    var baseSocket = Cc["@mozilla.org/tcp-socket;1"].createInstance(Ci.nsIDOMTCPSocket);
    var socket = baseSocket.open(host, port, {useSecureTransport: ssl, binaryType: 'arraybuffer'});
    if (!socket) {
      return null;
    }

    var wrapperOnOpen = null, wrapperOnData = null, wrapperOnDrain = null;
    var wrapperOnError = null, wrapperOnClose = null;
    socket.onopen = function () {
      if (wrapperOnOpen) {
        wrapperOnOpen.call(wrapper, new sandbox.Object());
      }
    };
    socket.ondata = function (e) {
      if (wrapperOnData) {
        var wrappedE = new sandbox.Object();
        wrappedE.data = Components.utils.cloneInto(e.data, sandbox);
        wrapperOnData.call(wrapper, wrappedE);
      }
    };
    socket.ondrain = function () {
      if (wrapperOnDrain) {
        wrapperOnDrain.call(wrapper, new sandbox.Object());
      }
    };
    socket.onerror = function (e) {
      if (wrapperOnError) {
        var wrappedE = new sandbox.Object();
        wrappedE.data = Components.utils.cloneInto(e.data, sandbox);
        wrapperOnError.call(wrapper, wrappedE);
      }
    };
    socket.onclose = function () {
      if (wrapperOnClose) {
        wrapperOnClose.call(wrapper, new sandbox.Object());
      }
    };

    var wrapper = new sandbox.Object();
    var waived = Components.utils.waiveXrays(wrapper);
    Object.defineProperties(waived, {
      onopen: {
        get: function () { return wrapperOnOpen; },
        set: function (value) { wrapperOnOpen = value; },
        enumerable: true
      },
      ondata: {
        get: function () { return wrapperOnData; },
        set: function (value) { wrapperOnData = value; },
        enumerable: true
      },
      ondrain: {
        get: function () { return wrapperOnDrain; },
        set: function (value) { wrapperOnDrain = value; },
        enumerable: true
      },
      onerror: {
        get: function () { return wrapperOnError; },
        set: function (value) { wrapperOnError = value; },
        enumerable: true
      },
      onclose: {
        get: function () { return wrapperOnClose; },
        set: function (value) { wrapperOnClose = value; },
        enumerable: true
      },

      send: {
        value: function (buffer, offset, count) {
          return socket.send(buffer, offset, count);
        }
      },

      close: {
        value: function () {
          socket.close();
        }
      }
    });
    return wrapper;
  },

  createXHR: function (sandbox) {
    var xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                .createInstance(Ci.nsIXMLHttpRequest);
    var wrapperOnLoad = null, wrapperOnError = null;
    xhr.onload = function () {
      if (wrapperOnLoad) {
        wrapperOnLoad.call(wrapper, new sandbox.Object());
      }
    };
    xhr.onerror = function () {
      if (wrappedOnError) {
        wrappedOnError.call(wrapper, new sandbox.Object());
      }
    };

    var wrapper = new sandbox.Object();
    var waived = Components.utils.waiveXrays(wrapper);
    Object.defineProperties(waived, {
      status: {
        get: function () { return xhr.status; },
        enumerable: true
      },
      response: {
        get: function () { return Components.utils.cloneInto(xhr.response, sandbox); },
        enumerable: true
      },
      responseType: {
        get: function () { return xhr.responseType; },
        set: function (value) {
          if (value !== 'arraybuffer') {
            throw new Error('Invalid responseType.');
          }
        },
        enumerable: true
      },
      onload: {
        get: function () { return wrapperOnLoad; },
        set: function (value) { wrapperOnLoad = value; },
        enumerable: true
      },
      onerror: {
        get: function () { return wrapperOnError; },
        set: function (value) { wrapperOnError = value; },
        enumerable: true
      },
      open: {
        value: function (method, path, async) {
          if (method !== 'POST' || !path || (async !== undefined && !async)) {
            throw new Error('invalid open() arguments');
          }
          // TODO check path
          xhr.open('POST', path, true);
          xhr.responseType = 'arraybuffer';
          xhr.setRequestHeader('Content-Type', 'application/x-fcs');
        }
      },
      setRequestHeader: {
        value: function (header, value) {
          if (header !== 'Content-Type' || value !== 'application/x-fcs') {
            throw new Error('invalid setRequestHeader() arguments');
          }
        }
      },

      send: {
        value: function (data) {
          xhr.send(data);
        }
      }
    });
    return wrapper;
  }
};
