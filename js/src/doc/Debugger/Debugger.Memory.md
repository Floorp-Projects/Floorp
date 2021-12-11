Debugger.Memory
===============

The [`Debugger API`][debugger] can help tools observe the debuggee's memory use
in various ways:

- It can mark each new object with the JavaScript call stack at which it was
  allocated.

- It can log all object allocations, yielding a stream of JavaScript call stacks
  at which allocations have occurred.

- It can compute a *census* of items belonging to the debuggee, categorizing
  items in various ways, and yielding item counts.

If <i>dbg</i> is a [`Debugger`][debugger-object] instance, then the methods and
accessor properties of `dbg.memory` control how <i>dbg</i>
observes its debuggees' memory use. The `dbg.memory` object is
an instance of `Debugger.Memory`; its inherited accessors and methods are
described below.


### Allocation Site Tracking

The JavaScript engine marks each new object with the call stack at which it was
allocated, if:

- the object is allocated in the scope of a global object that is a debuggee of
  some [`Debugger`][debugger-object] instance <i>dbg</i>; and

- <code><i>dbg</i>.memory.[trackingAllocationSites][tracking-allocs]</code> is
  set to `true`.

- A [Bernoulli trial][bernoulli-trial] succeeds, with probability equal to the
  maximum of
  [`d.memory.allocationSamplingProbability`][alloc-sampling-probability] of all
  `Debugger` instances `d` that are observing the global that this object is
  allocated within the scope of.

Given a [`Debugger.Object`][object] instance <i>dobj</i> referring to some
object, <code><i>dobj</i>.[allocationSite][allocation-site]</code> returns a
[saved call stack][saved-frame] indicating where <i>dobj</i>'s referent was
allocated.


### Allocation Logging

If <i>dbg</i> is a [`Debugger`][debugger-object] instance, and
<code><i>dbg</i>.memory.[trackingAllocationSites][tracking-allocs]</code> is set
to `true`, then the JavaScript engine logs each object allocated by <i>dbg</i>'s
debuggee code. You can retrieve the current log by calling
<code><i>dbg</i>.memory.[drainAllocationsLog][drain-alloc-log]</code>. You can
control the limit on the log's size by setting
<code><i>dbg</i>.memory.[maxAllocationsLogLength][max-alloc-log]</code>.


### Censuses

A *census* is a complete traversal of the graph of all reachable memory items
belonging to a particular `Debugger`'s debuggees. It produces a count of those
items, broken down by various criteria. If <i>dbg</i> is a
[`Debugger`][debugger-object] instance, you can call
<code><i>dbg</i>.memory.[takeCensus][take-census]</code> to conduct a census of
its debuggees' possessions.


Accessor Properties of the `Debugger.Memory.prototype` Object
-------------------------------------------------------------

If <i>dbg</i> is a [`Debugger`][debugger-object] instance, then
`<i>dbg</i>.memory` is a `Debugger.Memory` instance, which inherits the
following accessor properties from its prototype:

### `trackingAllocationSites`
A boolean value indicating whether this `Debugger.Memory` instance is
capturing the JavaScript execution stack when each Object is allocated. This
accessor property has both a getter and setter: assigning to it enables or
disables the allocation site tracking. Reading the accessor produces `true`
if the Debugger is capturing stacks for Object allocations, and `false`
otherwise. Allocation site tracking is initially disabled in a new Debugger.

Assignment is fallible: if the Debugger cannot track allocation sites, it
throws an `Error` instance.

You can retrieve the allocation site for a given object with the
[`Debugger.Object.prototype.allocationSite`][allocation-site] accessor
property.

### `allocationSamplingProbability`
A number between 0 and 1 that indicates the probability with which each new
allocation should be entered into the allocations log. 0 is equivalent to
"never", 1 is "always", and .05 would be "one out of twenty".

The default is 1, or logging every allocation.

Note that in the presence of multiple <code>Debugger</code> instances
observing the same allocations within a global's scope, the maximum
<code>allocationSamplingProbability</code> of all the
<code>Debugger</code>s is used.

### `maxAllocationsLogLength`
The maximum number of allocation sites to accumulate in the allocations log
at a time. This accessor can be both fetched and stored to. Its default
value is `5000`.

### `allocationsLogOverflowed`
Returns `true` if there have been more than
[`maxAllocationsLogLength`][#max-alloc-log] allocations since the last time
[`drainAllocationsLog`][#drain-alloc-log] was called and some data has been
lost. Returns `false` otherwise.

Debugger.Memory Handler Functions
---------------------------------

Similar to [`Debugger`'s handler functions][debugger], `Debugger.Memory`
inherits accessor properties that store handler functions for SpiderMonkey to
call when given events occur in debuggee code.

Unlike `Debugger`'s hooks, `Debugger.Memory`'s handlers' return values are not
significant, and are ignored. The handler functions receive the
`Debugger.Memory`'s owning `Debugger` instance as their `this` value. The owning
`Debugger`'s `uncaughtExceptionHandler` is still fired for errors thrown in
`Debugger.Memory` hooks.

On a new `Debugger.Memory` instance, each of these properties is initially
`undefined`. Any value assigned to a debugging handler must be either a function
or `undefined`; otherwise a `TypeError` is thrown.

Handler functions run in the same thread in which the event occurred.
They run in the compartment to which they belong, not in a debuggee
compartment.

### `onGarbageCollection(statistics)`
A garbage collection cycle spanning one or more debuggees has just been
completed.

The *statistics* parameter is an object containing information about the GC
cycle. It has the following properties:

#### `collections`
The `collections` property's value is an array. Because SpiderMonkey's
collector is incremental, a full collection cycle may consist of
multiple discrete collection slices with the JS mutator running
interleaved. For each collection slice that occurred, there is an entry
in the `collections` array with the following form:

```
{
  "startTimestamp": timestamp,
  "endTimestamp": timestamp,
}
```

Here the `timestamp` values are [timestamps][timestamps] of the GC slice's start
and end events.

#### `reason`
A very short string describing the reason why the collection was
triggered. Known values include the following:

* `"API"`
* `"EAGER_ALLOC_TRIGGER"`
* `"DESTROY_RUNTIME"`
* `"LAST_DITCH"`
* `"TOO_MUCH_MALLOC"`
* `"ALLOC_TRIGGER"`
* `"DEBUG_GC"`
* `"COMPARTMENT_REVIVED"`
* `"RESET"`
* `"OUT_OF_NURSERY"`
* `"EVICT_NURSERY"`
* `"FULL_STORE_BUFFER"`
* `"SHARED_MEMORY_LIMIT"`
* `"PERIODIC_FULL_GC"`
* `"INCREMENTAL_TOO_SLOW"`
* `"DOM_WINDOW_UTILS"`
* `"COMPONENT_UTILS"`
* `"MEM_PRESSURE"`
* `"CC_FINISHED"`
* `"CC_FORCED"`
* `"LOAD_END"`
* `"PAGE_HIDE"`
* `"NSJSCONTEXT_DESTROY"`
* `"SET_NEW_DOCUMENT"`
* `"SET_DOC_SHELL"`
* `"DOM_UTILS"`
* `"DOM_IPC"`
* `"DOM_WORKER"`
* `"INTER_SLICE_GC"`
* `"REFRESH_FRAME"`
* `"FULL_GC_TIMER"`
* `"SHUTDOWN_CC"`
* `"USER_INACTIVE"`

#### `nonincrementalReason`
If SpiderMonkey's collector determined it could not incrementally
collect garbage, and had to do a full GC all at once, this is a short
string describing the reason it determined the full GC was necessary.
Otherwise, `null` is returned. Known values include the following:

* `"GC mode"`
* `"malloc bytes trigger"`
* `"allocation trigger"`
* `"requested"`

#### `gcCycleNumber`
The GC cycle's "number". Does not correspond to the number
of GC cycles that have run, but is guaranteed to be monotonically
increasing.

Function Properties of the `Debugger.Memory.prototype` Object
-------------------------------------------------------------

### `drainAllocationsLog()`
When `trackingAllocationSites` is `true`, this method returns an array of
recent `Object` allocations within the set of debuggees. *Recent* is
defined as the `maxAllocationsLogLength` most recent `Object` allocations
since the last call to `drainAllocationsLog`. Therefore, calling this
method effectively clears the log.

Objects in the array are of the form:

```
{
  "timestamp": timestamp,
  "frame": allocationSite,
  "class": className,
  "size": byteSize,
  "inNursery": inNursery,
}
```

Where

* `timestamp` is the [timestamp][timestamps] of the allocation event.

* `allocationSite` is an allocation site (as a
  [captured stack][saved-frame]). Note that this property can be null if the
  object was allocated with no JavaScript frames on the stack.

* `className` is the string name of the allocated object's internal
`[[Class]]` property, for example "Array", "Date", "RegExp", or (most
commonly) "Object".

* `byteSize` is the size of the object in bytes.

* `inNursery` is true if the allocation happened inside the nursery. False
  if the allocation skipped the nursery and started in the tenured heap.

When `trackingAllocationSites` is `false`, `drainAllocationsLog()` throws an
`Error`.

### `takeCensus(options)`
Carry out a census of the debuggee compartments' contents. A *census* is a
complete traversal of the graph of all reachable memory items belonging to a
particular `Debugger`'s debuggees. The census produces a count of those
items, broken down by various criteria.

The <i>options</i> argument is an object whose properties specify how the
census should be carried out.

If <i>options</i> has a `breakdown` property, that determines how the census
categorizes the items it finds, and what data it collects about them. For
example, if `dbg` is a `Debugger` instance, the following performs a simple
count of debuggee items:

    dbg.memory.takeCensus({ breakdown: { by: 'count' } })

That might produce a result like:

    { "count": 1616, "bytes": 93240 }

Here is a breakdown that groups JavaScript objects by their class name,
non-string, non-script items by their C++ type name, and DOM nodes with
their node name:

    {
      by: "coarseType",
      objects: { by: "objectClass" },
      other:   { by: "internalType" },
      domNode: { by: "descriptiveType" }
    }

which produces a result like this:

    {
      "objects": {
        "Function":         { "count": 404, "bytes": 37328 },
        "Object":           { "count": 11,  "bytes": 1264 },
        "Debugger":         { "count": 1,   "bytes": 416 },
        "ScriptSource":     { "count": 1,   "bytes": 64 },
        // ... omitted for brevity...
      },
      "scripts":            { "count": 1,   "bytes": 0 },
      "strings":            { "count": 701, "bytes": 49080 },
      "other": {
        "js::Shape":        { "count": 450, "bytes": 0 },
        "js::BaseShape":    { "count": 21,  "bytes": 0 },
        "js::ObjectGroup":  { "count": 17,  "bytes": 0 }
      },
      "domNode": {
        "#text":            { "count": 1,   "bytes": 12 }
      }
    }

In general, a `breakdown` value has one of the following forms:

* <code>{ by: "count", count:<i>count<i>, bytes:<i>bytes</i> }</code>

  The trivial categorization: none whatsoever. Simply tally up the items
  visited. If <i>count</i> is true, count the number of items visited; if
  <i>bytes</i> is true, total the number of bytes the items use directly.
  Both <i>count</i> and <i>bytes</i> are optional; if omitted, they
  default to `true`. In the result of the census, this breakdown produces
  a value of the form:

      { "count": n, "bytes": b }

  where the `count` and `bytes` properties are present as directed by the
  <i>count</i> and <i>bytes</i> properties on the breakdown.

  Note that the census can produce byte sizes only for the most common
  types. When the census cannot find the byte size for a given type, it
  returns zero.

* <code>{ by: "bucket" }</code>

  Do not do any filtering or categorizing. Instead, accumulate a bucket of
  each node's ID for every node that matches. The resulting report is an
  array of the IDs.

  For example, to find the ID of all nodes whose internal object
  `[[class]]` property is named "RegExp", you could use the following code:

      const report = dbg.memory.takeCensus({
        breakdown: {
          by: "objectClass",
          then: { by: "bucket" }
        }
      });
      doStuffWithRegExpIDs(report.RegExp);

* <code>{ by: "allocationStack", then:<i>breakdown</i>, noStack:<i>noStackBreakdown</i> }</code>

  Group items by the full JavaScript stack trace at which they were
  allocated.

  Further categorize all the items allocated at each distinct stack using
  <i>breakdown</i>.

  In the result of the census, this breakdown produces a JavaScript `Map`
  value whose keys are `SavedFrame` values, and whose values are whatever
  sort of result <i>breakdown</i> produces. Objects allocated on an empty
  JavaScript stack appear under the key `null`.

  SpiderMonkey only tracks allocation sites for items if requested via the
  [`trackingAllocationSites`][tracking-allocs] flag; even then, it does
  not record allocation sites for every kind of item that appears in the
  heap. Items that lack allocation site information are counted using
  <i>noStackBreakdown</i>. These appear in the result `Map` under the key
  string `"noStack"`.

* <code>{ by: "objectClass", then:<i>breakdown</i>, other:<i>otherBreakdown</i> }</code>

  Group JavaScript objects by their ECMAScript `[[Class]]` internal property values.

  Further categorize JavaScript objects in each class using
  <i>breakdown</i>. Further categorize items that are not JavaScript
  objects using <i>otherBreakdown</i>.

  In the result of the census, this breakdown produces a JavaScript object
  with no prototype whose own property names are strings naming classes,
  and whose values are whatever sort of result <i>breakdown</i> produces.
  The results for non-object items appear as the value of the property
  named `"other"`.

* <code>{ by: "coarseType", objects:<i>objects</i>, scripts:<i>scripts</i>, strings:<i>strings</i>, domNode:<i>domNode</i>, other:<i>other</i> }</code>

  Group items by their coarse type.

  Use the breakdown value <i>objects</i> for items that are JavaScript
  objects.

  Use the breakdown value <i>scripts</i> for items that are
  representations of JavaScript code. This includes bytecode, compiled
  machine code, and saved source code.

  Use the breakdown value <i>strings</i> for JavaScript strings.

  Use the breakdown value <i>domNode</i> for DOM nodes.

  Use the breakdown value <i>other</i> for items that don't fit into any of
  the above categories.

  In the result of the census, this breakdown produces a JavaScript object
  of the form:

  ```
  {
    "objects": result,
    "scripts": result,
    "strings": result,
    "domNode:" result,
    "other": result,
  }
  ```

  where each <i>result</i> is a value of whatever sort the corresponding
  breakdown value produces. All breakdown values are optional, and default
  to `{ type: "count" }`.

* `{ by: "internalType", then: breakdown }`

  Group items by the names given their types internally by SpiderMonkey.
  These names are not meaningful to web developers, but this type of
  breakdown does serve as a catch-all that can be useful to Firefox tool
  developers.

  For example, a census of a pristine debuggee global broken down by
  internal type name typically looks like this:

      {
        "JSString":        { "count": 701, "bytes": 49080 },
        "js::Shape":       { "count": 450, "bytes": 0 },
        "JSObject":        { "count": 426, "bytes": 44160 },
        "js::BaseShape":   { "count": 21,  "bytes": 0 },
        "js::ObjectGroup": { "count": 17,  "bytes": 0 },
        "JSScript":        { "count": 1,   "bytes": 0 }
      }

  In the result of the census, this breakdown produces a JavaScript object
  with no prototype whose own property names are strings naming types,
  and whose values are whatever sort of result <i>breakdown</i> produces.

* <code>[ <i>breakdown</i>, ... ]</code>

  Group each item using all the given breakdown values. In the result of
  the census, this breakdown produces an array of values of the sort
  produced by each listed breakdown.

To simplify breakdown values, all `then` and `other` properties are optional.
If omitted, they are treated as if they were `{ type: "count" }`.

If the `options` argument has no `breakdown` property, `takeCensus` defaults
to the following:

```js
{
  by: "coarseType",
  objects: { by: "objectClass" },
  domNode: { by: "descriptiveType" },
  other:   { by: "internalType" }
}
```

which produces results of the form:

```
{
  objects: { class: count, ... },
  scripts: count,
  strings: count,
  domNode: { node name:count, ... },
  other:   { type name:count, ... }
}
```

where each `count` has the form:

```js
{ "count": count, bytes: bytes }
```

Because performing a census requires traversing the entire graph of objects
in debuggee compartments, it is an expensive operation. On developer
hardware in 2014, traversing a memory graph containing roughly 130,000 nodes
and 410,000 edges took roughly 100ms. The traversal itself temporarily
allocates one hash table entry per node (roughly two address-sized words) in
addition to the per-category counts, whose size depends on the number of
categories.


Memory Use Analysis Exposes Implementation Details
--------------------------------------------------

Memory analysis may yield surprising results, because browser implementation
details that are transparent to content JavaScript often have visible effects on
memory consumption. Web developers need to know their pages' actual memory
consumption on real browsers, so it is correct for the tool to expose these
behaviors, as long as it is done in a way that helps developers make decisions
about their own code.

This section covers some areas where Firefox's actual behavior deviates from
what one might expect from the specified behavior of the web platform.


### Objects

SpiderMonkey objects usually use less memory than the naïve "table of properties
with attributes" model would suggest. For example, it is typical for many
objects to have identical sets of properties, with only the properties' values
varying from one object to the next. To take advantage of this regularity,
SpiderMonkey objects with identical sets of properties may share their property
metadata; only property values are stored directly in the object.

Array objects may also be optimized, if the set of live indices is dense.


### Strings

SpiderMonkey has three representations of strings:

- Normal: the string's text is counted in its size.

- Substring: the string is a substring of some other string, and points to that
  string for its storage. This representation may result in a small string
  retaining a very large string. However, the memory consumed by the string
  itself is a small constant independent of its size, since it is simply a
  reference to the larger string, a start position, and a length.

- Concatenations: When asked to concatenate two strings, SpiderMonkey may elect
  to delay copying the strings' data, and represent the result simply as a
  pointer to the two original strings. Again, such a string retains other
  strings, but the memory consumed by the string itself is a small constant
  independent of its size, since it is simply a pair of pointers.

SpiderMonkey converts strings from the more complex representations to the
simpler ones when it pleases. Such conversions usually increase memory
consumption.

SpiderMonkey shares some strings amongst all web pages and browser JS. These
shared strings, called *atoms*, are not included in censuses' string counts.


### Scripts

SpiderMonkey has a complex, hybrid representation of JavaScript code. There
are four representations kept in memory:

- _Source code_. SpiderMonkey retains a copy of most JavaScript source code.

- _Compressed source code_. SpiderMonkey compresses JavaScript source code,
  and de-compresses it on demand. Heuristics determine how long to retain the
  uncompressed code.

- _Bytecode_. This is SpiderMonkey's parsed representation of JavaScript.
  Bytecode can be interpreted directly, or used as input to a just-in-time
  compiler. Source is parsed into bytecode on demand; functions that are never
  called are never parsed.

- _Machine code_. SpiderMonkey includes several just-in-time compilers, each of
  which translates JavaScript source or bytecode to machine code. Heuristics
  determine which code to compile, and which compiler to use. Machine code may
  be dropped in response to memory pressure, and regenerated as needed.

Furthermore, SpiderMonkey's just-in-time compilers generate inline caches for
type specialization. This information is dropped periodically to reduce memory
usage.

In a census, all the various forms of JavaScript code are placed in the
`"scripts"` category.


[debugger]: Debugger-API.md
[debugger-object]: Debugger.md
[tracking-allocs]: #trackingallocationsites
[bernoulli-trial]: https://en.wikipedia.org/wiki/Bernoulli_trial
[alloc-sampling-probability]: #allocsamplingprobability
[object]: Debugger.Object.md
[allocation-site]: Debugger.Object.html#allocationsite
[saved-frame]: ../SavedFrame/index
[drain-alloc-log]: #drainAllocationsLog
[max-alloc-log]: #maxAllocationsLogLength
[take-census]: #takecensus-options
[timestamps]: ./Conventions.html#timestamps
