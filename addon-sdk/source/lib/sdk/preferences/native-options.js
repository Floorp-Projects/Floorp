/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  "stability": "unstable"
};

const { Cc, Ci, Cu } = require('chrome');
const { on } = require('../system/events');
const { id, preferencesBranch } = require('../self');
const { localizeInlineOptions } = require('../l10n/prefs');
const { Services } = require("resource://gre/modules/Services.jsm");
const { AddonManager } = Cu.import("resource://gre/modules/AddonManager.jsm");
const { defer } = require("sdk/core/promise");

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";;
const DEFAULT_OPTIONS_URL = 'data:text/xml,<placeholder/>';

const VALID_PREF_TYPES = ['bool', 'boolint', 'integer', 'string', 'color',
                          'file', 'directory', 'control', 'menulist', 'radio'];

const isFennec = require("sdk/system/xul-app").is("Fennec");

function enable({ preferences, id }) {
  let enabled = defer();

  validate(preferences);

  setDefaults(preferences, preferencesBranch);

  // allow the use of custom options.xul
  AddonManager.getAddonByID(id, (addon) => {
    on('addon-options-displayed', onAddonOptionsDisplayed, true);
    enabled.resolve({ id: id });
  });

  function onAddonOptionsDisplayed({ subject: doc, data }) {
    if (data === id) {
      let parent;

      if (isFennec) {
        parent = doc.querySelector('.options-box');

        // NOTE: This disable the CSS rule that makes the options invisible
        let item = doc.querySelector('#addons-details .addon-item');
        item.removeAttribute("optionsURL");
      } else {
        parent = doc.getElementById('detail-downloads').parentNode;
      }

      if (parent) {
        injectOptions({
          preferences: preferences,
          preferencesBranch: preferencesBranch,
          document: doc,
          parent: parent,
          id: id
        });
        localizeInlineOptions(doc);
      } else {
        throw Error("Preferences parent node not found in Addon Details. The configured custom preferences will not be visible.");
      }
    }
  }

  return enabled.promise;
}
exports.enable = enable;

// centralized sanity checks
function validate(preferences) {
  for (let { name, title, type, label, options } of preferences) {
    // make sure the title is set and non-empty
    if (!title)
      throw Error("The '" + name + "' pref requires a title");

    // make sure that pref type is a valid inline option type
    if (!~VALID_PREF_TYPES.indexOf(type))
      throw Error("The '" + name + "' pref must be of valid type");

    // if it's a control, make sure it has a label
    if (type === 'control' && !label)
      throw Error("The '" + name + "' control requires a label");

    // if it's a menulist or radio, make sure it has options
    if (type === 'menulist' || type === 'radio') {
      if (!options)
        throw Error("The '" + name + "' pref requires options");

      // make sure each option has a value and a label
      for (let item of options) {
        if (!('value' in item) || !('label' in item))
          throw Error("Each option requires both a value and a label");
      }
    }

    // TODO: check that pref type matches default value type
  }
}
exports.validate = validate;

// initializes default preferences, emulates defaults/prefs.js
function setDefaults(preferences, preferencesBranch) {
  const branch = Cc['@mozilla.org/preferences-service;1'].
                 getService(Ci.nsIPrefService).
                 getDefaultBranch('extensions.' + preferencesBranch + '.');
  for (let { name, value } of preferences) {
    switch (typeof value) {
      case 'boolean':
        branch.setBoolPref(name, value);
        break;
      case 'number':
        // must be integer, ignore otherwise
        if (value % 1 === 0) {
          branch.setIntPref(name, value);
        }
        break;
      case 'string':
        let str = Cc["@mozilla.org/supports-string;1"].
                  createInstance(Ci.nsISupportsString);
        str.data = value;
        branch.setComplexValue(name, Ci.nsISupportsString, str);
        break;
    }
  }
}
exports.setDefaults = setDefaults;

// dynamically injects inline options into about:addons page at runtime
// NOTE: on Firefox Desktop the about:addons page is a xul page document,
// on Firefox for Android the about:addons page is an xhtml page, to support both
// the XUL xml namespace have to be enforced.
function injectOptions({ preferences, preferencesBranch, document, parent, id }) {
  preferences.forEach(({name, type, hidden, title, description, label, options, on, off}) => {
    if (hidden) {
      return;
    }

    let setting = document.createElementNS(XUL_NS, 'setting');
    setting.setAttribute('pref-name', name);
    setting.setAttribute('data-jetpack-id', id);
    setting.setAttribute('pref', 'extensions.' + preferencesBranch + '.' + name);
    setting.setAttribute('type', type);
    setting.setAttribute('title', title);
    if (description)
      setting.setAttribute('desc', description);

    if (type === 'file' || type === 'directory') {
      setting.setAttribute('fullpath', 'true');
    }
    else if (type === 'control') {
      let button = document.createElementNS(XUL_NS, 'button');
      button.setAttribute('pref-name', name);
      button.setAttribute('data-jetpack-id', id);
      button.setAttribute('label', label);
      button.addEventListener('command', function() {
        Services.obs.notifyObservers(null, `${id}-cmdPressed`, name);
      }, true);
      setting.appendChild(button);
    }
    else if (type === 'boolint') {
      setting.setAttribute('on', on);
      setting.setAttribute('off', off);
    }
    else if (type === 'menulist') {
      let menulist = document.createElementNS(XUL_NS, 'menulist');
      let menupopup = document.createElementNS(XUL_NS, 'menupopup');
      for (let { value, label } of options) {
        let menuitem = document.createElementNS(XUL_NS, 'menuitem');
        menuitem.setAttribute('value', value);
        menuitem.setAttribute('label', label);
        menupopup.appendChild(menuitem);
      }
      menulist.appendChild(menupopup);
      setting.appendChild(menulist);
    }
    else if (type === 'radio') {
      let radiogroup = document.createElementNS(XUL_NS, 'radiogroup');
      for (let { value, label } of options) {
        let radio = document.createElementNS(XUL_NS, 'radio');
        radio.setAttribute('value', value);
        radio.setAttribute('label', label);
        radiogroup.appendChild(radio);
      }
      setting.appendChild(radiogroup);
    }

    parent.appendChild(setting);
  });
}
exports.injectOptions = injectOptions;
