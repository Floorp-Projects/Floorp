/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * A HeapSnapshot represents a snapshot of the heap graph
 */
[ChromeOnly, Exposed=(Window,Worker)]
interface HeapSnapshot {
  /**
   * A time stamp of when the heap snapshot was taken, if available. Units are
   * microseconds since midnight (00:00:00) 1 January 1970 UTC.
   */
  readonly attribute unsigned long long? creationTime;

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

  /**
   * Describe `node` with the specified `breakdown`. See the comment above
   * `takeCensus` or `js/src/doc/Debugger/Debugger.Memory.md` for detailed
   * documentation on breakdowns.
   *
   * Throws an error when `node` is not the id of a node in the heap snapshot,
   * or if the breakdown is invalid.
   */
  [Throws]
  any describeNode(object breakdown, NodeId node);

  /**
   * Compute the dominator tree for this heap snapshot.
   *
   * @see DominatorTree.webidl
   */
  [Throws]
  DominatorTree computeDominatorTree();

  /**
   * Find the shortest retaining paths from the node associated with the ID
   * `start` to each node associated with the IDs in `targets`. Find at most
   * `maxNumPaths` retaining paths for each target node.
   *
   * The return value is a Map object mapping from each target node ID to an
   * array of retaining paths. The array may be empty if we did not find any
   * retaining paths.
   *
   * A path is an array of objects of the form:
   *
   *     {
   *         predecessor: <node ID>,
   *         edge: <string or null>,
   *     }
   *
   * The first `predecessor` will always be `start`. The last edge in the path
   * leads to the `target` node that is mapped to the path; the `target` does
   * not appear as a `predecessor` in the path.
   *
   * Throws when `start` or any of the elements of `targets` are not an ID of a
   * node in the snapshot, or if we encounter an out of memory exception.
   */
  [Throws]
  object computeShortestPaths(NodeId start, sequence<NodeId> targets,
                              unsigned long long maxNumPaths);
};
