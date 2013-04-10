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
    switch (topic) {
      case 'app-startup':
        let enumerator = cm.enumerateCategory("JavaScript-navigator-property-maybe");
        while (enumerator.hasMoreElements()) {
          let entry = enumerator.getNext().QueryInterface(Ci.nsISupportsCString);
          let keyVal = cm.getCategoryEntry("JavaScript-navigator-property-maybe", entry).split(',');
          try {
            if (Services.prefs.getBoolPref(entry)) {
              if (DEBUG) debug("enable: " + keyVal[0]);
              cm.addCategoryEntry("JavaScript-navigator-property", keyVal[0], keyVal[1], false, false);
            }
          } catch (ex) {
            if (DEBUG) debug("no pref found: " + entry);
          }
          Services.prefs.addObserver(entry, this, true);
        }
        break;
      case 'nsPref:changed':
        let keyVal = cm.getCategoryEntry("JavaScript-navigator-property-maybe", data).split(',');
        let key = keyVal[0];
        let val = keyVal[1];
        try {
          if (Services.prefs.getBoolPref(data)) {
            if (DEBUG) debug("enable: " + key);
            cm.addCategoryEntry("JavaScript-navigator-property", key, val, false, false);
          } else {
            if (DEBUG) debug("disable: " + key);
            cm.deleteCategoryEntry("JavaScript-navigator-property", key, false);
          }
        } catch (ex) {
          if (DEBUG) debug("no pref found: " + data);
          cm.deleteCategoryEntry("JavaScript-navigator-property", key, false);
        }
        break;
      default:
        if (DEBUG) debug("unknown topic: " + topic);
    }
  },
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NavigatorPropertyHelper])
