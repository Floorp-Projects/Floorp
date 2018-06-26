# How actors are organized

To start with, actors are living within devtools/server/actors folder.
They are organized in a hierarchy for easier lifecycle and memory management:
once a parent is removed from the pool, its children are removed as well.
(See actor-registration.md for more information about how to implement one)

The overall hierarchy of actors looks like this:

```
RootActor: First one, automatically instantiated when we start connecting.
   |       Mostly meant to instantiate new actors.
   |
   |-- Global-scoped actors:
   |   Actors exposing features related to the main process, that are not
   |   specific to any particular target (document, tab, add-on, or worker).
   |   These actors are registered with `global: true` in
   |   devtools/server/main.js.
   |   Examples include:
   |   PreferenceActor (for Firefox prefs)
   |
   \-- Target actors:
       Actors that represent the main "thing" being targeted by a given toolbox,
       such as a tab, frame, worker, add-on, etc. and track its lifetime.
       Generally, there is a target actor for each thing you can point a
       toolbox at.
       Examples include:
       FrameTargetActor (for a frame, such as a tab)
       WorkerTargetActor (for various kind of workers)
       |
       \-- Target-scoped actors:
           Actors exposing one particular feature set. They are children of a
           given target actor and the data they return is filtered to reflect
           the target.
           These actors are registered with `target: true` in
           devtools/server/main.js.
           Examples include:
           WebConsoleActor
           InspectorActor
           These actors may extend this hierarchy by having their own children,
           like LongStringActor, WalkerActor, etc.
```

## RootActor

The root actor is special. It is automatically created when a client connects.
It has a special `actorID` which is unique and is "root".
All other actors have an `actorID` which is computed dynamically,
so that you need to ask an existing actor to create an Actor
and returns its `actorID`. That's the main role of RootActor.

```
RootActor (root.js)
   |
   |-- FrameTargetActorProxy (frame-proxy.js)
   |   Targets frames (such as a tab) living in the parent or child process.
   |   Note that this is just a proxy for FrameTargetActor, which is loaded via
   |   the frame's message manager as a frame script in the process containing
   |   the frame content. This proxy via message manager is always used, even
   |   when the content happens to be in the same process.
   |   Returned by "listTabs" or "getTab" requests.
   |   |
   |   \-- FrameTargetActor (frame.js)
   |       The "real" target actor for a frame (such as a tab) which runs in
   |       whichever process holds the content. FrameTargetActorProxy
   |       communicates with this via the frame's message manager.
   |       Extends the abstract class BrowsingContextTargetActor.
   |       Returned by "connect" server method on FrameTargetActorProxy.
   |
   |-- WorkerTargetActor (worker.js)
   |   Targets a worker (applies to various kinds like web worker, service
   |   worker, etc.).
   |   Returned by "listWorkers" request to the root actor to get all workers.
   |   Returned by "listWorkers" request to a FrameTargetActorProxy to get
   |   workers for a specific frame.
   |   Returned by "listWorkers" request to a ContentProcessTargetActor to get
   |   workers for the chrome of the child process.
   |
   |-- ChromeWindowTargetActor (chrome-window.js)
   |   Targets a single window, such as a browser window in Firefox, but it can
   |   be used to reach any window in the parent process.
   |   Extends the abstract class BrowsingContextTargetActor.
   |   Returned by "getWindow" request to the root actor.
   |
   |-- ParentProcessTargetActor (parent-process.js)
   |   Targets all resources in the parent process of Firefox (chrome documents,
   |   JSMs, JS XPCOM, etc.).
   |   Extends the abstract class BrowsingContextTargetActor.
   |   Extended by WebExtensionTargetActor.
   |   Returned by "getProcess" request without any argument.
   |
   |-- ContentProcessTargetActor (content-process.js)
   |   Targets all resources in a content process of Firefox (chrome sandboxes,
   |   frame scripts, documents, etc.)
   |   Returned by "getProcess" request with a id argument, matching the
   |   targeted process.
   |
   |-- AddonTargetActor (addon.js)
   |   Targets a legacy (non-WebExtension) add-on.
   |   Returned by "listAddons" request.
   |
   \-- WebExtensionActor (addon/webextension.js)
       Represents a WebExtension add-on in the parent process. This gives some
       metadata about the add-on and watches for uninstall events. This uses a
       proxy to access the actual WebExtension in the WebExtension process via
       the message manager.
       Returned by "listAddons" request.
       |
       \-- WebExtensionTargetActor (targets/webextension.js)
           Targets a WebExtension add-on. This runs in the WebExtension process.
           The client issues an additional "connect" request to
           WebExtensionActor to get this actor, which is different from the
           approach used for frame target actors.
           Extends ParentProcessTargetActor.
           Returned by "connect" request to WebExtensionActor.
```

## Target Actors

Those are the actors exposed by the root actors which are meant to track the
lifetime of a given target: tab, process, add-on, or worker. It also allows to
fetch the target-scoped actors connected to this target, which are actors like
console, inspector, thread (for debugger), style inspector, etc.

Some target actors inherit from BrowsingContextTargetActor (defined in
browsing-context.js) which is meant for "browsing contexts" which present
documents to the user. It automatically tracks the lifetime of the targeted
browsing context, but it also tracks its iframes and allows switching the
target to one of its iframes.

For historical reasons, target actors also handle creating the ThreadActor, used
to manage breakpoints in the debugger. Actors inheriting from
BrowsingContextTargetActor expose `attach`/`detach` requests, that allows to
start/stop the ThreadActor.

Target-scoped actors are accessed via the target actor's RDP form which contains
the `actorID` for each target-scoped actor.

The target-scoped actors expect to find the following properties on the target
actor:
 - threadActor:
   ThreadActor instance for the given target,
   only defined once `attach` request is called, or on construction.
 - isRootActor: (historical name)
   Always false, except on ParentProcessTargetActor.
   Despite the attribute name, it is being used to accept all resources
   (like chrome one) instead of limiting only to content resources.
 - makeDebugger:
   Helper function used to create Debugger object for the target.
   (See actors/utils/make-debugger.js for more info)

In addition to this, the actors inheriting from BrowsingContextTargetActor,
expose many other attributes and events:
 - window:
   Reference to the window global object currently targeted.
   It can change over time if we switch target to an iframe, so it
   shouldn't be stored in a variable, but always retrieved from the actor.
 - windows:
   List of all document globals including the main window object and all
   iframes.
 - docShell:
   Primary docShell reference for the targeted document.
 - docShells:
   List of all docShells for the targeted document and all its iframes.
 - chromeEventHandler:
   The chrome event handler for the current target. Allows to listen to events
   that can be missing/cancelled on this document itself.

See BrowsingContextTargetActor documentation for more details.

## Target-scoped actors

Each of these actors focuses on providing one particular feature set. They are
children of a given target actor.

The data they return is filtered to reflect the target. For example, the
InspectorActor that you fetch from a FrameTargetActor gives you information
about the markup and styles for only that frame.

These actors may extend this hierarchy by having their own children, like
LongStringActor, WalkerActor, etc.

To improve performance, target-scoped actors are created lazily. The target
actor lists the actor ID for each one, but the actor modules aren't actually
loaded and instantiated at that point. Once the first request for a given
target-scoped actor is received by the server, that specific actor is
instantiated just in time to service the request.
