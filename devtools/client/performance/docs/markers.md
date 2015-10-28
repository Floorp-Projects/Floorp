# Timeline Markers

## Common

* DOMHighResTimeStamp start
* DOMHighResTimeStamp end
* DOMString name
* object? stack
* object? endStack
* unsigned short processType;
* boolean isOffMainThread;

The `processType` a GeckoProcessType enum listed in xpcom/build/nsXULAppAPI.h,
specifying if this marker originates in a content, chrome, plugin etc. process.
Additionally, markers may be created from any thread on those processes, and
`isOffMainThread` highights whether or not they're from the main thread. The most
common type of marker is probably going to be from a GeckoProcessType_Content's
main thread when debugging content.

## DOMEvent

Triggered when a DOM event occurs, like a click or a keypress.

* unsigned short eventPhase - a number indicating what phase this event is
  in (target, bubbling, capturing, maps to Ci.nsIDOMEvent constants)
* DOMString type - the type of event, like "keypress" or "click"

## Reflow

Reflow markers (labeled as "Layout") indicate when a change has occurred to
a DOM element's positioning that requires the frame tree (rendering
representation of the DOM) to figure out the new position of a handful of
elements. Fired via `PresShell::DoReflow`

## Paint

* sequence<{ long height, long width, long x, long y }> rectangles - An array
  of rectangle objects indicating where painting has occurred.

## Styles

Style markers (labeled as "Recalculating Styles") are triggered when Gecko
needs to figure out the computational style of an element. Fired via
`RestyleTracker::DoProcessRestyles` when there are elements to restyle.

* DOMString restyleHint - A string indicating what kind of restyling will need
  to be processed; for example "eRestyle_StyleAttribute" is relatively cheap,
  whereas "eRestyle_Subtree" is more expensive. The hint can be a string of
  any amount of the following, separated via " | ". All future restyleHints
  are from `RestyleManager::RestyleHintToString`.

  * "eRestyle_Self"
  * "eRestyle_Subtree"
  * "eRestyle_LaterSiblings"
  * "eRestyle_CSSTransitions"
  * "eRestyle_CSSAnimations"
  * "eRestyle_SVGAttrAnimations"
  * "eRestyle_StyleAttribute"
  * "eRestyle_StyleAttribute_Animations"
  * "eRestyle_Force"
  * "eRestyle_ForceDescendants"


## Javascript

`Javascript` markers are emitted indicating when JS execution begins and ends,
with a reason that triggered it (causeName), like a requestAnimationFrame or
a setTimeout.

* string causeName - The reason that JS was entered. There are many possible
  reasons, and the interesting ones to show web developers (triggered by content) are:

  * "\<script\> element"
  * "EventListener.handleEvent"
  * "setInterval handler"
  * "setTimeout handler"
  * "FrameRequestCallback"
  * "EventHandlerNonNull"
  * "promise callback"
  * "promise initializer"
  * "Worker runnable"

  There are also many more potential JS causes, some which are just internally
  used and won't emit a marker, but the below ones are only of interest to
  Gecko hackers, most likely

  * "promise thenable"
  * "worker runnable"
  * "nsHTTPIndex set HTTPIndex property"
  * "XPCWrappedJS method call"
  * "nsHTTPIndex OnFTPControlLog"
  * "XPCWrappedJS QueryInterface"
  * "xpcshell argument processing”
  * "XPConnect sandbox evaluation "
  * "component loader report global"
  * "component loader load module"
  * "Cross-Process Object Wrapper call/construct"
  * "Cross-Process Object Wrapper ’set'"
  * "Cross-Process Object Wrapper 'get'"
  * "nsXULTemplateBuilder creation"
  * "TestShellCommand"
  * "precompiled XUL \<script\> element"
  * "XBL \<field\> initialization "
  * "NPAPI NPN_evaluate"
  * "NPAPI get"
  * "NPAPI set"
  * "NPAPI doInvoke"
  * "javascript: URI"
  * "geolocation.always_precise indexing"
  * "geolocation.app_settings enumeration"
  * "WebIDL dictionary creation"
  * "XBL \<constructor\>/\<destructor\> invocation"
  * "message manager script load"
  * "message handler script load"
  * "nsGlobalWindow report new global"

## GarbageCollection

Emitted after a full GC cycle has completed (which is after any number of
incremental slices).

* DOMString causeName - The reason for a GC event to occur. A full list of
  GC reasons can be found [on MDN](https://developer.mozilla.org/en-US/docs/Tools/Debugger-API/Debugger.Memory#Debugger.Memory_Handler_Functions).
* DOMString nonincremenetalReason - If the GC could not do an incremental
  GC (smaller, quick GC events), and we have to walk the entire heap and
  GC everything marked, then the reason listed here is why.

## nsCycleCollector::Collect

An `nsCycleCollector::Collect` marker is emitted for each incremental cycle
collection slice and each non-incremental cycle collection.

# nsCycleCollector::ForgetSkippable

`nsCycleCollector::ForgetSkippable` is presented as "Cycle Collection", but in
reality it is preparation/pre-optimization for cycle collection, and not the
actual tracing of edges and collecting of cycles.

## ConsoleTime

A marker generated via `console.time()` and `console.timeEnd()`.

* DOMString causeName - the label passed into `console.time(label)` and
  `console.timeEnd(label)` if passed in.

## TimeStamp

A marker generated via `console.timeStamp(label)`.

* DOMString causeName - the label passed into `console.timeStamp(label)`
  if passed in.

## document::DOMContentLoaded

A marker generated when the DOMContentLoaded event is fired.

## document::Load

A marker generated when the document's "load" event is fired.

## Parse HTML

## Parse XML

## Worker

Emitted whenever there's an operation dealing with Workers (any kind of worker,
Web Workers, Service Workers etc.). Currently there are 4 types of operations
being tracked: serializing/deserializing data on the main thread, and also
serializing/deserializing data off the main thread.

* ProfileTimelineWorkerOperationType operationType - the type of operation
  being done by the Worker or the main thread when dealing with workers.
  Can be one of the following enums defined in ProfileTimelineMarker.webidl
  * "serializeDataOffMainThread"
  * "serializeDataOnMainThread"
  * "deserializeDataOffMainThread"
  * "deserializeDataOnMainThread"

## Composite

Composite markers trace the actual time an inner composite operation
took on the compositor thread. Currently, these markers are only especially
interesting for Gecko platform developers, and thus disabled by default.

## CompositeForwardTransaction

Markers generated when the IPC request was made to the compositor from
the child process's main thread.
