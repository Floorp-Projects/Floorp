/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["SitePermissions"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

var gStringBundle = Services.strings.createBundle(
  "chrome://browser/locale/sitePermissions.properties"
);

/**
 * A helper module to manage temporary permissions.
 *
 * Permissions are keyed by browser, so methods take a Browser
 * element to identify the corresponding permission set.
 *
 * This uses a WeakMap to key browsers, so that entries are
 * automatically cleared once the browser stops existing
 * (once there are no other references to the browser object);
 */
const TemporaryPermissions = {
  // This is a three level deep map with the following structure:
  //
  // Browser => {
  //   <baseDomain>: {
  //     <permissionID>: {Number} <timeStamp>
  //   }
  // }
  //
  // Only the top level browser elements are stored via WeakMap. The WeakMap
  // value is an object with URI baseDomains as keys. The keys of that object
  // are ids that identify permissions that were set for the specific URI.
  // The final value is an object containing the timestamp of when the permission
  // was set (in order to invalidate after a certain amount of time has passed).
  _stateByBrowser: new WeakMap(),

  // Private helper method that bundles some shared behavior for
  // get() and getAll(), e.g. deleting permissions when they have expired.
  _get(entry, baseDomain, id, permission) {
    if (permission == null || permission.timeStamp == null) {
      delete entry[baseDomain][id];
      return null;
    }
    if (
      permission.timeStamp + SitePermissions.temporaryPermissionExpireTime <
      Date.now()
    ) {
      delete entry[baseDomain][id];
      return null;
    }
    return {
      id,
      state: permission.state,
      scope: SitePermissions.SCOPE_TEMPORARY,
    };
  },

  // Extract baseDomain from uri. Fallback to hostname on conversion error.
  _uriToBaseDomain(uri) {
    try {
      return Services.eTLD.getBaseDomain(uri);
    } catch (error) {
      if (
        error.result !== Cr.NS_ERROR_HOST_IS_IP_ADDRESS &&
        error.result !== Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS
      ) {
        throw error;
      }
      return uri.host;
    }
  },

  // Sets a new permission for the specified browser.
  set(browser, id, state) {
    if (!browser) {
      return;
    }
    if (!this._stateByBrowser.has(browser)) {
      this._stateByBrowser.set(browser, {});
    }
    let entry = this._stateByBrowser.get(browser);
    let baseDomain = this._uriToBaseDomain(browser.currentURI);
    if (!entry[baseDomain]) {
      entry[baseDomain] = {};
    }
    entry[baseDomain][id] = { timeStamp: Date.now(), state };
  },

  // Removes a permission with the specified id for the specified browser.
  remove(browser, id) {
    if (!browser || !this._stateByBrowser.has(browser)) {
      return;
    }
    let entry = this._stateByBrowser.get(browser);
    let baseDomain = this._uriToBaseDomain(browser.currentURI);
    if (entry[baseDomain]) {
      delete entry[baseDomain][id];
    }
  },

  // Gets a permission with the specified id for the specified browser.
  get(browser, id) {
    if (!browser || !browser.currentURI || !this._stateByBrowser.has(browser)) {
      return null;
    }
    let entry = this._stateByBrowser.get(browser);
    let baseDomain = this._uriToBaseDomain(browser.currentURI);
    if (entry[baseDomain]) {
      let permission = entry[baseDomain][id];
      return this._get(entry, baseDomain, id, permission);
    }
    return null;
  },

  // Gets all permissions for the specified browser.
  // Note that only permissions that apply to the current URI
  // of the passed browser element will be returned.
  getAll(browser) {
    let permissions = [];
    if (!this._stateByBrowser.has(browser)) {
      return permissions;
    }
    let entry = this._stateByBrowser.get(browser);
    let baseDomain = this._uriToBaseDomain(browser.currentURI);
    if (entry[baseDomain]) {
      let timeStamps = entry[baseDomain];
      for (let id of Object.keys(timeStamps)) {
        let permission = this._get(entry, baseDomain, id, timeStamps[id]);
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

// This hold a flag per browser to indicate whether we should show the
// user a notification as a permission has been requested that has been
// blocked globally. We only want to notify the user in the case that
// they actually requested the permission within the current page load
// so will clear the flag on navigation.
const GloballyBlockedPermissions = {
  _stateByBrowser: new WeakMap(),

  set(browser, id) {
    if (!this._stateByBrowser.has(browser)) {
      this._stateByBrowser.set(browser, {});
    }
    let entry = this._stateByBrowser.get(browser);
    let prePath = browser.currentURI.prePath;
    if (!entry[prePath]) {
      entry[prePath] = {};
    }

    if (entry[prePath][id]) {
      return;
    }
    entry[prePath][id] = true;

    // Clear the flag and remove the listener once the user has navigated.
    // WebProgress will report various things including hashchanges to us, the
    // navigation we care about is either leaving the current page or reloading.
    browser.addProgressListener(
      {
        QueryInterface: ChromeUtils.generateQI([
          Ci.nsIWebProgressListener,
          Ci.nsISupportsWeakReference,
        ]),
        onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
          let hasLeftPage =
            aLocation.prePath != prePath ||
            !(aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT);
          let isReload = !!(
            aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_RELOAD
          );

          if (aWebProgress.isTopLevel && (hasLeftPage || isReload)) {
            GloballyBlockedPermissions.remove(browser, id, prePath);
            browser.removeProgressListener(this);
          }
        },
      },
      Ci.nsIWebProgress.NOTIFY_LOCATION
    );
  },

  // Removes a permission with the specified id for the specified browser.
  remove(browser, id, prePath = null) {
    let entry = this._stateByBrowser.get(browser);
    if (!prePath) {
      prePath = browser.currentURI.prePath;
    }
    if (entry && entry[prePath]) {
      delete entry[prePath][id];
    }
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
        permissions.push({
          id,
          state: gPermissionObject[id].getDefault(),
          scope: SitePermissions.SCOPE_GLOBAL,
        });
      }
    }
    return permissions;
  },

  // Copies the globally blocked permission state of one browser
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
  UNKNOWN: Services.perms.UNKNOWN_ACTION,
  ALLOW: Services.perms.ALLOW_ACTION,
  BLOCK: Services.perms.DENY_ACTION,
  PROMPT: Services.perms.PROMPT_ACTION,
  ALLOW_COOKIES_FOR_SESSION: Ci.nsICookiePermission.ACCESS_SESSION,
  AUTOPLAY_BLOCKED_ALL: Ci.nsIAutoplay.BLOCKED_ALL,

  // Permission scopes.
  SCOPE_REQUEST: "{SitePermissions.SCOPE_REQUEST}",
  SCOPE_TEMPORARY: "{SitePermissions.SCOPE_TEMPORARY}",
  SCOPE_SESSION: "{SitePermissions.SCOPE_SESSION}",
  SCOPE_PERSISTENT: "{SitePermissions.SCOPE_PERSISTENT}",
  SCOPE_POLICY: "{SitePermissions.SCOPE_POLICY}",
  SCOPE_GLOBAL: "{SitePermissions.SCOPE_GLOBAL}",

  _permissionsArray: null,
  _defaultPrefBranch: Services.prefs.getBranch("permissions.default."),

  /**
   * Gets all custom permissions for a given principal.
   * Install addon permission is excluded, check bug 1303108.
   *
   * @return {Array} a list of objects with the keys:
   *          - id: the permissionId of the permission
   *          - scope: the scope of the permission (e.g. SitePermissions.SCOPE_TEMPORARY)
   *          - state: a constant representing the current permission state
   *            (e.g. SitePermissions.ALLOW)
   */
  getAllByPrincipal(principal) {
    let result = [];
    if (!principal) {
      throw new Error("principal argument cannot be null.");
    }
    if (!this.isSupportedPrincipal(principal)) {
      return result;
    }

    let permissions = Services.perms.getAllForPrincipal(principal);
    for (let permission of permissions) {
      // filter out unknown permissions
      if (gPermissionObject[permission.type]) {
        // Hide canvas permission when privacy.resistFingerprinting is false.
        if (permission.type == "canvas" && !this.resistFingerprinting) {
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

    for (let permission of TemporaryPermissions.getAll(browser)) {
      permission.scope = this.SCOPE_TEMPORARY;
      permissions[permission.id] = permission;
    }

    for (let permission of GloballyBlockedPermissions.getAll(browser)) {
      permissions[permission.id] = permission;
    }

    for (let permission of this.getAllByPrincipal(browser.contentPrincipal)) {
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
   *           - label: the localized label, or null if none is available.
   */
  getAllPermissionDetailsForBrowser(browser) {
    return this.getAllForBrowser(browser).map(({ id, scope, state }) => ({
      id,
      scope,
      state,
      label: this.getPermissionLabel(id),
    }));
  },

  /**
   * Checks whether a UI for managing permissions should be exposed for a given
   * principal. This excludes file URIs, for instance, as they don't have a host,
   * even though nsIPermissionManager can still handle them.
   *
   * @param {nsIPrincipal} principal
   *        The principal to check.
   *
   * @return {boolean} if the principal is supported.
   */
  isSupportedPrincipal(principal) {
    if (!principal) {
      return false;
    }
    if (!(principal instanceof Ci.nsIPrincipal)) {
      throw new Error(
        "Argument passed as principal is not an instance of Ci.nsIPrincipal"
      );
    }
    return ["http", "https", "moz-extension"].some(scheme =>
      principal.schemeIs(scheme)
    );
  },

  /**
   * Gets an array of all permission IDs.
   *
   * @return {Array<String>} an array of all permission IDs.
   */
  listPermissions() {
    if (this._permissionsArray === null) {
      let permissions = Object.keys(gPermissionObject);

      // Hide canvas permission when privacy.resistFingerprinting is false.
      if (!this.resistFingerprinting) {
        permissions = permissions.filter(permission => permission !== "canvas");
      }
      this._permissionsArray = permissions;
    }

    return this._permissionsArray;
  },

  /**
   * Called when the privacy.resistFingerprinting preference changes its value.
   *
   * @param {string} data
   *        The last argument passed to the preference change observer
   * @param {string} previous
   *        The previous value of the preference
   * @param {string} latest
   *        The latest value of the preference
   */
  onResistFingerprintingChanged(data, previous, latest) {
    // Ensure that listPermissions() will reconstruct its return value the next
    // time it's called.
    this._permissionsArray = null;
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
    if (
      permissionID in gPermissionObject &&
      gPermissionObject[permissionID].states
    ) {
      return gPermissionObject[permissionID].states;
    }

    /* Since the permissions we are dealing with have adopted the convention
     * of treating UNKNOWN == PROMPT, we only include one of either UNKNOWN
     * or PROMPT in this list, to avoid duplicating states. */
    if (this.getDefault(permissionID) == this.UNKNOWN) {
      return [
        SitePermissions.UNKNOWN,
        SitePermissions.ALLOW,
        SitePermissions.BLOCK,
      ];
    }

    return [
      SitePermissions.PROMPT,
      SitePermissions.ALLOW,
      SitePermissions.BLOCK,
    ];
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
    if (
      permissionID in gPermissionObject &&
      gPermissionObject[permissionID].getDefault
    ) {
      return gPermissionObject[permissionID].getDefault();
    }

    // Otherwise try to get the default preference for that permission.
    return this._defaultPrefBranch.getIntPref(permissionID, this.UNKNOWN);
  },

  /**
   * Set the default state of a particular permission.
   *
   * @param {string} permissionID
   *        The ID to set the default for.
   *
   * @param {string} state
   *        The state to set.
   */
  setDefault(permissionID, state) {
    if (
      permissionID in gPermissionObject &&
      gPermissionObject[permissionID].setDefault
    ) {
      return gPermissionObject[permissionID].setDefault(state);
    }
    let key = "permissions.default." + permissionID;
    return Services.prefs.setIntPref(key, state);
  },

  /**
   * Returns the state and scope of a particular permission for a given principal.
   *
   * This method will NOT dispatch a "PermissionStateChange" event on the specified
   * browser if a temporary permission was removed because it has expired.
   *
   * @param {nsIPrincipal} principal
   *        The principal to check.
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
  getForPrincipal(principal, permissionID, browser) {
    if (!principal && !browser) {
      throw new Error(
        "Atleast one of the arguments, either principal or browser should not be null."
      );
    }
    let defaultState = this.getDefault(permissionID);
    let result = { state: defaultState, scope: this.SCOPE_PERSISTENT };
    if (this.isSupportedPrincipal(principal)) {
      let permission = null;
      if (
        permissionID in gPermissionObject &&
        gPermissionObject[permissionID].exactHostMatch
      ) {
        permission = Services.perms.getPermissionObject(
          principal,
          permissionID,
          true
        );
      } else {
        permission = Services.perms.getPermissionObject(
          principal,
          permissionID,
          false
        );
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
      let value = TemporaryPermissions.get(browser, permissionID);

      if (value) {
        result.state = value.state;
        result.scope = this.SCOPE_TEMPORARY;
      }
    }

    return result;
  },

  /**
   * Sets the state of a particular permission for a given principal or browser.
   * This method will dispatch a "PermissionStateChange" event on the specified
   * browser if a temporary permission was set
   *
   * @param {nsIPrincipal} principal
   *        The principal to set the permission for.
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
  setForPrincipal(
    principal,
    permissionID,
    state,
    scope = this.SCOPE_PERSISTENT,
    browser = null
  ) {
    if (!principal && !browser) {
      throw new Error(
        "Atleast one of the arguments, either principal or browser should not be null."
      );
    }
    if (scope == this.SCOPE_GLOBAL && state == this.BLOCK) {
      GloballyBlockedPermissions.set(browser, permissionID);
      browser.dispatchEvent(
        new browser.ownerGlobal.CustomEvent("PermissionStateChange")
      );
      return;
    }

    if (state == this.UNKNOWN || state == this.getDefault(permissionID)) {
      // Because they are controlled by two prefs with many states that do not
      // correspond to the classical ALLOW/DENY/PROMPT model, we want to always
      // allow the user to add exceptions to their cookie rules without removing them.
      if (permissionID != "cookie") {
        this.removeFromPrincipal(principal, permissionID, browser);
        return;
      }
    }

    if (state == this.ALLOW_COOKIES_FOR_SESSION && permissionID != "cookie") {
      throw new Error(
        "ALLOW_COOKIES_FOR_SESSION can only be set on the cookie permission"
      );
    }

    // Save temporary permissions.
    if (scope == this.SCOPE_TEMPORARY) {
      // We do not support setting temp ALLOW for security reasons.
      // In its current state, this permission could be exploited by subframes
      // on the same page. This is because for BLOCK we ignore the request
      // principal and only consider the current browser principal, to avoid notification spamming.
      //
      // If you ever consider removing this line, you likely want to implement
      // a more fine-grained TemporaryPermissions that temporarily blocks for the
      // entire browser, but temporarily allows only for specific frames.
      if (state != this.BLOCK) {
        throw new Error(
          "'Block' is the only permission we can save temporarily on a browser"
        );
      }

      if (!browser) {
        throw new Error(
          "TEMPORARY scoped permissions require a browser object"
        );
      }

      TemporaryPermissions.set(browser, permissionID, state);

      browser.dispatchEvent(
        new browser.ownerGlobal.CustomEvent("PermissionStateChange")
      );
    } else if (this.isSupportedPrincipal(principal)) {
      let perms_scope = Services.perms.EXPIRE_NEVER;
      if (scope == this.SCOPE_SESSION) {
        perms_scope = Services.perms.EXPIRE_SESSION;
      } else if (scope == this.SCOPE_POLICY) {
        perms_scope = Services.perms.EXPIRE_POLICY;
      }

      Services.perms.addFromPrincipal(
        principal,
        permissionID,
        state,
        perms_scope
      );
    }
  },

  /**
   * Removes the saved state of a particular permission for a given principal and/or browser.
   * This method will dispatch a "PermissionStateChange" event on the specified
   * browser if a temporary permission was removed.
   *
   * @param {nsIPrincipal} principal
   *        The principal to remove the permission for.
   * @param {String} permissionID
   *        The id of the permission.
   * @param {Browser} browser (optional)
   *        The browser object to remove temporary permissions on.
   */
  removeFromPrincipal(principal, permissionID, browser) {
    if (!principal && !browser) {
      throw new Error(
        "Atleast one of the arguments, either principal or browser should not be null."
      );
    }
    if (this.isSupportedPrincipal(principal)) {
      Services.perms.removeFromPrincipal(principal, permissionID);
    }

    // TemporaryPermissions.get() deletes expired permissions automatically,
    if (TemporaryPermissions.get(browser, permissionID)) {
      // If it exists but has not expired, remove it explicitly.
      TemporaryPermissions.remove(browser, permissionID);
      // Send a PermissionStateChange event only if the permission hasn't expired.
      browser.dispatchEvent(
        new browser.ownerGlobal.CustomEvent("PermissionStateChange")
      );
    }
  },

  /**
   * Clears all permissions that were temporarily saved.
   *
   * @param {Browser} browser
   *        The browser object to clear.
   */
  clearTemporaryPermissions(browser) {
    TemporaryPermissions.clear(browser);
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
    TemporaryPermissions.copy(browser, newBrowser);
    GloballyBlockedPermissions.copy(browser, newBrowser);
  },

  /**
   * Returns the localized label for the permission with the given ID, to be
   * used in a UI for managing permissions.
   *
   * @param {string} permissionID
   *        The permission to get the label for.
   *
   * @return {String} the localized label or null if none is available.
   */
  getPermissionLabel(permissionID) {
    if (!(permissionID in gPermissionObject)) {
      // Permission can't be found.
      return null;
    }
    if (
      "labelID" in gPermissionObject[permissionID] &&
      gPermissionObject[permissionID].labelID === null
    ) {
      // Permission doesn't support having a label.
      return null;
    }
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
  getMultichoiceStateLabel(permissionID, state) {
    // If the permission has custom logic for getting its default value,
    // try that first.
    if (
      permissionID in gPermissionObject &&
      gPermissionObject[permissionID].getMultichoiceStateLabel
    ) {
      return gPermissionObject[permissionID].getMultichoiceStateLabel(state);
    }

    switch (state) {
      case this.UNKNOWN:
      case this.PROMPT:
        return gStringBundle.GetStringFromName("state.multichoice.alwaysAsk");
      case this.ALLOW:
        return gStringBundle.GetStringFromName("state.multichoice.allow");
      case this.ALLOW_COOKIES_FOR_SESSION:
        return gStringBundle.GetStringFromName(
          "state.multichoice.allowForSession"
        );
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
    switch (state) {
      case this.PROMPT:
        return gStringBundle.GetStringFromName("state.current.prompt");
      case this.ALLOW:
        if (
          scope &&
          scope != this.SCOPE_PERSISTENT &&
          scope != this.SCOPE_POLICY
        ) {
          return gStringBundle.GetStringFromName(
            "state.current.allowedTemporarily"
          );
        }
        return gStringBundle.GetStringFromName("state.current.allowed");
      case this.ALLOW_COOKIES_FOR_SESSION:
        return gStringBundle.GetStringFromName(
          "state.current.allowedForSession"
        );
      case this.BLOCK:
        if (
          scope &&
          scope != this.SCOPE_PERSISTENT &&
          scope != this.SCOPE_POLICY &&
          scope != this.SCOPE_GLOBAL
        ) {
          return gStringBundle.GetStringFromName(
            "state.current.blockedTemporarily"
          );
        }
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
   */

  "autoplay-media": {
    exactHostMatch: true,
    getDefault() {
      let pref = Services.prefs.getIntPref(
        "media.autoplay.default",
        Ci.nsIAutoplay.BLOCKED
      );
      if (pref == Ci.nsIAutoplay.ALLOWED) {
        return SitePermissions.ALLOW;
      }
      if (pref == Ci.nsIAutoplay.BLOCKED_ALL) {
        return SitePermissions.AUTOPLAY_BLOCKED_ALL;
      }
      return SitePermissions.BLOCK;
    },
    setDefault(value) {
      let prefValue = Ci.nsIAutoplay.BLOCKED;
      if (value == SitePermissions.ALLOW) {
        prefValue = Ci.nsIAutoplay.ALLOWED;
      } else if (value == SitePermissions.AUTOPLAY_BLOCKED_ALL) {
        prefValue = Ci.nsIAutoplay.BLOCKED_ALL;
      }
      Services.prefs.setIntPref("media.autoplay.default", prefValue);
    },
    labelID: "autoplay",
    states: [
      SitePermissions.ALLOW,
      SitePermissions.BLOCK,
      SitePermissions.AUTOPLAY_BLOCKED_ALL,
    ],
    getMultichoiceStateLabel(state) {
      switch (state) {
        case SitePermissions.AUTOPLAY_BLOCKED_ALL:
          return gStringBundle.GetStringFromName(
            "state.multichoice.autoplayblockall"
          );
        case SitePermissions.BLOCK:
          return gStringBundle.GetStringFromName(
            "state.multichoice.autoplayblock"
          );
        case SitePermissions.ALLOW:
          return gStringBundle.GetStringFromName(
            "state.multichoice.autoplayallow"
          );
      }
      throw new Error(`Unkown state: ${state}`);
    },
  },

  cookie: {
    states: [
      SitePermissions.ALLOW,
      SitePermissions.ALLOW_COOKIES_FOR_SESSION,
      SitePermissions.BLOCK,
    ],
    getDefault() {
      if (
        Services.prefs.getIntPref("network.cookie.cookieBehavior") ==
        Ci.nsICookieService.BEHAVIOR_REJECT
      ) {
        return SitePermissions.BLOCK;
      }

      if (
        Services.prefs.getIntPref("network.cookie.lifetimePolicy") ==
        Ci.nsICookieService.ACCEPT_SESSION
      ) {
        return SitePermissions.ALLOW_COOKIES_FOR_SESSION;
      }

      return SitePermissions.ALLOW;
    },
  },

  "desktop-notification": {
    exactHostMatch: true,
    labelID: "desktop-notification3",
  },

  camera: {
    exactHostMatch: true,
  },

  microphone: {
    exactHostMatch: true,
  },

  screen: {
    exactHostMatch: true,
    states: [SitePermissions.UNKNOWN, SitePermissions.BLOCK],
  },

  popup: {
    getDefault() {
      return Services.prefs.getBoolPref("dom.disable_open_during_load")
        ? SitePermissions.BLOCK
        : SitePermissions.ALLOW;
    },
    states: [SitePermissions.ALLOW, SitePermissions.BLOCK],
  },

  install: {
    getDefault() {
      return Services.prefs.getBoolPref("xpinstall.whitelist.required")
        ? SitePermissions.UNKNOWN
        : SitePermissions.ALLOW;
    },
  },

  geo: {
    exactHostMatch: true,
  },

  xr: {
    exactHostMatch: true,
  },

  "focus-tab-by-prompt": {
    exactHostMatch: true,
    states: [SitePermissions.UNKNOWN, SitePermissions.ALLOW],
  },
  "persistent-storage": {
    exactHostMatch: true,
  },

  shortcuts: {
    states: [SitePermissions.ALLOW, SitePermissions.BLOCK],
  },

  canvas: {},

  midi: {
    exactHostMatch: true,
  },

  "midi-sysex": {
    exactHostMatch: true,
  },

  "storage-access": {
    labelID: null,
    getDefault() {
      return SitePermissions.UNKNOWN;
    },
  },
};

if (!Services.prefs.getBoolPref("dom.webmidi.enabled")) {
  // ESLint gets angry about array versus dot notation here, but some permission
  // names use hyphens. Disabling rule for line to keep things consistent.
  // eslint-disable-next-line dot-notation
  delete gPermissionObject["midi"];
  delete gPermissionObject["midi-sysex"];
}

XPCOMUtils.defineLazyPreferenceGetter(
  SitePermissions,
  "temporaryPermissionExpireTime",
  "privacy.temporary_permission_expire_time_ms",
  3600 * 1000
);
XPCOMUtils.defineLazyPreferenceGetter(
  SitePermissions,
  "resistFingerprinting",
  "privacy.resistFingerprinting",
  false,
  SitePermissions.onResistFingerprintingChanged.bind(SitePermissions)
);
