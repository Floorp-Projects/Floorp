/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * A collection of **thread-safe** static utility methods that are only exposed
 * to Chrome. This interface is exposed in workers, while ChromeUtils is not.
 */
[ChromeOnly, Exposed=(Window,System,Worker)]
interface ThreadSafeChromeUtils {
  /**
   * Serialize a snapshot of the heap graph, as seen by |JS::ubi::Node| and
   * restricted by |boundaries|, and write it to the provided file path.
   *
   * @param boundaries        The portion of the heap graph to write.
   *
   * @returns                 The path to the file the heap snapshot was written
   *                          to. This is guaranteed to be within the temp
   *                          directory and its file name will match the regexp
   *                          `\d+(\-\d+)?\.fxsnapshot`.
   */
  [Throws]
  static DOMString saveHeapSnapshot(optional HeapSnapshotBoundaries boundaries);

  /**
   * Deserialize a core dump into a HeapSnapshot.
   *
   * @param filePath          The file path to read the heap snapshot from.
   */
  [Throws, NewObject]
  static HeapSnapshot readHeapSnapshot(DOMString filePath);

  /**
   * Return the keys in a weak map.  This operation is
   * non-deterministic because it is affected by the scheduling of the
   * garbage collector and the cycle collector.
   *
   * @param aMap weak map or other JavaScript value
   * @returns If aMap is a weak map object, return the keys of the weak
   *          map as an array.  Otherwise, return undefined.
   */
  [Throws, NewObject]
  static any nondeterministicGetWeakMapKeys(any map);

  /**
   * Return the keys in a weak set.  This operation is
   * non-deterministic because it is affected by the scheduling of the
   * garbage collector and the cycle collector.
   *
   * @param aSet weak set or other JavaScript value
   * @returns If aSet is a weak set object, return the keys of the weak
   *          set as an array.  Otherwise, return undefined.
   */
  [Throws, NewObject]
  static any nondeterministicGetWeakSetKeys(any aSet);

  /**
   * Converts a buffer to a Base64 URL-encoded string per RFC 4648.
   *
   * @param source The buffer to encode.
   * @param options Additional encoding options.
   * @returns The encoded string.
   */
  [Throws]
  static ByteString base64URLEncode(BufferSource source,
                                    Base64URLEncodeOptions options);

  /**
   * Decodes a Base64 URL-encoded string per RFC 4648.
   *
   * @param string The string to decode.
   * @param options Additional decoding options.
   * @returns The decoded buffer.
   */
  [Throws, NewObject]
  static ArrayBuffer base64URLDecode(ByteString string,
                                     Base64URLDecodeOptions options);
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

dictionary Base64URLEncodeOptions {
  /** Specifies whether the output should be padded with "=" characters. */
  required boolean pad;
};

enum Base64URLDecodePadding {
  /**
   * Fails decoding if the input is unpadded. RFC 4648, section 3.2 requires
   * padding, unless the referring specification prohibits it.
   */
  "require",

  /** Tolerates padded and unpadded input. */
  "ignore",

  /**
   * Fails decoding if the input is padded. This follows the strict base64url
   * variant used in JWS (RFC 7515, Appendix C) and HTTP Encrypted
   * Content-Encoding (draft-ietf-httpbis-encryption-encoding-01).
   */
  "reject"
};

dictionary Base64URLDecodeOptions {
  /** Specifies the padding mode for decoding the input. */
  required Base64URLDecodePadding padding;
};
