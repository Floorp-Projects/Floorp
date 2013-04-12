/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) { dump("-*- NavigatorPropertyHelper: " + s + "\n"); }

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function NavigatorPropertyHelper() { };

NavigatorPropertyHelper.prototype = {

  classID : Components.ID("{f0d03420-a0ce-11e2-9e96-0800200c9a66}"),
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]),

  observe: function observe(subject, topic, data) {
    if (DEBUG) debug("topic: " + topic + ", data: " + data);
    let cm = Components.classes["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);

    let enableAndAddObserver = function(oldCategory, newCategory) {
      let enumerator = cm.enumerateCategory(oldCategory);
      while (enumerator.hasMoreElements()) {
        let entry = enumerator.getNext().QueryInterface(Ci.nsISupportsCString);
        let keyVal = cm.getCategoryEntry(oldCategory, entry).split(',');
        try {
          if (Services.prefs.getBoolPref(entry)) {
            if (DEBUG) debug("enable: " + keyVal[0]);
            cm.addCategoryEntry(newCategory, keyVal[0], keyVal[1], false, false);
          }
        } catch (ex) {
          if (DEBUG) debug("no pref found: " + entry);
        }
        Services.prefs.addObserver(entry, this, true);
      }
    }.bind(this);

    function addOrRemoveCategory(oldCategory, newCategory) {
      let keyVal = cm.getCategoryEntry(oldCategory, data).split(',');
      let key = keyVal[0];
      let val = keyVal[1];
      try {
        if (Services.prefs.getBoolPref(data)) {
          if (DEBUG) debug("enable: " + key);
          cm.addCategoryEntry(newCategory, key, val, false, false);
        } else {
          if (DEBUG) debug("disable: " + key);
          cm.deleteCategoryEntry(newCategory, key, false);
        }
      } catch (ex) {
        cm.deleteCategoryEntry(newCategory, key, false);
        if (DEBUG) debug("no pref found: " + data);
      }
    }

    switch (topic) {
      case 'app-startup':
        enableAndAddObserver("JavaScript-navigator-property-maybe", "JavaScript-navigator-property");
        enableAndAddObserver("JavaScript-global-constructor-maybe", "JavaScript-global-constructor");
        break;
      case 'nsPref:changed':
        addOrRemoveCategory("JavaScript-navigator-property-maybe", "JavaScript-navigator-property");
        addOrRemoveCategory("JavaScript-global-constructor-maybe", "JavaScript-global-constructor");
        break;
      default:
        if (DEBUG) debug("unknown topic: " + topic);
    }
  },
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NavigatorPropertyHelper])
