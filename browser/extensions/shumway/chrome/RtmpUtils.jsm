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
    function genPropDesc(value) {
      return {
        enumerable: true, configurable: true, writable: true, value: value
      };
    }

    var host = params.host, port = params.port, ssl = params.ssl;

    var baseSocket = Cc["@mozilla.org/tcp-socket;1"].createInstance(Ci.nsIDOMTCPSocket);
    var socket = baseSocket.open(host, port, {useSecureTransport: ssl, binaryType: 'arraybuffer'});
    if (!socket) {
      return null;
    }

    socket.onopen = function () {
      if (wrapper.onopen) {
        wrapper.onopen.call(wrapper, new sandbox.Object());
      }
    };
    socket.ondata = function (e) {
      if (wrapper.ondata) {
        var wrappedE = new sandbox.Object();
        wrappedE.data = Components.utils.cloneInto(e.data, sandbox);
        wrapper.ondata.call(wrapper, wrappedE);
      }
    };
    socket.ondrain = function () {
      if (wrapper.ondrain) {
        wrapper.ondrain.call(wrapper, new sandbox.Object());
      }
    };
    socket.onerror = function (e) {
      if (wrapper.onerror) {
        var wrappedE = new sandbox.Object();
        wrappedE.data = Components.utils.cloneInto(e.data, sandbox);
        wrapper.onerror.call(wrapper, wrappedE);
      }
    };
    socket.onclose = function () {
      if (wrapper.onclose) {
        wrapper.onclose.call(wrapper, new sandbox.Object());
      }
    };

    var wrapper = Cu.createObjectIn(sandbox);
    Object.defineProperties(wrapper, {
      onopen: genPropDesc(null),
      ondata: genPropDesc(null),
      ondrain: genPropDesc(null),
      onerror: genPropDesc(null),
      onclose: genPropDesc(null),

      send: genPropDesc(function (buffer, offset, count) {
        return socket.send(buffer, offset, count);
      }),

      close: genPropDesc(function () {
        socket.close();
      })
    });
    Components.utils.makeObjectPropsNormal(wrapper);
    return wrapper;
  },

  createXHR: function (sandbox) {
    function genPropDesc(value) {
      return {
        enumerable: true, configurable: true, writable: true, value: value
      };
    }

    var xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                .createInstance(Ci.nsIXMLHttpRequest);

    xhr.onload = function () {
      wrapper.status = xhr.status;
      wrapper.response = Components.utils.cloneInto(xhr.response, sandbox);
      if (wrapper.onload) {
        wrapper.onload.call(wrapper, new sandbox.Object());
      }
    };
    xhr.onerror = function () {
      wrapper.status = xhr.status;
      if (wrapper.onerror) {
        wrapper.onerror.call(wrapper, new sandbox.Object());
      }
    };

    var wrapper = Components.utils.createObjectIn(sandbox);
    Object.defineProperties(wrapper, {
      status: genPropDesc(0),
      response: genPropDesc(undefined),
      responseType: genPropDesc('text'),

      onload: genPropDesc(null),
      onerror: genPropDesc(null),

      open: genPropDesc(function (method, path, async) {
        if (method !== 'POST' || !path || (async !== undefined && !async)) {
          throw new Error('invalid open() arguments');
        }
        // TODO check path
        xhr.open('POST', path, true);
        xhr.responseType = 'arraybuffer';
        xhr.setRequestHeader('Content-Type', 'application/x-fcs');
      }),

      setRequestHeader: genPropDesc(function (header, value) {
        if (header !== 'Content-Type' || value !== 'application/x-fcs') {
          throw new Error('invalid setRequestHeader() arguments');
        }
      }),

      send: genPropDesc(function (data) {
        if (this.responseType !== 'arraybuffer') {
          throw new Error('Invalid responseType.');
        }
        xhr.send(data);
      })
    });
    Components.utils.makeObjectPropsNormal(wrapper);
    return wrapper;
  }
};
