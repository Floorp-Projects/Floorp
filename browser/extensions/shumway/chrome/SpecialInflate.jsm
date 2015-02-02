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

var EXPORTED_SYMBOLS = ['SpecialInflate', 'SpecialInflateUtils'];

Components.utils.import('resource://gre/modules/Services.jsm');

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

function SimpleStreamListener() {
  this.binaryStream = Cc['@mozilla.org/binaryinputstream;1']
    .createInstance(Ci.nsIBinaryInputStream);
  this.onData = null;
  this.buffer = null;
}
SimpleStreamListener.prototype = {
  QueryInterface: function (iid) {
    if (iid.equals(Ci.nsIStreamListener) ||
      iid.equals(Ci.nsIRequestObserver) ||
      iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  onStartRequest: function (aRequest, aContext) {
    return Cr.NS_OK;
  },
  onStopRequest: function (aRequest, aContext, sStatusCode) {
    return Cr.NS_OK;
  },
  onDataAvailable: function (aRequest, aContext, aInputStream, aOffset, aCount) {
    this.binaryStream.setInputStream(aInputStream);
    if (!this.buffer || aCount > this.buffer.byteLength) {
      this.buffer = new ArrayBuffer(aCount);
    }
    this.binaryStream.readArrayBuffer(aCount, this.buffer);
    this.onData(new Uint8Array(this.buffer, 0, aCount));
    return Cr.NS_OK;
  }
};

function SpecialInflate() {
  var listener = new SimpleStreamListener();
  listener.onData = function (data) {
    this.onData(data);
  }.bind(this);

  var converterService = Cc["@mozilla.org/streamConverters;1"].getService(Ci.nsIStreamConverterService);
  var converter = converterService.asyncConvertData("deflate", "uncompressed", listener, null);
  converter.onStartRequest(null, null);
  this.converter = converter;

  var binaryStream = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(Ci.nsIBinaryOutputStream);
  var pipe = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);
  pipe.init(true, true, 0, 0xFFFFFFFF, null);
  binaryStream.setOutputStream(pipe.outputStream);
  this.binaryStream = binaryStream;

  this.pipeInputStream = pipe.inputStream;

  this.onData = null;
}
SpecialInflate.prototype = {
  push: function (data) {
    this.binaryStream.writeByteArray(data, data.length);
    this.converter.onDataAvailable(null, null, this.pipeInputStream, 0, data.length);
  },
  close: function () {
    this.binaryStream.close();
    this.converter.onStopRequest(null, null, Cr.NS_OK);
  }
};

var SpecialInflateUtils = {
  get isSpecialInflateEnabled() {
    try {
      return Services.prefs.getBoolPref('shumway.specialInflate');
    } catch (ex) {
      return false; // TODO true;
    }
  },

  createWrappedSpecialInflate: function (sandbox) {
    var wrapped = new SpecialInflate();
    var wrapperOnData = null;
    wrapped.onData = function(data) {
      if (wrapperOnData) {
        wrapperOnData.call(wrapper, Components.utils.cloneInto(data, sandbox));
      }
    };
    // We will return object created in the sandbox/content, with some exposed
    // properties/methods, so we can send data between wrapped object and
    // and sandbox/content.
    var wrapper = new sandbox.Object();
    var waived = Components.utils.waiveXrays(wrapper);
    Object.defineProperties(waived, {
      onData: {
        get: function () { return wrapperOnData; },
        set: function (value) { wrapperOnData = value; },
        enumerable: true
      },
      push: {
        value: function (data) {
          // Uint8Array is expected in the data parameter.
          // SpecialInflate.push() fails with other argument types.
          return wrapped.push(data);
        }
      },
      close: {
        value: function () {
          return wrapped.close();
        }
      }
    });
    return wrapper;
  }
};
