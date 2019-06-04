/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.
*
* The origin of this IDL file is
* http://www.whatwg.org/html/#the-storage-interface
*
* © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
* Opera Software ASA. You are granted a license to use, reproduce
* and create derivative works of this document.
*/

interface Storage {
  [Throws, NeedsSubjectPrincipal]
  readonly attribute unsigned long length;

  [Throws, NeedsSubjectPrincipal]
  DOMString? key(unsigned long index);

  [Throws, NeedsSubjectPrincipal]
  getter DOMString? getItem(DOMString key);

  [Throws, NeedsSubjectPrincipal]
  setter void setItem(DOMString key, DOMString value);

  [Throws, NeedsSubjectPrincipal]
  deleter void removeItem(DOMString key);

  [Throws, NeedsSubjectPrincipal]
  void clear();

  [ChromeOnly]
  readonly attribute boolean isSessionOnly;
};

/**
 * Testing methods that exist only for the benefit of automated glass-box
 * testing.  Will never be exposed to content at large and unlikely to be useful
 * in a WebDriver context.
 */
partial interface Storage {
  /**
   * Does a security-check and ensures the underlying database has been opened
   * without actually calling any database methods.  (Read-only methods will
   * have a similar effect but also impact the state of the snapshot.)
   */
  [Throws, NeedsSubjectPrincipal, Pref="dom.storage.testing"]
  void open();

  /**
   * Automatically ends any explicit snapshot and drops the reference to the
   * underlying database, but does not otherwise perturb the database.
   */
  [Throws, NeedsSubjectPrincipal, Pref="dom.storage.testing"]
  void close();

  /**
   * Ensures the database has been opened and initiates an explicit snapshot.
   * Snapshots are normally automatically ended and checkpointed back to the
   * parent, but explicitly opened snapshots must be explicitly ended via
   * `endExplicitSnapshot` or `close`.
   */
  [Throws, NeedsSubjectPrincipal, Pref="dom.storage.testing"]
  void beginExplicitSnapshot();

  /**
   * Ends the explicitly begun snapshot and retains the underlying database.
   * Compare with `close` which also drops the reference to the database.
   */
  [Throws, NeedsSubjectPrincipal, Pref="dom.storage.testing"]
  void endExplicitSnapshot();

  /**
   * Returns true if the underlying database has been opened and it has an
   * active snapshot (initialized implicitly or explicitly).
   */
  [Throws, NeedsSubjectPrincipal, Pref="dom.storage.testing"]
  readonly attribute boolean hasActiveSnapshot;
};
