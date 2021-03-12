/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["SitePermissions"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  clearTimeout: "resource://gre/modules/Timer.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

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
  //   <baseDomain|prePath>: {
  //     <permissionID>: {state: Number, expireTimeout: Number}
  //   }
  // }
  //
  // Only the top level browser elements are stored via WeakMap. The WeakMap
  // value is an object with URI baseDomains or prePaths as keys. The keys of
  // that object are ids that identify permissions that were set for the
  // specific URI. The final value is an object containing the permission state
  // and the id of the timeout which will cause permission expiry.
  // BLOCK permissions are keyed under baseDomain to prevent bypassing the block
  // (see Bug 1492668). Any other permissions are keyed under URI prePath.
  _stateByBrowser: new WeakMap(),

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

  /**
   * Generate keys to store temporary permissions under. The strict key is
   * nsIURI.prePath, non-strict is URI baseDomain.
   * @param {nsIURI} uri - URI to derive keys from.
   * @returns {Object} keys - Object containing the generated permission keys.
   * @returns {string} keys.strict - Key to be used for strict matching.
   * @returns {string} keys.nonStrict - Key to be used for non-strict matching.
   * @throws {Error} - Throws if URI is undefined or no valid permission key can
   * be generated.
   */
  _getKeysFromURI(uri) {
    return { strict: uri.prePath, nonStrict: this._uriToBaseDomain(uri) };
  },

  // Sets a new permission for the specified browser.
  set(browser, id, state, expireTimeMS, browserURI, expireCallback) {
    if (
      !browser ||
      !browserURI ||
      !SitePermissions.isSupportedScheme(browserURI.scheme)
    ) {
      return;
    }
    let entry = this._stateByBrowser.get(browser);
    if (!entry) {
      entry = { browser: Cu.getWeakReference(browser), uriToPerm: {} };
      this._stateByBrowser.set(browser, entry);
    }
    let { uriToPerm } = entry;
    // We store blocked permissions by baseDomain. Other states by URI prePath.
    let { strict, nonStrict } = this._getKeysFromURI(browserURI);
    let setKey;
    let deleteKey;
    // Differenciate between block and non-block permissions. If we store a
    // block permission we need to delete old entries which may be set under URI
    // prePath before setting the new permission for baseDomain. For non-block
    // permissions this is swapped.
    if (state == SitePermissions.BLOCK) {
      setKey = nonStrict;
      deleteKey = strict;
    } else {
      setKey = strict;
      deleteKey = nonStrict;
    }

    if (!uriToPerm[setKey]) {
      uriToPerm[setKey] = {};
    }

    let expireTimeout = uriToPerm[setKey][id]?.expireTimeout;
    // If overwriting a permission state. We need to cancel the old timeout.
    if (expireTimeout) {
      clearTimeout(expireTimeout);
    }
    // Construct the new timeout to remove the permission once it has expired.
    expireTimeout = setTimeout(() => {
      let entryBrowser = entry.browser.get();
      // Exit early if the browser is no longer alive when we get the timeout
      // callback.
      if (!entryBrowser || !uriToPerm[setKey]) {
        return;
      }
      delete uriToPerm[setKey][id];
      // Notify SitePermissions that a temporary permission has expired.
      // Get the browser the permission is currently set for. If this.copy was
      // used this browser is different from the original one passed above.
      expireCallback(entryBrowser);
    }, expireTimeMS);
    uriToPerm[setKey][id] = {
      expireTimeout,
      state,
    };

    // If we set a permission state for a prePath we need to reset the old state
    // which may be set for baseDomain and vice versa. An individual permission
    // must only ever be keyed by either prePath or baseDomain.
    let permissions = uriToPerm[deleteKey];
    if (!permissions) {
      return;
    }
    expireTimeout = permissions[id]?.expireTimeout;
    if (expireTimeout) {
      clearTimeout(expireTimeout);
    }
    delete permissions[id];
  },

  // Removes a permission with the specified id for the specified browser.
  remove(browser, id) {
    if (
      !browser ||
      !SitePermissions.isSupportedScheme(browser.currentURI.scheme) ||
      !this._stateByBrowser.has(browser)
    ) {
      return;
    }
    // Permission can be stored by any of the two keys (strict and non-strict).
    // getKeysFromURI can throw. We let the caller handle the exception.
    let { strict, nonStrict } = this._getKeysFromURI(browser.currentURI);
    let { uriToPerm } = this._stateByBrowser.get(browser);
    for (let key of [nonStrict, strict]) {
      if (uriToPerm[key]?.[id] != null) {
        let { expireTimeout } = uriToPerm[key][id];
        if (expireTimeout) {
          clearTimeout(expireTimeout);
        }
        delete uriToPerm[key][id];
        // Individual permissions can only ever be keyed either strict or
        // non-strict. If we find the permission via the first key run we can
        // return early.
        return;
      }
    }
  },

  // Gets a permission with the specified id for the specified browser.
  get(browser, id) {
    if (
      !browser ||
      !browser.currentURI ||
      !SitePermissions.isSupportedScheme(browser.currentURI.scheme) ||
      !this._stateByBrowser.has(browser)
    ) {
      return null;
    }
    let { uriToPerm } = this._stateByBrowser.get(browser);

    let { strict, nonStrict } = this._getKeysFromURI(browser.currentURI);
    for (let key of [nonStrict, strict]) {
      if (uriToPerm[key]) {
        let permission = uriToPerm[key][id];
        if (permission) {
          return {
            id,
            state: permission.state,
            scope: SitePermissions.SCOPE_TEMPORARY,
          };
        }
      }
    }
    return null;
  },

  // Gets all permissions for the specified browser.
  // Note that only permissions that apply to the current URI
  // of the passed browser element will be returned.
  getAll(browser) {
    let permissions = [];
    if (
      !SitePermissions.isSupportedScheme(browser.currentURI.scheme) ||
      !this._stateByBrowser.has(browser)
    ) {
      return permissions;
    }
    let { uriToPerm } = this._stateByBrowser.get(browser);

    let { strict, nonStrict } = this._getKeysFromURI(browser.currentURI);
    for (let key of [nonStrict, strict]) {
      if (uriToPerm[key]) {
        let perms = uriToPerm[key];
        for (let id of Object.keys(perms)) {
          let permission = perms[id];
          if (permission) {
            permissions.push({
              id,
              state: permission.state,
              scope: SitePermissions.SCOPE_TEMPORARY,
            });
          }
        }
      }
    }

    return permissions;
  },

  // Clears all permissions for the specified browser.
  // Unlike other methods, this does NOT clear only for
  // the currentURI but the whole browser state.

  /**
   * Clear temporary permissions for the specified browser. Unlike other
   * methods, this does NOT clear only for the currentURI but the whole browser
   * state.
   * @param {Browser} browser - Browser to clear permissions for.
   * @param {Number} [filterState] - Only clear permissions with the given state
   * value. Defaults to all permissions.
   */
  clear(browser, filterState = null) {
    let entry = this._stateByBrowser.get(browser);
    if (!entry?.uriToPerm) {
      return;
    }

    let { uriToPerm } = entry;
    Object.entries(uriToPerm).forEach(([uriKey, permissions]) => {
      Object.entries(permissions).forEach(
        ([permId, { state, expireTimeout }]) => {
          // We need to explicitly check for null or undefined here, because the
          // permission state may be 0.
          if (filterState != null) {
            if (state != filterState) {
              // Skip permission entry if it doesn't match the filter.
              return;
            }
            delete permissions[permId];
          }
          // For the clear-all case we remove the entire browser entry, so we
          // only need to clear the timeouts.
          if (!expireTimeout) {
            return;
          }
          clearTimeout(expireTimeout);
        }
      );
      // If there are no more permissions, remove the entry from the URI map.
      if (filterState != null && !Object.keys(permissions).length) {
        delete uriToPerm[uriKey];
      }
    });

    // We're either clearing all permissions or only the permissions with state
    // == filterState. If we have a filter, we can only clean up the browser if
    // there are no permission entries left in the map.
    if (filterState == null || !Object.keys(uriToPerm).length) {
      this._stateByBrowser.delete(browser);
    }
  },

  // Copies the temporary permission state of one browser
  // into a new entry for the other browser.
  copy(browser, newBrowser) {
    let entry = this._stateByBrowser.get(browser);
    if (entry) {
      entry.browser = Cu.getWeakReference(newBrowser);
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
          "nsIWebProgressListener",
          "nsISupportsWeakReference",
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
          state: gPermissions.get(id).getDefault(),
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

  // The delimiter used for double keyed permissions.
  // For example: open-protocol-handler^irc
  PERM_KEY_DELIMITER: "^",

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
    if (!principal) {
      throw new Error("principal argument cannot be null.");
    }
    if (!this.isSupportedPrincipal(principal)) {
      return [];
    }

    // Get all permissions from the permission manager by principal, excluding
    // the ones set to be disabled.
    let permissions = Services.perms
      .getAllForPrincipal(principal)
      .filter(permission => {
        let entry = gPermissions.get(permission.type);
        if (!entry || entry.disabled) {
          return false;
        }
        let type = entry.id;

        /* Hide persistent storage permission when extension principal
         * have WebExtensions-unlimitedStorage permission. */
        if (
          type == "persistent-storage" &&
          SitePermissions.getForPrincipal(
            principal,
            "WebExtensions-unlimitedStorage"
          ).state == SitePermissions.ALLOW
        ) {
          return false;
        }

        return true;
      });

    return permissions.map(permission => {
      let scope = this.SCOPE_PERSISTENT;
      if (permission.expireType == Services.perms.EXPIRE_SESSION) {
        scope = this.SCOPE_SESSION;
      } else if (permission.expireType == Services.perms.EXPIRE_POLICY) {
        scope = this.SCOPE_POLICY;
      }

      return {
        id: permission.type,
        scope,
        state: permission.capability,
      };
    });
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
   * principal.
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
    return this.isSupportedScheme(principal.scheme);
  },

  /**
   * Checks whether we support managing permissions for a specific scheme.
   * @param {string} scheme - Scheme to test.
   * @returns {boolean} Whether the scheme is supported.
   */
  isSupportedScheme(scheme) {
    return ["http", "https", "moz-extension", "file"].includes(scheme);
  },

  /**
   * Gets an array of all permission IDs.
   *
   * @return {Array<String>} an array of all permission IDs.
   */
  listPermissions() {
    if (this._permissionsArray === null) {
      this._permissionsArray = gPermissions.getEnabledPermissions();
    }
    return this._permissionsArray;
  },

  /**
   * Test whether a permission is managed by SitePermissions.
   * @param {string} type - Permission type.
   * @returns {boolean}
   */
  isSitePermission(type) {
    return gPermissions.has(type);
  },

  /**
   * Called when a preference changes its value.
   *
   * @param {string} data
   *        The last argument passed to the preference change observer
   * @param {string} previous
   *        The previous value of the preference
   * @param {string} latest
   *        The latest value of the preference
   */
  invalidatePermissionList(data, previous, latest) {
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
      gPermissions.has(permissionID) &&
      gPermissions.get(permissionID).states
    ) {
      return gPermissions.get(permissionID).states;
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
      gPermissions.has(permissionID) &&
      gPermissions.get(permissionID).getDefault
    ) {
      return gPermissions.get(permissionID).getDefault();
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
      gPermissions.has(permissionID) &&
      gPermissions.get(permissionID).setDefault
    ) {
      return gPermissions.get(permissionID).setDefault(state);
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
        gPermissions.has(permissionID) &&
        gPermissions.get(permissionID).exactHostMatch
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
   * @param {number} expireTimeMS (optional) If setting a temporary permission,
   *        how many milliseconds it should be valid for.
   * @param {nsIURI} browserURI (optional) Pass a custom URI for the
   *        temporary permission scope. This defaults to the current URI of the
   *        browser.
   */
  setForPrincipal(
    principal,
    permissionID,
    state,
    scope = this.SCOPE_PERSISTENT,
    browser = null,
    expireTimeMS = SitePermissions.temporaryPermissionExpireTime,
    browserURI = browser?.currentURI
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
      if (!browser) {
        throw new Error(
          "TEMPORARY scoped permissions require a browser object"
        );
      }
      if (!Number.isInteger(expireTimeMS) || expireTimeMS <= 0) {
        throw new Error("expireTime must be a positive integer");
      }

      TemporaryPermissions.set(
        browser,
        permissionID,
        state,
        expireTimeMS,
        browserURI,
        // On permission expiry
        origBrowser => {
          if (!origBrowser.ownerGlobal) {
            return;
          }
          origBrowser.dispatchEvent(
            new origBrowser.ownerGlobal.CustomEvent("PermissionStateChange")
          );
        }
      );

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
   * Clears all block permissions that were temporarily saved.
   *
   * @param {Browser} browser
   *        The browser object to clear.
   */
  clearTemporaryBlockPermissions(browser) {
    TemporaryPermissions.clear(browser, SitePermissions.BLOCK);
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
   * If a permission is double keyed (has an additional key in the ID), the
   * second key is split off and supplied to the string formatter as a variable.
   *
   * @param {string} permissionID
   *        The permission to get the label for. May include second key.
   *
   * @return {String} the localized label or null if none is available.
   */
  getPermissionLabel(permissionID) {
    let [id, key] = permissionID.split(this.PERM_KEY_DELIMITER);
    if (!gPermissions.has(id)) {
      // Permission can't be found.
      return null;
    }
    if (
      "labelID" in gPermissions.get(id) &&
      gPermissions.get(id).labelID === null
    ) {
      // Permission doesn't support having a label.
      return null;
    }
    if (id == "3rdPartyStorage") {
      // The key is the 3rd party origin, which we use for the label.
      return key;
    }
    let labelID = gPermissions.get(id).labelID || id;
    return gStringBundle.formatStringFromName(`permission.${labelID}.label`, [
      key,
    ]);
  },

  /**
   * Returns the localized label for the given permission state, to be used in
   * a UI for managing permissions.
   *
   * @param {string} permissionID
   *        The permission to get the label for.
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
      gPermissions.has(permissionID) &&
      gPermissions.get(permissionID).getMultichoiceStateLabel
    ) {
      return gPermissions.get(permissionID).getMultichoiceStateLabel(state);
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

let gPermissions = {
  _getId(type) {
    // Split off second key (if it exists).
    let [id] = type.split(SitePermissions.PERM_KEY_DELIMITER);
    return id;
  },

  has(type) {
    return this._getId(type) in this._permissions;
  },

  get(type) {
    let id = this._getId(type);
    let perm = this._permissions[id];
    if (perm) {
      perm.id = id;
    }
    return perm;
  },

  getEnabledPermissions() {
    return Object.keys(this._permissions).filter(
      id => !this._permissions[id].disabled
    );
  },

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
   *
   *  - getMultichoiceStateLabel
   *    Optional method to overwrite SitePermissions#getMultichoiceStateLabel with custom label logic.
   */
  _permissions: {
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
        throw new Error(`Unknown state: ${state}`);
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
          Services.cookies.cookieBehavior == Ci.nsICookieService.BEHAVIOR_REJECT
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

    "open-protocol-handler": {
      labelID: "open-protocol-handler",
      exactHostMatch: true,
      states: [SitePermissions.UNKNOWN, SitePermissions.ALLOW],
      get disabled() {
        return !SitePermissions.openProtoPermissionEnabled;
      },
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

    canvas: {
      get disabled() {
        return !SitePermissions.resistFingerprinting;
      },
    },

    midi: {
      exactHostMatch: true,
      get disabled() {
        return !SitePermissions.midiPermissionEnabled;
      },
    },

    "midi-sysex": {
      exactHostMatch: true,
      get disabled() {
        return !SitePermissions.midiPermissionEnabled;
      },
    },

    "storage-access": {
      labelID: null,
      getDefault() {
        return SitePermissions.UNKNOWN;
      },
    },

    "3rdPartyStorage": {
      get disabled() {
        return !SitePermissions.statePartitioningPermissionsEnabled;
      },
    },
  },
};

SitePermissions.midiPermissionEnabled = Services.prefs.getBoolPref(
  "dom.webmidi.enabled"
);

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
  SitePermissions.invalidatePermissionList.bind(SitePermissions)
);
XPCOMUtils.defineLazyPreferenceGetter(
  SitePermissions,
  "openProtoPermissionEnabled",
  "security.external_protocol_requires_permission",
  true,
  SitePermissions.invalidatePermissionList.bind(SitePermissions)
);
XPCOMUtils.defineLazyPreferenceGetter(
  SitePermissions,
  "statePartitioningPermissionsEnabled",
  "browser.contentblocking.state-partitioning.mvp.ui.enabled",
  false,
  SitePermissions.invalidatePermissionList.bind(SitePermissions)
);
