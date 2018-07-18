/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = [ "SitePermissions" ];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

var gStringBundle =
  Services.strings.createBundle("chrome://browser/locale/sitePermissions.properties");

/**
 * A helper module to manage temporarily blocked permissions.
 *
 * Permissions are keyed by browser, so methods take a Browser
 * element to identify the corresponding permission set.
 *
 * This uses a WeakMap to key browsers, so that entries are
 * automatically cleared once the browser stops existing
 * (once there are no other references to the browser object);
 */
const TemporaryBlockedPermissions = {
  // This is a three level deep map with the following structure:
  //
  // Browser => {
  //   <prePath>: {
  //     <permissionID>: {Number} <timeStamp>
  //   }
  // }
  //
  // Only the top level browser elements are stored via WeakMap. The WeakMap
  // value is an object with URI prePaths as keys. The keys of that object
  // are ids that identify permissions that were set for the specific URI.
  // The final value is an object containing the timestamp of when the permission
  // was set (in order to invalidate after a certain amount of time has passed).
  _stateByBrowser: new WeakMap(),

  // Private helper method that bundles some shared behavior for
  // get() and getAll(), e.g. deleting permissions when they have expired.
  _get(entry, prePath, id, timeStamp) {
    if (timeStamp == null) {
      delete entry[prePath][id];
      return null;
    }
    if (timeStamp + SitePermissions.temporaryPermissionExpireTime < Date.now()) {
      delete entry[prePath][id];
      return null;
    }
    return {id, state: SitePermissions.BLOCK, scope: SitePermissions.SCOPE_TEMPORARY};
  },

  // Sets a new permission for the specified browser.
  set(browser, id) {
    if (!browser) {
      return;
    }
    if (!this._stateByBrowser.has(browser)) {
      this._stateByBrowser.set(browser, {});
    }
    let entry = this._stateByBrowser.get(browser);
    let prePath = browser.currentURI.prePath;
    if (!entry[prePath]) {
      entry[prePath] = {};
    }
    entry[prePath][id] = Date.now();
  },

  // Removes a permission with the specified id for the specified browser.
  remove(browser, id) {
    if (!browser) {
      return;
    }
    let entry = this._stateByBrowser.get(browser);
    let prePath = browser.currentURI.prePath;
    if (entry && entry[prePath]) {
      delete entry[prePath][id];
    }
  },

  // Gets a permission with the specified id for the specified browser.
  get(browser, id) {
    if (!browser || !browser.currentURI) {
      return null;
    }
    let entry = this._stateByBrowser.get(browser);
    let prePath = browser.currentURI.prePath;
    if (entry && entry[prePath]) {
      let permission = entry[prePath][id];
      return this._get(entry, prePath, id, permission);
    }
    return null;
  },

  // Gets all permissions for the specified browser.
  // Note that only permissions that apply to the current URI
  // of the passed browser element will be returned.
  getAll(browser) {
    let permissions = [];
    let entry = this._stateByBrowser.get(browser);
    let prePath = browser.currentURI.prePath;
    if (entry && entry[prePath]) {
      let timeStamps = entry[prePath];
      for (let id of Object.keys(timeStamps)) {
        let permission = this._get(entry, prePath, id, timeStamps[id]);
        // _get() returns null when the permission has expired.
        if (permission) {
          permissions.push(permission);
        }
      }
    }
    return permissions;
  },

  // Clears all permissions for the specified browser.
  // Unlike other methods, this does NOT clear only for
  // the currentURI but the whole browser state.
  clear(browser) {
    this._stateByBrowser.delete(browser);
  },

  // Copies the temporary permission state of one browser
  // into a new entry for the other browser.
  copy(browser, newBrowser) {
    let entry = this._stateByBrowser.get(browser);
    if (entry) {
      this._stateByBrowser.set(newBrowser, entry);
    }
  },
};

/**
 * A module to manage permanent and temporary permissions
 * by URI and browser.
 *
 * Some methods have the side effect of dispatching a "PermissionStateChange"
 * event on changes to temporary permissions, as mentioned in the respective docs.
 */
var SitePermissions = {
  // Permission states.
  // PROMPT_HIDE state is only used to show the "Hide Prompt" state in the identity panel
  // for the "plugin:flash" permission and not in pageinfo.
  UNKNOWN: Services.perms.UNKNOWN_ACTION,
  ALLOW: Services.perms.ALLOW_ACTION,
  BLOCK: Services.perms.DENY_ACTION,
  PROMPT: Services.perms.PROMPT_ACTION,
  ALLOW_COOKIES_FOR_SESSION: Ci.nsICookiePermission.ACCESS_SESSION,
  PROMPT_HIDE: Ci.nsIObjectLoadingContent.PLUGIN_PERMISSION_PROMPT_ACTION_QUIET,

  // Permission scopes.
  SCOPE_REQUEST: "{SitePermissions.SCOPE_REQUEST}",
  SCOPE_TEMPORARY: "{SitePermissions.SCOPE_TEMPORARY}",
  SCOPE_SESSION: "{SitePermissions.SCOPE_SESSION}",
  SCOPE_PERSISTENT: "{SitePermissions.SCOPE_PERSISTENT}",
  SCOPE_POLICY: "{SitePermissions.SCOPE_POLICY}",

  _defaultPrefBranch: Services.prefs.getBranch("permissions.default."),

  /**
   * Gets all custom permissions for a given URI.
   * Install addon permission is excluded, check bug 1303108.
   *
   * @return {Array} a list of objects with the keys:
   *          - id: the permissionId of the permission
   *          - scope: the scope of the permission (e.g. SitePermissions.SCOPE_TEMPORARY)
   *          - state: a constant representing the current permission state
   *            (e.g. SitePermissions.ALLOW)
   */
  getAllByURI(uri) {
    let result = [];
    if (!this.isSupportedURI(uri)) {
      return result;
    }

    let permissions = Services.perms.getAllForURI(uri);
    while (permissions.hasMoreElements()) {
      let permission = permissions.getNext();

      // filter out unknown permissions
      if (gPermissionObject[permission.type]) {
        // XXX Bug 1303108 - Control Center should only show non-default permissions
        if (permission.type == "install") {
          continue;
        }

        // Hide canvas permission when privacy.resistFingerprinting is false.
        if ((permission.type == "canvas") &&
            !Services.prefs.getBoolPref("privacy.resistFingerprinting")) {
          continue;
        }

        let scope = this.SCOPE_PERSISTENT;
        if (permission.expireType == Services.perms.EXPIRE_SESSION) {
          scope = this.SCOPE_SESSION;
        } else if (permission.expireType == Services.perms.EXPIRE_POLICY) {
          scope = this.SCOPE_POLICY;
        }

        result.push({
          id: permission.type,
          scope,
          state: permission.capability,
        });
      }
    }

    return result;
  },

  /**
   * Returns all custom permissions for a given browser.
   *
   * To receive a more detailed, albeit less performant listing see
   * SitePermissions.getAllPermissionDetailsForBrowser().
   *
   * @param {Browser} browser
   *        The browser to fetch permission for.
   *
   * @return {Array} a list of objects with the keys:
   *         - id: the permissionId of the permission
   *         - state: a constant representing the current permission state
   *           (e.g. SitePermissions.ALLOW)
   *         - scope: a constant representing how long the permission will
   *           be kept.
   */
  getAllForBrowser(browser) {
    let permissions = {};

    for (let permission of TemporaryBlockedPermissions.getAll(browser)) {
      permission.scope = this.SCOPE_TEMPORARY;
      permissions[permission.id] = permission;
    }

    for (let permission of this.getAllByURI(browser.currentURI)) {
      permissions[permission.id] = permission;
    }

    return Object.values(permissions);
  },

  /**
   * Returns a list of objects with detailed information on all permissions
   * that are currently set for the given browser.
   *
   * @param {Browser} browser
   *        The browser to fetch permission for.
   *
   * @return {Array<Object>} a list of objects with the keys:
   *           - id: the permissionID of the permission
   *           - state: a constant representing the current permission state
   *             (e.g. SitePermissions.ALLOW)
   *           - scope: a constant representing how long the permission will
   *             be kept.
   *           - label: the localized label
   */
  getAllPermissionDetailsForBrowser(browser) {
    return this.getAllForBrowser(browser).map(({id, scope, state}) =>
      ({id, scope, state, label: this.getPermissionLabel(id)}));
  },

  /**
   * Checks whether a UI for managing permissions should be exposed for a given
   * URI. This excludes file URIs, for instance, as they don't have a host,
   * even though nsIPermissionManager can still handle them.
   *
   * @param {nsIURI} uri
   *        The URI to check.
   *
   * @return {boolean} if the URI is supported.
   */
  isSupportedURI(uri) {
    return uri && ["http", "https", "moz-extension"].includes(uri.scheme);
  },

  /**
   * Gets an array of all permission IDs.
   *
   * @return {Array<String>} an array of all permission IDs.
   */
  listPermissions() {
    let permissions = Object.keys(gPermissionObject);

    // Hide canvas permission when privacy.resistFingerprinting is false.
    if (!Services.prefs.getBoolPref("privacy.resistFingerprinting")) {
      permissions = permissions.filter(permission => permission !== "canvas");
    }

    return permissions;
  },

  /**
   * Returns an array of permission states to be exposed to the user for a
   * permission with the given ID.
   *
   * @param {string} permissionID
   *        The ID to get permission states for.
   *
   * @return {Array<SitePermissions state>} an array of all permission states.
   */
  getAvailableStates(permissionID) {
    if (permissionID in gPermissionObject &&
        gPermissionObject[permissionID].states)
      return gPermissionObject[permissionID].states;

    /* Since the permissions we are dealing with have adopted the convention
     * of treating UNKNOWN == PROMPT, we only include one of either UNKNOWN
     * or PROMPT in this list, to avoid duplicating states. */
    if (this.getDefault(permissionID) == this.UNKNOWN)
      return [ SitePermissions.UNKNOWN, SitePermissions.ALLOW, SitePermissions.BLOCK ];

    return [ SitePermissions.PROMPT, SitePermissions.ALLOW, SitePermissions.BLOCK ];
  },

  /**
   * Returns the default state of a particular permission.
   *
   * @param {string} permissionID
   *        The ID to get the default for.
   *
   * @return {SitePermissions.state} the default state.
   */
  getDefault(permissionID) {
    // If the permission has custom logic for getting its default value,
    // try that first.
    if (permissionID in gPermissionObject &&
        gPermissionObject[permissionID].getDefault)
      return gPermissionObject[permissionID].getDefault();

    // Otherwise try to get the default preference for that permission.
    return this._defaultPrefBranch.getIntPref(permissionID, this.UNKNOWN);
  },

  /**
   * Returns the state and scope of a particular permission for a given URI.
   *
   * This method will NOT dispatch a "PermissionStateChange" event on the specified
   * browser if a temporary permission was removed because it has expired.
   *
   * @param {nsIURI} uri
   *        The URI to check.
   * @param {String} permissionID
   *        The id of the permission.
   * @param {Browser} browser (optional)
   *        The browser object to check for temporary permissions.
   *
   * @return {Object} an object with the keys:
   *           - state: The current state of the permission
   *             (e.g. SitePermissions.ALLOW)
   *           - scope: The scope of the permission
   *             (e.g. SitePermissions.SCOPE_PERSISTENT)
   */
  get(uri, permissionID, browser) {
    let defaultState = this.getDefault(permissionID);
    let result = { state: defaultState, scope: this.SCOPE_PERSISTENT };
    if (this.isSupportedURI(uri)) {
      let permission = null;
      if (permissionID in gPermissionObject &&
        gPermissionObject[permissionID].exactHostMatch) {
        permission = Services.perms.getPermissionObjectForURI(uri, permissionID, true);
      } else {
        permission = Services.perms.getPermissionObjectForURI(uri, permissionID, false);
      }

      if (permission) {
        result.state = permission.capability;
        if (permission.expireType == Services.perms.EXPIRE_SESSION) {
          result.scope = this.SCOPE_SESSION;
        } else if (permission.expireType == Services.perms.EXPIRE_POLICY) {
          result.scope = this.SCOPE_POLICY;
        }
      }
    }

    if (result.state == defaultState) {
      // If there's no persistent permission saved, check if we have something
      // set temporarily.
      let value = TemporaryBlockedPermissions.get(browser, permissionID);

      if (value) {
        result.state = value.state;
        result.scope = this.SCOPE_TEMPORARY;
      }
    }

    return result;
  },

  /**
   * Sets the state of a particular permission for a given URI or browser.
   * This method will dispatch a "PermissionStateChange" event on the specified
   * browser if a temporary permission was set
   *
   * @param {nsIURI} uri
   *        The URI to set the permission for.
   *        Note that this will be ignored if the scope is set to SCOPE_TEMPORARY
   * @param {String} permissionID
   *        The id of the permission.
   * @param {SitePermissions state} state
   *        The state of the permission.
   * @param {SitePermissions scope} scope (optional)
   *        The scope of the permission. Defaults to SCOPE_PERSISTENT.
   * @param {Browser} browser (optional)
   *        The browser object to set temporary permissions on.
   *        This needs to be provided if the scope is SCOPE_TEMPORARY!
   */
  set(uri, permissionID, state, scope = this.SCOPE_PERSISTENT, browser = null) {
    if (state == this.UNKNOWN || state == this.getDefault(permissionID)) {
      // Because they are controlled by two prefs with many states that do not
      // correspond to the classical ALLOW/DENY/PROMPT model, we want to always
      // allow the user to add exceptions to their cookie rules without removing them.
      if (permissionID != "cookie") {
        this.remove(uri, permissionID, browser);
        return;
      }
    }

    if (state == this.ALLOW_COOKIES_FOR_SESSION && permissionID != "cookie") {
      throw "ALLOW_COOKIES_FOR_SESSION can only be set on the cookie permission";
    }

    // Save temporary permissions.
    if (scope == this.SCOPE_TEMPORARY) {
      // We do not support setting temp ALLOW for security reasons.
      // In its current state, this permission could be exploited by subframes
      // on the same page. This is because for BLOCK we ignore the request
      // URI and only consider the current browser URI, to avoid notification spamming.
      //
      // If you ever consider removing this line, you likely want to implement
      // a more fine-grained TemporaryBlockedPermissions that temporarily blocks for the
      // entire browser, but temporarily allows only for specific frames.
      if (state != this.BLOCK) {
        throw "'Block' is the only permission we can save temporarily on a browser";
      }

      if (!browser) {
        throw "TEMPORARY scoped permissions require a browser object";
      }

      TemporaryBlockedPermissions.set(browser, permissionID);

      browser.dispatchEvent(new browser.ownerGlobal
                                       .CustomEvent("PermissionStateChange"));
    } else if (this.isSupportedURI(uri)) {
      let perms_scope = Services.perms.EXPIRE_NEVER;
      if (scope == this.SCOPE_SESSION) {
        perms_scope = Services.perms.EXPIRE_SESSION;
      } else if (scope == this.SCOPE_POLICY) {
        perms_scope = Services.perms.EXPIRE_POLICY;
      }

      Services.perms.add(uri, permissionID, state, perms_scope);
    }
  },

  /**
   * Removes the saved state of a particular permission for a given URI and/or browser.
   * This method will dispatch a "PermissionStateChange" event on the specified
   * browser if a temporary permission was removed.
   *
   * @param {nsIURI} uri
   *        The URI to remove the permission for.
   * @param {String} permissionID
   *        The id of the permission.
   * @param {Browser} browser (optional)
   *        The browser object to remove temporary permissions on.
   */
  remove(uri, permissionID, browser) {
    if (this.isSupportedURI(uri))
      Services.perms.remove(uri, permissionID);

    // TemporaryBlockedPermissions.get() deletes expired permissions automatically,
    if (TemporaryBlockedPermissions.get(browser, permissionID)) {
      // If it exists but has not expired, remove it explicitly.
      TemporaryBlockedPermissions.remove(browser, permissionID);
      // Send a PermissionStateChange event only if the permission hasn't expired.
      browser.dispatchEvent(new browser.ownerGlobal
                                       .CustomEvent("PermissionStateChange"));
    }
  },

  /**
   * Clears all permissions that were temporarily saved.
   *
   * @param {Browser} browser
   *        The browser object to clear.
   */
  clearTemporaryPermissions(browser) {
    TemporaryBlockedPermissions.clear(browser);
  },

  /**
   * Copy all permissions that were temporarily saved on one
   * browser object to a new browser.
   *
   * @param {Browser} browser
   *        The browser object to copy from.
   * @param {Browser} newBrowser
   *        The browser object to copy to.
   */
  copyTemporaryPermissions(browser, newBrowser) {
    TemporaryBlockedPermissions.copy(browser, newBrowser);
  },

  /**
   * Returns the localized label for the permission with the given ID, to be
   * used in a UI for managing permissions.
   *
   * @param {string} permissionID
   *        The permission to get the label for.
   *
   * @return {String} the localized label.
   */
  getPermissionLabel(permissionID) {
    let labelID = gPermissionObject[permissionID].labelID || permissionID;
    return gStringBundle.GetStringFromName("permission." + labelID + ".label");
  },

  /**
   * Returns the localized label for the given permission state, to be used in
   * a UI for managing permissions.
   *
   * @param {SitePermissions state} state
   *        The state to get the label for.
   *
   * @return {String|null} the localized label or null if an
   *         unknown state was passed.
   */
  getMultichoiceStateLabel(state) {
    switch (state) {
      case this.UNKNOWN:
      case this.PROMPT:
        return gStringBundle.GetStringFromName("state.multichoice.alwaysAsk");
      case this.ALLOW:
        return gStringBundle.GetStringFromName("state.multichoice.allow");
      case this.ALLOW_COOKIES_FOR_SESSION:
        return gStringBundle.GetStringFromName("state.multichoice.allowForSession");
      case this.BLOCK:
        return gStringBundle.GetStringFromName("state.multichoice.block");
      default:
        return null;
    }
  },

  /**
   * Returns the localized label for a permission's current state.
   *
   * @param {SitePermissions state} state
   *        The state to get the label for.
   * @param {string} id
   *        The permission to get the state label for.
   * @param {SitePermissions scope} scope (optional)
   *        The scope to get the label for.
   *
   * @return {String|null} the localized label or null if an
   *         unknown state was passed.
   */
  getCurrentStateLabel(state, id, scope = null) {
    // We try to avoid a collision between SitePermissions.PROMPT_HIDE
    // and SitePermissions.ALLOW_COOKIES_FOR_SESSION which share the same const value.
    if (id.startsWith("plugin") && state == SitePermissions.PROMPT_HIDE) {
      return gStringBundle.GetStringFromName("state.current.hide");
    }

    switch (state) {
      case this.PROMPT:
        return gStringBundle.GetStringFromName("state.current.prompt");
      case this.ALLOW:
        if (scope && scope != this.SCOPE_PERSISTENT && scope != this.SCOPE_POLICY)
          return gStringBundle.GetStringFromName("state.current.allowedTemporarily");
        return gStringBundle.GetStringFromName("state.current.allowed");
      case this.ALLOW_COOKIES_FOR_SESSION:
        return gStringBundle.GetStringFromName("state.current.allowedForSession");
      case this.BLOCK:
        if (scope && scope != this.SCOPE_PERSISTENT && scope != this.SCOPE_POLICY)
          return gStringBundle.GetStringFromName("state.current.blockedTemporarily");
        return gStringBundle.GetStringFromName("state.current.blocked");
      default:
        return null;
    }
  },
};

var gPermissionObject = {
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
   *  - labelID
   *    Use the given ID instead of the permission name for looking up strings.
   *    e.g. "desktop-notification2" to use permission.desktop-notification2.label
   *
   *  - states
   *    Array of permission states to be exposed to the user.
   *    Defaults to ALLOW, BLOCK and the default state (see getDefault).
   *    The PROMPT_HIDE state is deliberately excluded from "plugin:flash" since we
   *    don't want to expose a "Hide Prompt" button to the user through pageinfo.
   */

  "autoplay-media": {
    exactHostMatch: true,
    getDefault() {
      let state = Services.prefs.getIntPref("media.autoplay.default",
                                            Ci.nsIAutoplay.PROMPT);
      if (state == Ci.nsIAutoplay.ALLOW) {
        return SitePermissions.ALLOW;
      } if (state == Ci.nsIAutoplay.BLOCK) {
        return SitePermissions.DENY;
      }
      return SitePermissions.UNKNOWN;
    },
    labelID: "autoplay-media"
  },

  "image": {
    states: [ SitePermissions.ALLOW, SitePermissions.BLOCK ],
  },

  "cookie": {
    states: [ SitePermissions.ALLOW, SitePermissions.ALLOW_COOKIES_FOR_SESSION, SitePermissions.BLOCK ],
    getDefault() {
      if (Services.prefs.getIntPref("network.cookie.cookieBehavior") == Ci.nsICookieService.BEHAVIOR_REJECT)
        return SitePermissions.BLOCK;

      if (Services.prefs.getIntPref("network.cookie.lifetimePolicy") == Ci.nsICookieService.ACCEPT_SESSION)
        return SitePermissions.ALLOW_COOKIES_FOR_SESSION;

      return SitePermissions.ALLOW;
    }
  },

  "desktop-notification": {
    exactHostMatch: true,
    labelID: "desktop-notification2",
  },

  "camera": {
    exactHostMatch: true,
  },

  "microphone": {
    exactHostMatch: true,
  },

  "screen": {
    exactHostMatch: true,
    states: [ SitePermissions.UNKNOWN, SitePermissions.BLOCK ],
  },

  "popup": {
    getDefault() {
      return Services.prefs.getBoolPref("dom.disable_open_during_load") ?
               SitePermissions.BLOCK : SitePermissions.ALLOW;
    },
    states: [ SitePermissions.ALLOW, SitePermissions.BLOCK ],
  },

  "install": {
    getDefault() {
      return Services.prefs.getBoolPref("xpinstall.whitelist.required") ?
               SitePermissions.BLOCK : SitePermissions.ALLOW;
    },
    states: [ SitePermissions.ALLOW, SitePermissions.BLOCK ],
  },

  "geo": {
    exactHostMatch: true
  },

  "focus-tab-by-prompt": {
    exactHostMatch: true,
    states: [ SitePermissions.UNKNOWN, SitePermissions.ALLOW ],
  },
  "persistent-storage": {
    exactHostMatch: true
  },

  "shortcuts": {
    states: [ SitePermissions.ALLOW, SitePermissions.BLOCK ],
  },

  "canvas": {
  },

  "plugin:flash": {
    labelID: "flash-plugin",
    states: [ SitePermissions.UNKNOWN, SitePermissions.ALLOW, SitePermissions.BLOCK ],
  },

  "midi": {
    exactHostMatch: true
  },

  "midi-sysex": {
    exactHostMatch: true
  }
};

if (!Services.prefs.getBoolPref("dom.webmidi.enabled")) {
  // ESLint gets angry about array versus dot notation here, but some permission
  // names use hyphens. Disabling rule for line to keep things consistent.
  // eslint-disable-next-line dot-notation
  delete gPermissionObject["midi"];
  delete gPermissionObject["midi-sysex"];
}

XPCOMUtils.defineLazyPreferenceGetter(SitePermissions, "temporaryPermissionExpireTime",
                                      "privacy.temporary_permission_expire_time_ms", 3600 * 1000);
