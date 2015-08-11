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

'use strict';

var EXPORTED_SYMBOLS = ['ShumwayBootstrapUtils'];

const PREF_PREFIX = 'shumway.';
const PREF_IGNORE_CTP = PREF_PREFIX + 'ignoreCTP';
const PREF_WHITELIST = PREF_PREFIX + 'swf.whitelist';
const SWF_CONTENT_TYPE = 'application/x-shockwave-flash';
const PLUGIN_HANLDER_URI = 'chrome://shumway/content/content.html';

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cm = Components.manager;
let Cu = Components.utils;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');

let Ph = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);

function getBoolPref(pref, def) {
  try {
    return Services.prefs.getBoolPref(pref);
  } catch (ex) {
    return def;
  }
}

function getStringPref(pref, def) {
  try {
    return Services.prefs.getComplexValue(pref, Ci.nsISupportsString).data;
  } catch (ex) {
    return def;
  }
}

function log(str) {
  var msg = 'ShumwayBootstrapUtils.jsm: ' + str;
  Services.console.logStringMessage(msg);
  dump(msg + '\n');
}

// Register/unregister a constructor as a factory.
function Factory() {}
Factory.prototype = {
  register: function register(targetConstructor) {
    var proto = targetConstructor.prototype;
    this._classID = proto.classID;

    var factory = XPCOMUtils._getFactory(targetConstructor);
    this._factory = factory;

    var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.registerFactory(proto.classID, proto.classDescription,
      proto.contractID, factory);
  },

  unregister: function unregister() {
    var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.unregisterFactory(this._classID, this._factory);
  }
};

function allowedPlatformForMedia() {
  var oscpu = Cc["@mozilla.org/network/protocol;1?name=http"]
                .getService(Ci.nsIHttpProtocolHandler).oscpu;
  if (oscpu.indexOf('Windows NT') === 0) {
    return oscpu.indexOf('Windows NT 5') < 0; // excluding Windows XP
  }
  if (oscpu.indexOf('Intel Mac OS X') === 0) {
    return true;
  }
  // Other platforms are not supported yet.
  return false;
}

var ShumwayBootstrapUtils = {
  isRegistered: false,
  isJSPluginsSupported: false,

  register: function () {
    if (this.isRegistered) {
      return;
    }

    this.isRegistered = true;

    // Register the components.
    this.isJSPluginsSupported = !!Ph.registerFakePlugin &&
                                getBoolPref('shumway.jsplugins', false);

    if (this.isJSPluginsSupported) {
      let initPluginDict = {
        handlerURI: PLUGIN_HANLDER_URI,
        mimeEntries: [
          {
            type: SWF_CONTENT_TYPE,
            description: 'Shockwave Flash',
            extension: 'swf'
          }
        ],
        niceName: 'Shumway plugin',
        name: 'Shumway',
        supersedeExisting: true, // TODO verify when jsplugins (bug 558184) is implemented
        sandboxScript: 'chrome://shumway/content/plugin.js', // TODO verify when jsplugins (bug 558184) is implemented
        version: '10.0.0.0'
      };
      Ph.registerFakePlugin(initPluginDict);
    } else {
      Cu.import('resource://shumway/ShumwayStreamConverter.jsm');

      let converterFactory = new Factory();
      converterFactory.register(ShumwayStreamConverter);
      this.converterFactory = converterFactory;
      let overlayConverterFactory = new Factory();
      overlayConverterFactory.register(ShumwayStreamOverlayConverter);
      this.overlayConverterFactory = overlayConverterFactory;

      let registerOverlayPreview = 'registerPlayPreviewMimeType' in Ph;
      if (registerOverlayPreview) {
        var ignoreCTP = getBoolPref(PREF_IGNORE_CTP, true);
        var whitelist = getStringPref(PREF_WHITELIST);
        // Some platforms cannot support video playback, and our whitelist targets
        // only video players atm. We need to disable Shumway for those platforms.
        if (whitelist && !Services.prefs.prefHasUserValue(PREF_WHITELIST) && !allowedPlatformForMedia()) {
          log('Default SWF whitelist is used on an unsupported platform -- ' +
          'using demo whitelist.');
          whitelist = 'http://www.areweflashyet.com/*.swf';
        }
        Ph.registerPlayPreviewMimeType(SWF_CONTENT_TYPE, ignoreCTP,
          undefined, whitelist);
      }
      this.registerOverlayPreview = registerOverlayPreview;
    }
  },

  unregister: function () {
    if (!this.isRegistered) {
      return;
    }

    this.isRegistered = false;

    // Remove the contract/component.
    if (this.isJSPluginsSupported) {
      Ph.unregisterFakePlugin(PLUGIN_HANLDER_URI);
    } else {
      this.converterFactory.unregister();
      this.converterFactory = null;
      this.overlayConverterFactory.unregister();
      this.overlayConverterFactory = null;

      if (this.registerOverlayPreview) {
        Ph.unregisterPlayPreviewMimeType(SWF_CONTENT_TYPE);
      }
    }
  }
};
