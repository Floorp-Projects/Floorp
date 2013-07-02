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
    return Object.keys(gPermissionObject);
  },

  /* Returns an array of permission states to be exposed to the user for a
   * permission with the given ID.
   */
  getAvailableStates: function (aPermissionID) {
    return gPermissionObject[aPermissionID].states ||
           [ SitePermissions.ALLOW, SitePermissions.BLOCK ];
  },

  /* Returns the state of a perticular permission for a given URI.
   */
  get: function (aURI, aPermissionID) {
    if (!this.isSupportedURI(aURI))
      return this.UNKNOWN;

    let state;
    if (gPermissionObject[aPermissionID].exactHostMatch)
      state = Services.perms.testExactPermission(aURI, aPermissionID);
    else
      state = Services.perms.testPermission(aURI, aPermissionID);
    return state;
  },

  /* Sets the state of a perticular permission for a given URI.
   */
  set: function (aURI, aPermissionID, aState) {
    if (!this.isSupportedURI(aURI))
      return;

    Services.perms.add(aURI, aPermissionID, aState);

    if (gPermissionObject[aPermissionID].onSet)
      gPermissionObject[aPermissionID].onSet(aURI, aState);
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
  getStateLabel: function (aState) {
    switch (aState) {
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
   *  - onSet
   *    Called when a permission state changes.
   *
   *  - states
   *    Array of permission states to be exposed to the user.
   *    Defaults to ALLOW and BLOCK.
   */

  "image": {},

  "cookie": {
    states: [ SitePermissions.ALLOW, SitePermissions.SESSION, SitePermissions.BLOCK ]
  },

  "desktop-notification": {},

  "popup": {},

  "install": {},

  "geo": {
    exactHostMatch: true
  },

  "indexedDB": {
    onSet: function (aURI, aState) {
      if (aState == SitePermissions.ALLOW || aState == SitePermissions.BLOCK)
        Services.perms.remove(aURI.host, "indexedDB-unlimited");
    }
  },

  "fullscreen": {},

  "pointerLock": {
    exactHostMatch: true
  }
};
