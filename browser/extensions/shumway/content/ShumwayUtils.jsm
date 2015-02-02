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

const PREF_PREFIX = 'shumway.';
const PREF_DISABLED = PREF_PREFIX + 'disabled';

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cm = Components.manager;
let Cu = Components.utils;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');

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

let ShumwayUtils = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
  _registered: false,

  init: function init() {
    if (this.enabled)
      this._ensureRegistered();
    else
      this._ensureUnregistered();

    Cc["@mozilla.org/parentprocessmessagemanager;1"]
      .getService(Ci.nsIMessageBroadcaster)
      .addMessageListener('Shumway:Chrome:isEnabled', this);

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

  receiveMessage: function(message) {
    switch (message.name) {
      case 'Shumway:Chrome:isEnabled':
        return this.enabled;
    }
  },
  
  /**
   * shumway is only enabled if the global switch enabling is true.
   * @return {boolean} Wether or not it's enabled.
   */
  get enabled() {
    return !getBoolPref(PREF_DISABLED, true);
  },

  _ensureRegistered: function _ensureRegistered() {
    if (this._registered)
      return;

    // Load the component and register it.
    Cu.import('resource://shumway/ShumwayBootstrapUtils.jsm');
    ShumwayBootstrapUtils.register();

    this._registered = true;

    log('Shumway is registered');

    let globalMM = Cc['@mozilla.org/globalmessagemanager;1']
      .getService(Ci.nsIFrameScriptLoader);
    globalMM.broadcastAsyncMessage('Shumway:Child:refreshSettings');
  },

  _ensureUnregistered: function _ensureUnregistered() {
    if (!this._registered)
      return;

    // Remove the contract/component.
    ShumwayBootstrapUtils.unregister();
    Cu.unload('resource://shumway/ShumwayBootstrapUtils.jsm');

    this._registered = false;

    log('Shumway is unregistered');

    let globalMM = Cc['@mozilla.org/globalmessagemanager;1']
      .getService(Ci.nsIFrameScriptLoader);
    globalMM.broadcastAsyncMessage('Shumway:Child:refreshSettings');
  }
};
