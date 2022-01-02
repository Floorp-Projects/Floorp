/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface nsISHistory;

/**
 * The ChildSHistory interface represents the child side of a browsing
 * context's session history.
 */
[ChromeOnly,
 Exposed=Window]
interface ChildSHistory {
  [Pure]
  readonly attribute long count;
  [Pure]
  readonly attribute long index;

  boolean canGo(long aOffset);
  [Throws] void go(long aOffset, optional boolean aRequireUserInteraction = false, optional boolean aUserActivation = false);

  /**
   * Reload the current entry. The flags which should be passed to this
   * function are documented and defined in nsIWebNavigation.idl
   */
  [Throws]
  void reload(unsigned long aReloadFlags);

  /**
   * Getter for the legacy nsISHistory implementation.
   *
   * legacySHistory has been deprecated. Don't use it, but instead handle
   * the interaction with nsISHistory in the parent process.
   */
  [Throws]
  readonly attribute nsISHistory legacySHistory;
};
