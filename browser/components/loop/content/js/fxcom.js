/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");
const Ci = Components.interfaces;

/**
 * Data accessor for chrome resources. This is used to provide an
 * interface for l10n.js to use.
 *
 * XXX currently this file accesses chrome direct, and hence is a big
 * simplification on what it could be. When we implement
 * bug 976109 we'll likely need to do a proxy to the chrome space.
 */
var FirefoxCom = (function FirefoxComClosure() {
  return {
    /**
     * Handles requests for data synchronously.
     */
    requestSync: function(action, data) {
      if (action === 'getLocale')
        return this.getLocale();

      if (action === 'getStrings')
        return this.getStrings(data);

      console.error('[fxcom] Action' + action + ' is unknown');
      return null;
    },

    getLocale: function() {
      try {
        return Services.prefs.getCharPref('general.useragent.locale');
      } catch (x) {
        return 'en-US';
      }
    },

    getStrings: function(data) {
      try {
        return JSON.stringify(this.localizedStrings[data] || null);
      } catch (e) {
        console.error('[fxcom] Unable to retrive localized strings: ' + e);
      }
    },

    get localizedStrings() {
      var stringBundle =
        Services.strings.createBundle('chrome://browser/locale/loop/loop.properties');

      var map = {};
      var enumerator = stringBundle.getSimpleEnumeration();
      while (enumerator.hasMoreElements()) {
        var string = enumerator.getNext().QueryInterface(Ci.nsIPropertyElement);
        var key = string.key, property = 'textContent';
        var i = key.lastIndexOf('.');
        if (i >= 0) {
          property = key.substring(i + 1);
          key = key.substring(0, i);
        }
        if (!(key in map))
          map[key] = {};
        map[key][property] = string.value;
      }
      delete this.localizedStrings;
      return this.localizedStrings = map;
    }
  };
})();
