# How actors are organized

tl;dr; The Root actor exposes Descriptor actors which describe a debuggable context.
The Descriptor then exposes a dedicated Watcher actor.
The Watcher actor notifies about many target actors (one per smaller debuggable context: WindowGlobal, worker,...)
and also notifies about all resources (console messages, sources, network events,...).

To start with, actors are living within [devtools/server/actors][actors-folder] folder.
They are organized in a hierarchy for easier lifecycle and memory management:
once a parent is removed from the pool, its children are removed as well.
(See [actor-registration.md](actor-registration.md) for more information about how to implement a new one)

The overall hierarchy of actors looks like this:

```
 ###  RootActor:
  |
  |   First one, automatically instantiated when we start connecting
  |   with a fixed actor ID set to "root".
  |
  |   It is mostly meant to instantiate all the following actors.
  |
  |-- Global-scoped actors:
  |
  |   Actors exposing features related to the main process, that are not
  |   specific to any particular target (document, tab, add-on, or worker).
  |   These actors are registered with `global: true` in:
  |     devtools/server/actors/utils/actor-registry.js
  |
  |   Examples:
  |     * PreferenceActor (for Firefox prefs)
  |     * DeviceActor (for fetching and toggling runtime specifics)
  |     * PerfActor (used by the profiler.firefox.com profiler)
  |     * ...
  |
  \-- Descriptor actors
   |
   |  These actors expose information about a particular context that DevTools can focus on:
   |    * a tab (TabDescriptor)
   |    * a (service/shared/chrome) worker (WorkerDescriptor)
   |    * a Web Extension (WebExtensionDescriptor)
   |    * the browser (ProcessDescriptor)
   |
   |   This actor is mostly informative. It exposes a WatcherActor to actually debug this context.
   |   This is typically used by about:debugging to display all available debuggable contexts.
   |
   \ -- WatcherActor
     |
     |   Each descriptor actor exposes a dedicated Watcher actor instance.
     |   This actor allows to observe target actors and resources.
     |
     \ -- Target actors:
       |
       |  Within a given descriptor/watcher context, targets represents fine grained debuggable contexts.
       |  While the descriptor and watcher stays alive until the devtools are closed or the debuggable context
       |  was close (tab closed, worker destroyed or browser closed),
       |  the targets have a shorter lifecycle and a narrowed scope.
       |
       |  The typical target actors are WindowGlobal target actors.
       |  This represents one specific WindowGlobal. A tab is typically made of many of them.
       |  Each iframe will have its dedicated WindowGlobal.
       |  Each navigation will also spawn a new dedicated WindowGlobal.
       |  When debugging an add-on, you will have one WindowGlobal target for the background page,
       |  one for each popup, ...
       |
       |  The other targets actors are:
       |    * worker targets
       |    * process targets (only used in the Browser Toolbox to debug the browser itself made of many processes)
       |
       \ -- Target-scoped actors:

            Actors exposing one particular feature set. They are children of a
            given target actor and the data they return is filtered to reflect
            the target.
            These actors are registered with `target: true` in
              devtools/server/actors/utils/actor-registry.js

            Examples:
              * WebConsoleActor (for evaluating javascript)
              * InspectorActor (to observe and modify the DOM Elements)
              * ThreadActor (for setting breakpoints and pause/step/resume)
              * StorageActor (for managing storage data)
              * AccessibilityActor (to observe accessibility information)

            These actors may extend this hierarchy by having their own children,
            like LongStringActor, WalkerActor, SourceActor, NodeActor, etc.
```

## RootActor

The root actor is special. It is automatically created when a client connects.
It has a special `actorID` which is unique and is "root".
All other actors have an `actorID` which is computed dynamically,
so that you need to ask an existing actor to create an Actor
and returns its `actorID`.
That's the main role of RootActor. It will expose all the descriptor actors,
which is only the start of spawning the watcher, target and target-scoped actors.

```
RootActor (root.js)
   |
   |-- TabDescriptorActor (descriptors/tab.js)
   |
   |   Designates tabs running in the parent or child process.
   |
   |   Returned by "listTabs" or "getTab" requests.
   |
   |-- WorkerDescriptorActor (descriptors/worker.js)
   |
   |   Designates any type of worker: web worker, service worker, shared worker, chrome worker.
   |
   |   Returned by "listWorkers" request to the root actor to get all workers. (/!\ this is an expensive method)
   |   Returned by "listWorkers" request to a WindowGlobalTargetActor to get
   |   workers for a specific document/WindowGlobal. (this is a legacy, to be removed codepath)
   |   Returned by "listWorkers" request to a ContentProcessTargetActor to get
   |   workers for the chrome of the child process. (this is a legacy, to be removed codepath)
   |
   |-- ParentProcessDescriptorActor (descriptors/process.js)
   |
   |   Designates any parent or content processes.
   |   This exposes all chrome documents, JSMs/ESMs, JS XPCOM, etc.
   |
   |   Returned by "listProcesses" and "getProcess".
   |
   \-- WebExtensionDescriptorActor (descriptors/webextension.js)

       Designates a WebExtension add-on in the parent process. This gives some
       metadata about the add-on and watches for uninstall events.

       Returned by "listAddons" request.
```
All these descriptor actors expose a `getTarget()` method which
returns the target actor for the descriptor's debuggable context.

But note that this is now considered as a deprecated codepath.
Ideally, all targets should be retrieved via the new WatcherActor.
For now, the WatcherActor only support tabs and entire browser debugging.
Workers and add-ons still have to go through descriptor's getTarget.

## Watcher Actors

Each descriptor exposes a dedicated Watcher actor (via getWatcher RDP method),
which is scoped to the debuggable context of the descriptor.

This actor is about observing things.
It will notify you about:
* target actors via target-available-form and target-destroyed-form,
* resources via resource-available-form, resource-updated-form and resource-destroyed-form.

## Resources

Resources aren't necessary actors. That can be simple JSON objects describing a particular part of the Web.
Resources can be describing a console message, a JS source, a network event, ...

Each resource is being observed by a dedicated ResourceWatcher class implemented in [devtools/server/actors/resources/][resources-folder]
where the index.js registers all the resource types.

These `ResourceWatcher` classes should implement a `watch()` method to start watching for a given resource type
and a `destroy()` method to stop watching. One new instance will be instantiated each time we start watching for a given type.

These classes can be instantiated in various ways:
* just from the parent process if the resource can only be observed from there (ex: network events and some storages).
  In such case, the watch method will receive a watcher actor as argument.
* just from the target's thread, which can be a tab thread, or a worker thread.
  In such case, the watch method will receive a target actor as argument.

## Target Actors

Those are the actors exposed by the watcher actor `target-available-form` event , or, via descriptor's `getTarget()` methods.
They are meant to track the lifetime of a very precise debuggable piece of the descriptor context:
* One precise document instance, also called `WindowGlobal`,
* One worker,
* One parent or content process.

Its main purpose is to expose the target-scoped actor IDs, all contained in the target form.
The target form is exposed by watcher actor `target-available-form` event (or via the now deprecated descriptor's `getTarget()` method).

The target will be destroyed as soon as the related debuggable context.

For historical reasons, target actors also handle creating the ThreadActor, used
to manage breakpoints in the debugger.

The target-scoped actors expect to find the following properties on the target actor:
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

In addition to this, the actors inheriting from WindowGlobalTargetActor,
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

See WindowGlobalTargetActor documentation for more details.

## Target-scoped actors

Each of these actors focuses on providing one particular feature set. They are
children of a given target actor.

The data they return is filtered to reflect the target. For example, the
InspectorActor that you fetch from a WindowGlobalTargetActor gives you information
about the markup and styles only for the context of the target.

These actors may extend this hierarchy by having their own children, like
LongStringActor, WalkerActor, etc.

To improve performance, target-scoped actors are created lazily. The target
actor lists the actor ID for each one, but the actor modules aren't actually
loaded and instantiated at that point. Once the first request for a given
target-scoped actor is received by the server, that specific actor is
instantiated just in time to service the request.

The actor IDs of all these actors can be retrieve in the "form" of each target actor.
The "form" is notified by the Watcher actor via `target-avaible-form` RDP packet.

[actors-folder]: https://searchfox.org/mozilla-central/source/devtools/server/actors/
[resources-folder]: https://searchfox.org/mozilla-central/source/devtools/server/actors/resources/
