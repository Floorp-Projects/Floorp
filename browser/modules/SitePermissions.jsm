/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [ "SitePermissions" ];

Components.utils.import("resource://gre/modules/Services.jsm");

let gStringBundle =
  Services.strings.createBundle("chrome://browser/locale/sitePermissions.properties");

this.SitePermissions = {

  UNKNOWN: Services.perms.UNKNOWN_ACTION,
  ALLOW: Services.perms.ALLOW_ACTION,
  BLOCK: Services.perms.DENY_ACTION,
  SESSION: Components.interfaces.nsICookiePermission.ACCESS_SESSION,

  /* Checks whether a UI for managing permissions should be exposed for a given
   * URI. This excludes file URIs, for instance, as they don't have a host,
   * even though nsIPermissionManager can still handle them.
   */
  isSupportedURI: function (aURI) {
    return aURI.schemeIs("http") || aURI.schemeIs("https");
  },

  /* Returns an array of all permission IDs.
   */
  listPermissions: function () {
    let array = Object.keys(gPermissionObject);
    array.sort((a, b) => {
      return this.getPermissionLabel(a).localeCompare(this.getPermissionLabel(b));
    });
    return array;
  },

  /* Returns an array of permission states to be exposed to the user for a
   * permission with the given ID.
   */
  getAvailableStates: function (aPermissionID) {
    if (aPermissionID in gPermissionObject &&
        gPermissionObject[aPermissionID].states)
      return gPermissionObject[aPermissionID].states;

    if (this.getDefault(aPermissionID) == this.UNKNOWN)
      return [ SitePermissions.UNKNOWN, SitePermissions.ALLOW, SitePermissions.BLOCK ];

    return [ SitePermissions.ALLOW, SitePermissions.BLOCK ];
  },

  /* Returns the default state of a particular permission.
   */
  getDefault: function (aPermissionID) {
    if (aPermissionID in gPermissionObject &&
        gPermissionObject[aPermissionID].getDefault)
      return gPermissionObject[aPermissionID].getDefault();

    return this.UNKNOWN;
  },

  /* Returns the state of a particular permission for a given URI.
   */
  get: function (aURI, aPermissionID) {
    if (!this.isSupportedURI(aURI))
      return this.UNKNOWN;

    let state;
    if (aPermissionID in gPermissionObject &&
        gPermissionObject[aPermissionID].exactHostMatch)
      state = Services.perms.testExactPermission(aURI, aPermissionID);
    else
      state = Services.perms.testPermission(aURI, aPermissionID);
    return state;
  },

  /* Sets the state of a particular permission for a given URI.
   */
  set: function (aURI, aPermissionID, aState) {
    if (!this.isSupportedURI(aURI))
      return;

    if (aState == this.UNKNOWN) {
      this.remove(aURI, aPermissionID);
      return;
    }

    Services.perms.add(aURI, aPermissionID, aState);

    if (aPermissionID in gPermissionObject &&
        gPermissionObject[aPermissionID].onChange)
      gPermissionObject[aPermissionID].onChange(aURI, aState);
  },

  /* Removes the saved state of a particular permission for a given URI.
   */
  remove: function (aURI, aPermissionID) {
    if (!this.isSupportedURI(aURI))
      return;

    Services.perms.remove(aURI.host, aPermissionID);

    if (aPermissionID in gPermissionObject &&
        gPermissionObject[aPermissionID].onChange)
      gPermissionObject[aPermissionID].onChange(aURI, this.UNKNOWN);
  },

  /* Returns the localized label for the permission with the given ID, to be
   * used in a UI for managing permissions.
   */
  getPermissionLabel: function (aPermissionID) {
    return gStringBundle.GetStringFromName("permission." + aPermissionID + ".label");
  },

  /* Returns the localized label for the given permission state, to be used in
   * a UI for managing permissions.
   */
  getStateLabel: function (aPermissionID, aState) {
    if (aPermissionID in gPermissionObject &&
        gPermissionObject[aPermissionID].getStateLabel) {
      let label = gPermissionObject[aPermissionID].getStateLabel(aState);
      if (label)
        return label;
    }

    switch (aState) {
      case this.UNKNOWN:
        return gStringBundle.GetStringFromName("alwaysAsk");
      case this.ALLOW:
        return gStringBundle.GetStringFromName("allow");
      case this.SESSION:
        return gStringBundle.GetStringFromName("allowForSession");
      case this.BLOCK:
        return gStringBundle.GetStringFromName("block");
      default:
        throw new Error("unknown permission state");
    }
  }
};

let gPermissionObject = {
  /* Holds permission ID => options pairs.
   *
   * Supported options:
   *
   *  - exactHostMatch
   *    Allows sub domains to have their own permissions.
   *    Defaults to false.
   *
   *  - getDefault
   *    Called to get the permission's default state.
   *    Defaults to UNKNOWN, indicating that the user will be asked each time
   *    a page asks for that permissions.
   *
   *  - getStateLabel
   *    Called to get the localized label for the given permission state, to be
   *    used in a UI for managing permissions. May return null for states that
   *    should use their default label.
   *
   *  - onChange
   *    Called when a permission state changes.
   *
   *  - states
   *    Array of permission states to be exposed to the user.
   *    Defaults to ALLOW, BLOCK and the default state (see getDefault).
   */

  "image": {
    getDefault: function () {
      return Services.prefs.getIntPref("permissions.default.image") == 2 ?
               SitePermissions.BLOCK : SitePermissions.ALLOW;
    }
  },

  "cookie": {
    states: [ SitePermissions.ALLOW, SitePermissions.SESSION, SitePermissions.BLOCK ],
    getDefault: function () {
      if (Services.prefs.getIntPref("network.cookie.cookieBehavior") == 2)
        return SitePermissions.BLOCK;

      if (Services.prefs.getIntPref("network.cookie.lifetimePolicy") == 2)
        return SitePermissions.SESSION;

      return SitePermissions.ALLOW;
    }
  },

  "desktop-notification": {},

  "camera": {},
  "microphone": {},

  "popup": {
    getDefault: function () {
      return Services.prefs.getBoolPref("dom.disable_open_during_load") ?
               SitePermissions.BLOCK : SitePermissions.ALLOW;
    }
  },

  "install": {
    getDefault: function () {
      return Services.prefs.getBoolPref("xpinstall.whitelist.required") ?
               SitePermissions.BLOCK : SitePermissions.ALLOW;
    }
  },

  "geo": {
    exactHostMatch: true
  },

  "indexedDB": {
    states: [ SitePermissions.ALLOW, SitePermissions.UNKNOWN, SitePermissions.BLOCK ],
    getStateLabel: function (aState) {
      // indexedDB redefines nsIPermissionManager.UNKNOWN_ACTION (the default)
      // as "allow" and nsIPermissionManager.ALLOW_ACTION as "ask the user."
      switch (aState) {
        case SitePermissions.UNKNOWN:
          return gStringBundle.GetStringFromName("allow");
        case SitePermissions.ALLOW:
          return gStringBundle.GetStringFromName("alwaysAsk");
        default:
          return null;
      }
    },
    onChange: function (aURI, aState) {
      if (aState == SitePermissions.ALLOW || aState == SitePermissions.BLOCK)
        Services.perms.remove(aURI.host, "indexedDB-unlimited");
    }
  },

  "fullscreen": {},

  "pointerLock": {
    exactHostMatch: true
  }
};
