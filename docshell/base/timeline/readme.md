
#Timeline

The files in this directory are concerned with providing the backend platform features required for the developer tools interested in tracking down operations done in Gecko. The mechanism we use to define these operations are `markers`.

Examples of traced operations include:

* Style Recalculation
* Layout
* Painting
* JavaScript run-to-completion
* HTML parsing
* etc.

The traced operations are displayed in the DevTools Performance tool's timeline.

This is an overview of how everything works and can be extended.

##MarkersStorage
A `MarkersStorage` is an abstract class defining a place where timeline markers may be held. It defines an interface with pure virtual functions to highlight how this storage can be interacted with:

- `AddMarker`: adding a marker, from the main thread only
- `AddOTMTMarker`: adding a marker off the main thread only
- `ClearMarkers`: clearing all accumulated markers (both from the main thread and off it)
- `PopMarkers`: popping all accumulated markers (both from the main thread and off it).

Note on why we handle on/off the main thread markers separately: since most of our markers will come from the main thread, we can be a little more efficient and avoid dealing with multithreading scenarios until all the markers are actually cleared or popped in `ClearMarkers` or `PopMarkers`. Main thread markers may only be added via `AddMarker`, while off the main thread markers may only be added via `AddOTMTMarker`. Clearing and popping markers will yield until all operations involving off the main thread markers finish. When popping, the markers accumulated off the main thread will be moved over. We expect popping to be fairly infrequent (every few hundred milliseconds, currently we schedule this to happen every 200ms).

##ObservedDocShell
The only implementation of a MarkersStorage we have right now is an `ObservedDocShell`.

Instances of `ObservedDocShell` accumulate markers that are *mostly* about a particular docshell. At a high level, for example, an `ObservedDocshell` would be created when a timeline tool is opened on a page. It is reasonable to assume that most operations which are interesting for that particular page happen on the main thread. However certain operations may happen outside of it, yet are interesting for its developers, for which markers can be created as well (e.g. web audio stuff, service workers etc.). It is also reasonable to assume that a docshell may sometimes not be easily accessible from certain parts of the platform code, but for which markers still need to be created.

Therefore, the following scenarios arise:

- a). creating a marker on the main thread about a particular dochsell

- b). creating a marker on the main thread without pinpointing to an affected docshell (unlikely, but allowed; in this case such a marker would have to be stored in all currently existing `ObservedDocShell` instances)

- c). creating a marker off the main thread about a particular dochsell (impossible; docshells can't be referenced outside the main thread, in which case some other type of identification mechanism needs to be put in place).

- d). creating a marker off the main thread without pinpointing to a particular docshell (same path as c. here, such a marker would have to be stored in all currently existing `ObservedDocShell` instances).

An observed docshell (in other words, "a docshell for which a timeline tool was opened") can thus receive both main thread and off the main thread markers.

Cross-process markers are unnecessary at the moment, but tracked in bug 1200120.

##TimelineConsumers
A `TimelineConsumer` is a singleton that facilitates access to `ObservedDocShell` instances. This is where a docshell can register/unregister itself as being observed via the `AddConsumer` and `RemoveConsumer` methods.

All markers may only be stored via this singleton. Certain helper methods are available:

* Main thread only
`AddMarkerForDocShell(nsDocShell*, const char*, MarkerTracingType)`
`AddMarkerForDocShell(nsDocShell*, const char*, const TimeStamp&, MarkerTracingType)`
`AddMarkerForDocShell(nsDocShell*, UniquePtr<AbstractTimelineMarker>&&)`

* Any thread
`AddMarkerForAllObservedDocShells(const char*, MarkerTracingType)`
`AddMarkerForAllObservedDocShells(const char*, const TimeStamp&, MarkerTracingType)`
`AddMarkerForAllObservedDocShells(UniquePtr<AbstractTimelineMarker>&)`

The "main thread only" methods deal with point a). described above. The "any thread" methods deal with points b). and d).

##AbstractTimelineMarker

All markers inherit from this abstract class, providing a simple thread-safe extendable blueprint.

Markers are readonly after instantiation, and will always be identified by a name, a timestamp and their tracing type (`START`, `END`, `TIMESTAMP`). It *should not* make sense to modify their data after their creation.

There are only two accessible constructors:
`AbstractTimelineMarker(const char*, MarkerTracingType)`
`AbstractTimelineMarker(const char*, const TimeStamp&, MarkerTracingType)`
which create a marker with a name and a tracing type. If unspecified, the corresponding timestamp will be the current instantiation time. Instantiating a marker *much later* after a particular operation is possible, but be careful providing the correct timestamp.

The `AddDetails` virtual method should be implemented by subclasses when creating WebIDL versions of these markers, which will be sent over to a JavaScript frontend.

##TimelineMarker
A `TimelineMarker` is the main `AbstractTimelineMarker` implementation. They allow attaching a JavaScript stack on `START` and `TIMESTAMP` markers.

These markers will be created when using the `TimelineConsumers` helper methods which take in a string, a tracing type and (optionally) a timestamp. For more complex markers, subclasses are encouraged. See `EventTimelineMarker` or `ConsoleTimelineMarker` for some examples.

##RAII

### mozilla::AutoTimelineMarker

The easiest way to trace Gecko events/tasks with start and end timeline markers is to use the `mozilla::AutoTimelineMarker` RAII class. It automatically adds the start marker on construction, and adds the end marker on destruction. Don't worry too much about potential performance impact! It only actually adds the markers when the given docshell is being observed by a timeline consumer, so essentially nothing will happen if a tool to inspect those markers isn't specifically open.

This class may only be used on the main thread, and pointer to a docshell is necessary. If the docshell is a nullptr, nothing happens and this operation fails silently.

Example: `AutoTimelineMarker marker(aTargetNode->OwnerDoc()->GetDocShell(), "Parse HTML");`

### mozilla::AutoGlobalTimelineMarker`

Similar to the previous RAII class, but doesn't expect a specific docshell, and the marker will be visible in all timeline consumers. This is useful for generic operations that don't involve a particular dochsell, or where a docshell isn't accessible. May also only be used on the main thread.

Example: `AutoGlobalTimelineMarker marker("Some global operation");`
