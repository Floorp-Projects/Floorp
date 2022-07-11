/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Utility module for tests to access the PermissionManager
 * with uri or origin string parameters.
 */

"use strict";

let pm = Services.perms;

let secMan = Services.scriptSecurityManager;

const EXPORTED_SYMBOLS = ["PermissionTestUtils"];

/**
 * Convert origin string or uri to principal.
 * If passed an nsIPrincipal it will be returned without conversion.
 * @param {Ci.nsIPrincipal|Ci.nsIURI|string} subject - Subject to convert to principal
 * @returns {Ci.nsIPrincipal} Principal created from subject
 */
function convertToPrincipal(subject) {
  if (subject instanceof Ci.nsIPrincipal) {
    return subject;
  }
  if (typeof subject === "string") {
    return secMan.createContentPrincipalFromOrigin(subject);
  }
  if (subject === null || subject instanceof Ci.nsIURI) {
    return secMan.createContentPrincipal(subject, {});
  }
  throw new Error(
    "subject parameter must be an nsIURI an origin string or a principal."
  );
}

let PermissionTestUtils = {
  /**
   * Add permission information for a given subject.
   * Subject can be a principal, uri or origin string.
   * @see nsIPermissionManager for documentation
   *
   * @param {Ci.nsIPrincipal|Ci.nsIURI|string} subject
   * @param {*} args
   */
  add(subject, ...args) {
    return pm.addFromPrincipal(convertToPrincipal(subject), ...args);
  },
  /**
   * Get all custom permissions for a given subject.
   * Subject can be a principal, uri or origin string.
   * @see nsIPermissionManager for documentation
   *
   * @param {Ci.nsIPrincipal|Ci.nsIURI|string} subject
   * @param {*} args
   */
  getAll(subject, ...args) {
    return pm.getAllForPrincipal(convertToPrincipal(subject), ...args);
  },
  /**
   * Remove permission information for a given subject and permission type
   * Subject can be a principal, uri or origin string.
   * @see nsIPermissionManager for documentation
   *
   * @param {Ci.nsIPrincipal|Ci.nsIURI|string} subject
   * @param {*} args
   */
  remove(subject, ...args) {
    return pm.removeFromPrincipal(convertToPrincipal(subject), ...args);
  },
  /**
   * Test whether a website has permission to perform the given action.
   * Subject can be a principal, uri or origin string.
   * @see nsIPermissionManager for documentation
   *
   * @param {Ci.nsIPrincipal|Ci.nsIURI|string} subject
   * @param {*} args
   */
  testPermission(subject, ...args) {
    return pm.testPermissionFromPrincipal(convertToPrincipal(subject), ...args);
  },
  /**
   * Test whether a website has permission to perform the given action.
   * Subject can be a principal, uri or origin string.
   * @see nsIPermissionManager for documentation
   *
   * @param {Ci.nsIPrincipal|Ci.nsIURI|string} subject
   * @param {*} args
   */
  testExactPermission(subject, ...args) {
    return pm.testExactPermissionFromPrincipal(
      convertToPrincipal(subject),
      ...args
    );
  },
  /**
   * Get the permission object associated with the given subject and action.
   * Subject can be a principal, uri or origin string.
   * @see nsIPermissionManager for documentation
   *
   * @param {Ci.nsIPrincipal|Ci.nsIURI|string} subject
   * @param {*} args
   */
  getPermissionObject(subject, type, exactHost = false) {
    return pm.getPermissionObject(convertToPrincipal(subject), type, exactHost);
  },
};
