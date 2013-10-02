/* Copyright 2012 Mozilla Foundation
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

var EXPORTED_SYMBOLS = ["ShumwayUtils"];

const RESOURCE_NAME = 'shumway';
const EXT_PREFIX = 'shumway@research.mozilla.org';
const SWF_CONTENT_TYPE = 'application/x-shockwave-flash';
const PREF_PREFIX = 'shumway.';
const PREF_DISABLED = PREF_PREFIX + 'disabled';
const PREF_IGNORE_CTP = PREF_PREFIX + 'ignoreCTP';

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cm = Components.manager;
let Cu = Components.utils;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://shumway.components/FlashStreamConverter.js');

let Svc = {};
XPCOMUtils.defineLazyServiceGetter(Svc, 'mime',
                                   '@mozilla.org/mime;1',
                                   'nsIMIMEService');
XPCOMUtils.defineLazyServiceGetter(Svc, 'pluginHost',
                                   '@mozilla.org/plugin/host;1',
                                   'nsIPluginHost');

function getBoolPref(pref, def) {
  try {
    return Services.prefs.getBoolPref(pref);
  } catch (ex) {
    return def;
  }
}

function log(str) {
  dump(str + '\n');
}

// Register/unregister a constructor as a component.
function Factory() {}

Factory.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory]),
  _targetConstructor: null,

  register: function register(targetConstructor) {
    this._targetConstructor = targetConstructor;
    var proto = targetConstructor.prototype;
    var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.registerFactory(proto.classID, proto.classDescription,
                              proto.contractID, this);
  },

  unregister: function unregister() {
    var proto = this._targetConstructor.prototype;
    var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.unregisterFactory(proto.classID, this);
    this._targetConstructor = null;
  },

  // nsIFactory
  createInstance: function createInstance(aOuter, iid) {
    if (aOuter !== null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return (new (this._targetConstructor)).QueryInterface(iid);
  },

  // nsIFactory
  lockFactory: function lockFactory(lock) {
    // No longer used as of gecko 1.7.
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }
};

let factory1 = new Factory();
let factory2 = new Factory();

let ShumwayUtils = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
  _registered: false,

  init: function init() {
    if (this.enabled)
      this._ensureRegistered();
    else
      this._ensureUnregistered();

    // Listen for when shumway is completely disabled.
    Services.prefs.addObserver(PREF_DISABLED, this, false);
  },

  // nsIObserver
  observe: function observe(aSubject, aTopic, aData) {
    if (this.enabled)
      this._ensureRegistered();
    else
      this._ensureUnregistered();
  },
  
  /**
   * shumway is only enabled if it is both selected as the pdf viewer and if the 
   * global switch enabling it is true.
   * @return {boolean} Wether or not it's enabled.
   */
  get enabled() {
    return !getBoolPref(PREF_DISABLED, true);
  },

  _ensureRegistered: function _ensureRegistered() {
    if (this._registered)
      return;

    // Load the component and register it.
    factory1.register(FlashStreamConverter1);
    factory2.register(FlashStreamConverter2);

    var ignoreCTP = getBoolPref(PREF_IGNORE_CTP, true);

    Svc.pluginHost.registerPlayPreviewMimeType(SWF_CONTENT_TYPE, ignoreCTP);

    this._registered = true;

    log('Shumway is registered');
  },

  _ensureUnregistered: function _ensureUnregistered() {
    if (!this._registered)
      return;

    // Remove the contract/component.
    factory1.unregister();
    factory2.unregister();

    Svc.pluginHost.unregisterPlayPreviewMimeType(SWF_CONTENT_TYPE);

    this._registered = false;

    log('Shumway is unregistered');
  }
};
