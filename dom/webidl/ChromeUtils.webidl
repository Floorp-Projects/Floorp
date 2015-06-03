/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * A collection of static utility methods that are only exposed to Chrome.
 */
[ChromeOnly, Exposed=(Window,System)]
interface ChromeUtils {
  /**
   * Serialize a snapshot of the heap graph, as seen by |JS::ubi::Node| and
   * restricted by |boundaries|, and write it to the provided file path.
   *
   * @param filePath          The file path to write the heap snapshot to.
   *
   * @param boundaries        The portion of the heap graph to write.
   */
  [Throws]
  static void saveHeapSnapshot(DOMString filePath,
                               optional HeapSnapshotBoundaries boundaries);

  /**
   * Deserialize a core dump into a HeapSnapshot.
   *
   * @param filePath          The file path to read the core dump from.
   */
  [Throws, NewObject]
  static HeapSnapshot readHeapSnapshot(DOMString filePath);

  /**
   * A helper that converts OriginAttributesDictionary to cookie jar opaque
   * identfier.
   *
   * @param originAttrs       The originAttributes from the caller.
   */
  static ByteString
  originAttributesToCookieJar(optional OriginAttributesDictionary originAttrs);
};

/**
 * A JS object whose properties specify what portion of the heap graph to
 * write. The recognized properties are:
 *
 * * globals: [ global, ... ]
 *   Dump only nodes that either:
 *   - belong in the compartment of one of the given globals;
 *   - belong to no compartment, but do belong to a Zone that contains one of
 *     the given globals;
 *   - are referred to directly by one of the last two kinds of nodes; or
 *   - is the fictional root node, described below.
 *
 * * debugger: Debugger object
 *   Like "globals", but use the Debugger's debuggees as the globals.
 *
 * * runtime: true
 *   Dump the entire heap graph, starting with the JSRuntime's roots.
 *
 * One, and only one, of these properties must exist on the boundaries object.
 *
 * The root of the dumped graph is a fictional node whose ubi::Node type name is
 * "CoreDumpRoot". If we are dumping the entire ubi::Node graph, this root node
 * has an edge for each of the JSRuntime's roots. If we are dumping a selected
 * set of globals, the root has an edge to each global, and an edge for each
 * incoming JS reference to the selected Zones.
 */
dictionary HeapSnapshotBoundaries {
  sequence<object> globals;
  object           debugger;
  boolean          runtime;
};

/**
 * Used by principals and the script security manager to represent origin
 * attributes.
 *
 * IMPORTANT: If you add any members here, you need to update the
 * methods on mozilla::OriginAttributes, and bump the CIDs of all
 * the principal implementations that use OriginAttributes in their
 * nsISerializable implementations.
 */
dictionary OriginAttributesDictionary {
  unsigned long appId = 0;
  boolean inBrowser = false;
};
