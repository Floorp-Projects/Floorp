/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * A HeapSnapshot represents a snapshot of the heap graph
 */
[ChromeOnly, Exposed=(Window,System,Worker)]
interface HeapSnapshot {
  /**
   * Take a census of the heap snapshot.
   *
   * This is the same as |Debugger.Memory.prototype.takeCensus|, but operates on
   * the offline heap snapshot's serialized heap graph rather than the live heap
   * graph. The same optional configuration options that can be passed to that
   * function can be passed here.
   *
   * The returned value is determined by the `"breakdown"` option used, and is
   * usually a `Map`, `Object`, or `Array`. For example, the following breakdown
   *
   *     {
   *       by: "coarseType",
   *       objects: { by: "objectClass" },
   *       other:   { by: "internalType" }
   *     }
   *
   * produces a result like this:
   *
   *     {
   *       "objects": {
   *         "Function":         { "count": 404, "bytes": 37328 },
   *         "Object":           { "count": 11,  "bytes": 1264 },
   *         "Debugger":         { "count": 1,   "bytes": 416 },
   *         "ScriptSource":     { "count": 1,   "bytes": 64 },
   *         // ... omitted for brevity...
   *       },
   *       "scripts":            { "count": 1,   "bytes": 0 },
   *       "strings":            { "count": 701, "bytes": 49080 },
   *       "other": {
   *         "js::Shape":        { "count": 450, "bytes": 0 },
   *         "js::BaseShape":    { "count": 21,  "bytes": 0 },
   *         "js::ObjectGroup":  { "count": 17,  "bytes": 0 }
   *       }
   *     }
   *
   * See the `takeCensus` section of the `js/src/doc/Debugger/Debugger.Memory.md`
   * file for detailed documentation.
   */
  [Throws]
  any takeCensus(object? options);
};
