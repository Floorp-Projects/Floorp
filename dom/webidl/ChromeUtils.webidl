/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * A collection of static utility methods that are only exposed to system code.
 * This is exposed in all the system globals where we can expose stuff by
 * default, so should only include methods that are **thread-safe**.
 */
[ChromeOnly, Exposed=(Window,System,Worker)]
namespace ChromeUtils {
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
  DOMString saveHeapSnapshot(optional HeapSnapshotBoundaries boundaries);

  /**
   * This is the same as saveHeapSnapshot, but with a different return value.
   *
   * @returns                 The snapshot ID of the file. This is the file name
   *                          without the temp directory or the trailing
   *                          `.fxsnapshot`.
   */
  [Throws]
  DOMString saveHeapSnapshotGetId(optional HeapSnapshotBoundaries boundaries);

  /**
   * Deserialize a core dump into a HeapSnapshot.
   *
   * @param filePath          The file path to read the heap snapshot from.
   */
  [Throws, NewObject]
  HeapSnapshot readHeapSnapshot(DOMString filePath);

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
  any nondeterministicGetWeakMapKeys(any map);

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
  any nondeterministicGetWeakSetKeys(any aSet);

  /**
   * Converts a buffer to a Base64 URL-encoded string per RFC 4648.
   *
   * @param source The buffer to encode.
   * @param options Additional encoding options.
   * @returns The encoded string.
   */
  [Throws]
  ByteString base64URLEncode(BufferSource source,
                             Base64URLEncodeOptions options);

  /**
   * Decodes a Base64 URL-encoded string per RFC 4648.
   *
   * @param string The string to decode.
   * @param options Additional decoding options.
   * @returns The decoded buffer.
   */
  [Throws, NewObject]
  ArrayBuffer base64URLDecode(ByteString string,
                              Base64URLDecodeOptions options);

#ifdef NIGHTLY_BUILD

  /**
   * If the chrome code has thrown a JavaScript Dev Error
   * in the current JSRuntime. the first such error, or `undefined`
   * otherwise.
   *
   * A JavaScript Dev Error is an exception thrown by JavaScript
   * code that matches both conditions:
   * - it was thrown by chrome code;
   * - it is either a `ReferenceError`, a `TypeError` or a `SyntaxError`.
   *
   * Such errors are stored regardless of whether they have been
   * caught.
   *
   * This mechanism is designed to help ensure that the code of
   * Firefox is free from Dev Errors, even if they are accidentally
   * caught by clients.
   *
   * The object returned is not an exception. It has fields:
   * - DOMString stack
   * - DOMString filename
   * - DOMString lineNumber
   * - DOMString message
   */
  [Throws]
  readonly attribute any recentJSDevError;

  /**
   * Reset `recentJSDevError` to `undefined` for the current JSRuntime.
   */
  void clearRecentJSDevError();
#endif // NIGHTLY_BUILD

  /**
   * IF YOU ADD NEW METHODS HERE, MAKE SURE THEY ARE THREAD-SAFE.
   */
};

/**
 * Additional ChromeUtils methods that are _not_ thread-safe, and hence not
 * exposed in workers.
 */
[Exposed=(Window,System)]
partial namespace ChromeUtils {
  /**
   * A helper that converts OriginAttributesDictionary to a opaque suffix string.
   *
   * @param originAttrs       The originAttributes from the caller.
   */
  ByteString
  originAttributesToSuffix(optional OriginAttributesDictionary originAttrs);

  /**
   * Returns true if the members of |originAttrs| match the provided members
   * of |pattern|.
   *
   * @param originAttrs       The originAttributes under consideration.
   * @param pattern           The pattern to use for matching.
   */
  boolean
  originAttributesMatchPattern(optional OriginAttributesDictionary originAttrs,
                               optional OriginAttributesPatternDictionary pattern);

  /**
   * Returns an OriginAttributesDictionary with values from the |origin| suffix
   * and unspecified attributes added and assigned default values.
   *
   * @param origin            The origin URI to create from.
   * @returns                 An OriginAttributesDictionary with values from
   *                          the origin suffix and unspecified attributes
   *                          added and assigned default values.
   */
  [Throws]
  OriginAttributesDictionary
  createOriginAttributesFromOrigin(DOMString origin);

  /**
   * Returns an OriginAttributesDictionary that is a copy of |originAttrs| with
   * unspecified attributes added and assigned default values.
   *
   * @param originAttrs       The origin attributes to copy.
   * @returns                 An OriginAttributesDictionary copy of |originAttrs|
   *                          with unspecified attributes added and assigned
   *                          default values.
   */
  OriginAttributesDictionary
  fillNonDefaultOriginAttributes(optional OriginAttributesDictionary originAttrs);

  /**
   * Returns true if the 2 OriginAttributes are equal.
   */
  boolean
  isOriginAttributesEqual(optional OriginAttributesDictionary aA,
                          optional OriginAttributesDictionary aB);

  /**
   * Loads and compiles the script at the given URL and returns an object
   * which may be used to execute it repeatedly, in different globals, without
   * re-parsing.
   */
  [NewObject]
  Promise<PrecompiledScript>
  compileScript(DOMString url, optional CompileScriptOptionsDictionary options);

  /**
   * Waive Xray on a given value. Identity op for primitives.
   */
  [Throws]
  any waiveXrays(any val);

  /**
   * Strip off Xray waivers on a given value. Identity op for primitives.
   */
  [Throws]
  any unwaiveXrays(any val);

  /**
   * Gets the name of the JSClass of the object.
   *
   * if |unwrap| is true, all wrappers are unwrapped first. Unless you're
   * specifically trying to detect whether the object is a proxy, this is
   * probably what you want.
   */
  DOMString getClassName(object obj, optional boolean unwrap = true);

  /**
   * Clones the properties of the given object into a new object in the given
   * target compartment (or the caller compartment if no target is provided).
   * Property values themeselves are not cloned.
   *
   * Ignores non-enumerable properties, properties on prototypes, and properties
   * with getters or setters.
   */
  [Throws]
  object shallowClone(object obj, optional object? target = null);

  /**
   * Dispatches the given callback to the main thread when it would be
   * otherwise idle. Similar to Window.requestIdleCallback, but not bound to a
   * particular DOM windw.
   */
  [Throws]
  void idleDispatch(IdleRequestCallback callback,
                    optional IdleRequestOptions options);

  /**
   * Synchronously loads and evaluates the js file located at
   * 'aResourceURI' with a new, fully privileged global object.
   *
   * If 'aTargetObj' is specified and null, this method just returns
   * the module's global object. Otherwise (if 'aTargetObj' is not
   * specified, or 'aTargetObj' is != null) looks for a property
   * 'EXPORTED_SYMBOLS' on the new global object. 'EXPORTED_SYMBOLS'
   * is expected to be an array of strings identifying properties on
   * the global object.  These properties will be installed as
   * properties on 'targetObj', or, if 'aTargetObj' is not specified,
   * on the caller's global object. If 'EXPORTED_SYMBOLS' is not
   * found, an error is thrown.
   *
   * @param aResourceURI A resource:// URI string to load the module from.
   * @param aTargetObj the object to install the exported properties on or null.
   * @returns the module code's global object.
   *
   * The implementation maintains a hash of aResourceURI->global obj.
   * Subsequent invocations of import with 'aResourceURI' pointing to
   * the same file will not cause the module to be re-evaluated, but
   * the symbols in EXPORTED_SYMBOLS will be exported into the
   * specified target object and the global object returned as above.
   */
  [Throws]
  object import(DOMString aResourceURI, optional object? aTargetObj);
};

/**
 * Used by principals and the script security manager to represent origin
 * attributes. The first dictionary is designed to contain the full set of
 * OriginAttributes, the second is used for pattern-matching (i.e. does this
 * OriginAttributesDictionary match the non-empty attributes in this pattern).
 *
 * IMPORTANT: If you add any members here, you need to do the following:
 * (1) Add them to both dictionaries.
 * (2) Update the methods on mozilla::OriginAttributes, including equality,
 *     serialization, deserialization, and inheritance.
 * (3) Update the methods on mozilla::OriginAttributesPattern, including matching.
 */
dictionary OriginAttributesDictionary {
  unsigned long appId = 0;
  unsigned long userContextId = 0;
  boolean inIsolatedMozBrowser = false;
  unsigned long privateBrowsingId = 0;
  DOMString firstPartyDomain = "";
};
dictionary OriginAttributesPatternDictionary {
  unsigned long appId;
  unsigned long userContextId;
  boolean inIsolatedMozBrowser;
  unsigned long privateBrowsingId;
  DOMString firstPartyDomain;
};

dictionary CompileScriptOptionsDictionary {
  /**
   * The character set from which to decode the script.
   */
  DOMString charset = "utf-8";

  /**
   * If true, certain parts of the script may be parsed lazily, the first time
   * they are used, rather than eagerly parsed at load time.
   */
  boolean lazilyParse = false;

  /**
   * If true, the script will be compiled so that its last expression will be
   * returned as the value of its execution. This makes certain types of
   * optimization impossible, and disables the JIT in many circumstances, so
   * should not be used when not absolutely necessary.
   */
  boolean hasReturnValue = false;
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
