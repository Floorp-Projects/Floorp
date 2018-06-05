/* Copyright 2018 Mozilla Foundation
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

"use strict";

var EXPORTED_SYMBOLS = ["PdfJsRegistration"];

const Cm = Components.manager;

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "PdfStreamConverter",
  "resource://pdf.js/PdfStreamConverter.jsm");

// Register/unregister a constructor as a factory.
function StreamConverterFactory() {}
StreamConverterFactory.prototype = {

  // properties required for XPCOM registration:
  _classID: Components.ID("{d0c5195d-e798-49d4-b1d3-9324328b2291}"),
  _classDescription: "pdf.js Component",
  _contractID: "@mozilla.org/streamconv;1?from=application/pdf&to=*/*",

  _classID2: Components.ID("{d0c5195d-e798-49d4-b1d3-9324328b2292}"),
  _contractID2: "@mozilla.org/streamconv;1?from=application/pdf&to=text/html",

  register: function register() {
    var factory = {
      createInstance(outer, iid) {
        if (outer)
          throw Cr.NS_ERROR_NO_AGGREGATION;
        return (new PdfStreamConverter()).QueryInterface(iid);
      },
      QueryInterface: ChromeUtils.generateQI([Ci.nsIFactory])
    };

    this._factory = factory;

    var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.registerFactory(this._classID, this._classDescription,
                              this._contractID, factory);
    registrar.registerFactory(this._classID2, this._classDescription,
                              this._contractID2, factory);
  },

  unregister: function unregister() {
    var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.unregisterFactory(this._classID, this._factory);
    if (this._classID2) {
      registrar.unregisterFactory(this._classID2, this._factory);
    }
    this._factory = null;
  },
};

var PdfJsRegistration = {
  _registered: false,

  ensureRegistered: function ensureRegistered() {
    if (this._registered) {
      return;
    }
    this._pdfStreamConverterFactory = new StreamConverterFactory();
    this._pdfStreamConverterFactory.register();

    this._registered = true;
  },

  ensureUnregistered: function ensureUnregistered() {
    if (!this._registered) {
      return;
    }
    this._pdfStreamConverterFactory.unregister();
    delete this._pdfStreamConverterFactory;

    this._registered = false;
  },

};
