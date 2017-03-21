# Memory Tool Architecture

The memory tool is built of three main elements:

1. The live heap graph exists in memory, and is managed by the C++ allocator and
   garbage collector. In order to get access to the structure of this graph, a
   specialized interface is created to represent its state. The `JS::ubi::Node`
   is the basis for this representation. This interface can be created from the
   live heap graph, or a serialized, offline snapshot from a previous moment in
   time. Our various heap analyses (census, dominator trees, shortest paths,
   etc) run on top of `JS::ubi::Node` graphs. The `ubi` in the name stands for
   "ubiquitous" and provides a namespace for memory analyses in C++ code.

2. The `HeapAnalysesWorker` runs in a worker thread, performing analyses on
   snapshots and translating the results into something the frontend can render
   simply and quickly. The `HeapAnalysesClient` is used to communicate between
   the worker and the main thread.

3. Finally, the last element is the frontend that renders data received from the
   `HeapAnalysesClient` to the DOM and translates user input into requests for
   new data with the `HeapAnalysesClient`.

Unlike other tools (such as the JavaScript debugger), the memory tool makes very
little use of the Remote Debugger Server and the actors that reside in it. Use
of the [`MemoryActor`](devtools/server/actors/memory.js) is limited to toggling
allocation stack recording on and off, and transferring heap snapshots from the
debuggee (which is on the server) to the `HeapAnalysesWorker` (which is on the
client). A nice benefit that naturally emerges, is that supporting "legacy"
servers (eg, using Firefox Developer Edition as a client to remote debug a
release Firefox for Android server) is a no-op. As we add new analyses, we can
run them on snapshots taken on old servers no problem. The only requirement is
that changes to the snapshot format itself remain backwards compatible.

## `JS::ubi::Node`

`JS::ubi::Node` is a lightweight serializable interface that can represent the
current state of the heap graph. For a deeper dive into the particulars of how
it works, it is very well documented in the `js/public/UbiNode.h`

A "heap snapshot" is a representation of the heap graph at some particular past
instance in time.

A "heap analysis" is an algorithm that runs on a `JS::ubi::Node` heap graph.
Generally, analyses can run on either the live heap graph or a deserialized
snapshot. Example analyses include "census", which aggregates and counts nodes
into various user-specified buckets; "dominator trees", which compute the
[dominates](https://en.wikipedia.org/wiki/Dominator_%28graph_theory%29) relation
and retained size for all nodes in the heap graph; and "shortest paths" which
finds the shortest paths from the GC roots to some subset of nodes.

### Saving Heap Snapshots

Saving a heap snapshot has a few requirements:

1. The binary format must remain backwards compatible and future extensible.

2. The live heap graph must not mutate while we are in the process of
   serializing it.

3. The act of saving a heap snapshot should impose as little memory overhead as
   possible. If we are taking a snapshot to debug frequent out-of-memory errors,
   we don't want to trigger an OOM ourselves!

To solve (1), we use the [protobuf](https://developers.google.com/protocol-buffers/)
message format. The message definitions themselves are in
`devtools/shared/heapsnapshot/CoreDump.proto`. We always use `optional` fields
so we can change our mind about what fields are required sometime in the future.
Deserialization checks the semantic integrity of deserialized protobuf messages.

For (2), we rely on SpiderMonkey's GC rooting hazard static analysis and the
`AutoCheckCannotGC` dynamic analysis to ensure that neither JS nor GC runs and
modifies objects or moves them from one address in memory to another. There is
no equivalent suppression and static analysis technique for the
[cycle collector](https://developer.mozilla.org/en/docs/Interfacing_with_the_XPCOM_cycle_collector),
so care must be taken not to invoke methods that could start cycle collection or
mutate the heap graph from the cycle collector's perspective. At the time of
writing, we don't yet support saving the cycle collector's portion of the heap
graph in snapshots, but that work is deemed Very Important and Very High
Priority.

Finally, (3) imposes upon us that we do not build the serialized heap snapshot
binary blob in memory, but instead stream it out to disk while generating it.

Once all of that is accounted for, saving snapshots becomes pretty straight
forward. We traverse the live heap graph with `JS::ubi::Node` and
`JS::ubi::BreadthFirst`, create a protobuf message for each node and each node's
edges, and write these messages to disk before continuing the traversal to the
next node.

This functionality is exposed to chrome JavaScript as the
`[ThreadSafe]ChromeUtils.saveHeapSnapshot` function. See
`dom/webidl/ThreadSafeChromeUtils.webidl` for API documentation.

### Reading Heap Snapshots

Reading heap snapshots has less restrictions than saving heap snapshots. The
protobuf messages that make up the core dump are deserialized one by one, stored
as a set of `DeserializedNode`s and a set of `DeserializedEdge`s, and the result
is a `HeapSnapshot` instance.

The `DeserializedNode` and `DeserializedEdge` classes implement the
`JS::ubi::Node` interface. Analyses running on offline heap snapshots rather
than the live heap graph operate on these classes (unknowingly, of course).

For more details, see the
[`mozilla::devtools::HeapSnapshot`](devtools/shared/heapsnapshot/HeapSnapshot.cpp)
and
[`mozilla::devtools::Deserialized{Node,Edge}`](devtools/shared/heapsnapshot/DeserializedNode.h)
classes.

### Heap Analyses

Heap analyses operate on `JS::ubi::Node` graphs without knowledge of whether
that graph is backed by the live heap graph or an offline heap snapshot. They
must make sure never to allocate GC things or modify the live heap graph.

In general, analyses are implemented in their own `js/public/Ubi{AnalysisName}.h`
header (eg `js/public/UbiCensus.h`), and are exposed to chrome JavaScript code
via a method on the [`HeapSnapshot`](dom/webidl/HeapSnapshot.webidl) webidl
interface.

For each analysis we expose to chrome JavaScript on the `HeapSnapshot` webidl
interface, there is a small amount of glue code in Gecko. The
[`mozilla::devtools::HeapSnapshot`](devtools/shared/heapsnapshot/HeapSnapshot.h)
C++ class implements the webidl interface. The analyses methods (eg
`ComputeDominatorTree`) take the deserialized nodes and edges from the heap
snapshot, create `JS::ubi::Node`s from them, call the analyses from
`js/public/Ubi*.h`, and wrap the results in something that can be represented in
JavaScript.

For API documentation on running specific analyses, see the
[`HeapSnapshot`](dom/webidl/HeapSnapshot.webidl) webidl interface.

### Testing `JS::ubi::Node`, Snapshots, and Analyses

The majority of the tests reside within `devtools/shared/heapsnapshot/tests/**`.
For reading and saving heap snapshots, most tests are gtests. The gtests can be
run with the `mach gtest DevTools.*` command. The rest are integration sanity
tests to make sure we can read and save snapshots in various environments, such
as xpcshell or workers. These can be run with the usual `mach test $PATH`
commands.

There are also `JS::ubi::Node` related unit tests in
`js/src/jit-test/tests/heap-analysis/*`, `js/src/jit-test/tests/debug/Memory-*`,
and `js/src/jsapi-tests/testUbiNode.cpp`. See
https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey/Running_Automated_JavaScript_Tests#Running_jit-tests
for running the JIT tests.

## `HeapAnalysesWorker`

The `HeapAnalysesWorker` orchestrates running specific analyses on snapshots and
transforming the results into something that can simply and quickly be rendered
by the frontend. The analyses can take some time to run (sometimes on the order
of seconds), so doing them in a worker thread allows the interface to stay
responsive. The `HeapAnalysisClient` provides the main thread's interface to the
worker.

The `HeapAnalysesWorker` doesn't actually do much itself; mostly just shuffling
data and transforming it from one representation to another or calling C++
utility functions exposed by webidl that do those things. Most of these are
implemented as traversals of the resulting census or dominator trees.

See the following files for details on the various data transformations and
shuffling that the `HeapAnalysesWorker` delegates to.

* `devtools/shared/heapsnapshot/CensusUtils.js`
* `devtools/shared/heapsnapshot/CensusTreeNode.js`
* `devtools/shared/heapsnapshot/DominatorTreeNode.js`

### Testing the `HeapAnalysesWorker` and `HeapAnalysesClient`

Tests for the `HeapAnalysesWorker` and `HeapAnalysesClient` reside in
`devtools/shared/heapsnapshot/tests/**` and can be run with the usual `mach test
$PATH` command.

## Frontend

The frontend of the memory tool is built with React and Redux.

[React has thorough documentation.](https://facebook.github.io/react/)

[Redux has thorough documentation.](http://rackt.org/redux/index.html)

We have React components in `devtools/client/memory/components/*`.

We have Redux reducers in `devtools/client/memory/reducers/*`.

We have Redux actions and action-creating tasks in
`devtools/client/memory/actions/*`.

React components should be pure functions from their props to the rendered
(virtual) DOM. Redux reducers should also be observably pure.

Impurity within the frontend is confined to the tasks that are creating and
dispatching actions. All communication with the outside world (such as the
`HeapAnalysesWorker`, the Remote Debugger Server, or the file system) is
restricted to within these tasks.

### Snapshots State

On the JavaScript side, the snapshots represent a reference to the underlying
heap dump and the various analyses. The following diagram represents a finite
state machine describing the snapshot states. Any of these states may go to the
ERROR state, from which they can never leave.

```
SAVING → SAVED → READING → READ
                  ↗
         IMPORTING
```

Each of the report types (census, diffing, tree maps, dominators) have their own states as well, and are documented at `devtools/client/memory/constants.js`.
These report states are updated as the various filtering and selecting options
are updated in the UI.

### Testing the Frontend

Unit tests for React components are in `devtools/client/memory/test/chrome/*`.

Unit tests for actions, reducers, and state changes are in
`devtools/client/memory/test/unit/*`.

Holistic integration tests for the frontend and the whole memory tool are in
`devtools/client/memory/test/browser/*`.

All tests can be run with the usual `mach test $PATH` command.
