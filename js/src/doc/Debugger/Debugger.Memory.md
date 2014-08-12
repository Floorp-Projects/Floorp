Debugger.Memory
===============

`Debugger` can survey the debuggee's memory usage in various ways:

- It can find the path by which a given item is referenced from any debuggee
  global.

- It can compute the *retained size* of an item: the total size of all
  items that are reachable only through that item&mdash;and thus would be
  freed if the given item were freed.

- It can compute a *census* of items in debuggee compartments, categorizing
  items in various ways, yielding item counts, storage consumed, and "top
  N" item lists for each category.

In this documentation, the term *item* refers to anything that occupies memory
in the browser, whether it implements something that web developers would
recognize (JavaScript objects; DOM nodes) or an implementation detail that web
developers would have no reason to be familiar with (SpiderMonkey shapes and
types).

If `dbg` is a `Debugger` instance, then `dbg.memory` is an instance of
`Debugger.Memory` whose methods and accessors operate on `dbg`. This class
exists only to hold member functions and accessors related to memory analysis,
keeping them separate from other `Debugger` facilities.


Allocation Site Tracking
------------------------

<code id="trackingallocationsites">Debugger.Memory.prototype.trackingAllocationSites</code>
:   A boolean value indicating whether this `Debugger.Memory` instance is
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


Censuses
--------

A *census* is a complete traversal of the graph of all reachable items belonging
to a particular `Debugger`'s debuggee compartments, producing a count of those
items, broken down by various criteria. `Debugger` can perform a census as part
of the same heap traversal that computes [retained sizes][retsz], or as an
independent operation.

Because performing a census requires traversing the entire graph of objects in
debuggee compartments, it is an expensive operation. On developer hardware in
2014, traversing a memory graph containing roughly 130,000 nodes and 410,000
edges took roughly 100ms. The traversal itself temporarily allocates one hash
table entry per node (roughly two address-sized words) in addition to the
per-category counts, whose size depends on the number of categories.

`Debugger.Memory.prototype.takeCensus()`
:   Carry out a census of the debuggee compartments' contents. Return an
    object of the form:

    <pre class='language-js'><code>
    {
      "objects": { <i>class</i>: <i>tally</i>, ... },
      "scripts": <i>tally</i>,
      "strings": <i>tally</i>,
      "other": { <i>type name</i>: <i>tally</i>, ... }
    }
    </code></pre>

    Each <i>tally</i> has the form:

    <pre class='language-js'><code>
    { "count": <i>count</i> }
    </code></pre>

    where <i>count</i> is the number of items in the category.

    The `"objects"` property's value contains the tallies of JavaScript objects,
    broken down by their ECMAScript `[[Class]]` internal property values. Each
    <i>class</i> is a string.

    The `"scripts"` property's value tallies the in-memory representation of
    JavaScript code.

    The `"strings"` property's value tallies the debuggee's strings.

    The `"other"` property's value contains the tallies of other items used
    internally by SpiderMonkey, broken down by their C++ type name.


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
shared strings, called *atoms*, are not included in censuses' string tallies.


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

Furthermore, SpiderMonkey tracks which types of values have appeared in
variables and object properties. This type information can be large.

In a census, all the various forms of JavaScript code are placed in the
`"script"` category. Type information is accounted to the `"types"` category.

`maxAllocationsLogLength`
:   The maximum number of allocation sites to accumulate in the allocations log
    at a time. Its default value is `5000`.

## Function Properties of the `Debugger.Memory.prototype` Object

`drainAllocationsLog()`
:   When `trackingAllocationSites` is `true`, this method returns an array of
    allocation sites (as [captured stacks][saved-frame]) for recent `Object`
    allocations within the set of debuggees. *Recent* is defined as the
    `maxAllocationsLogLength` most recent `Object` allocations since the last
    call to `drainAllocationsLog`. Therefore, calling this method effectively
    clears the log.

    When `trackingAllocationSites` is `false`, `drainAllocationsLog()` throws an
    `Error`.
