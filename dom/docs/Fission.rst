=======
Fission
=======

Fission is a cross-functional project for revamping and strengthening the architecture of the Firefox browser.

The work is tracked under this bug (https://bugzilla.mozilla.org/show_bug.cgi?id=fission). See this Wiki page for more details (https://wiki.mozilla.org/Project_Fission).

We don't have an all-encompassing design document at this time. This may change in the future.

IPC Diagram
===========

.. image:: Fission-IPC-Diagram.svg

JSWindowActor
===============

What are JSWindowActors?
--------------------------

In the Fission world, JSWindowActors will be the replacement for framescripts. Framescripts were how we structured code to be aware of the parent (UI) and child (content) separation, including establishing the communication channel between the two (via the Frame Message Manager).

However, the framescripts had no way to establish further process separation downwards (that is, for out-of-process iframes). JSWindowActors will be the replacement.

How are they structured?
------------------------

A review of the current Message Manager mechanism
`````````````````````````````````````````````````

.. note::
   There are actually several types of Message Managers: Frame Message Managers, Window Message Managers, Group Message Managers and Process Message Managers. For the purposes of this documentation, it's simplest to refer to all of these mechanisms altogether as the "Message Manager mechanism". Most of the examples in this document will be operating on the assumption that the Message Manager is a Frame Message Manager, which is the most commonly used one.

Currently, in the post `Electrolysis Project`_ Firefox codebase, we have code living in the parent process (UI) that is in plain JS (.js files) or in JS modules (.jsm files). In the child process (hosting the content), we use framescripts (.js) and also JS modules. The framescripts are instantiated once per top-level frame (or, in simpler terms, once per tab). This code has access to all of the DOM from the web content, including all iframes within it.

The two processes communicate via the Frame Message Manager (mm) using the ``sendAsyncMessage`` / ``receiveMessage`` API, and any code in the parent can communicate with any code in the child (and vice versa), by just listening to the messages of interest.

The Frame Message Manager communication mechanism follows a publish / subscribe pattern similar to how Events work in Firefox:

1. Something exposes a mechanism for subscribing to notifications (``addMessageListener`` for the Frame Message Manager, ``addEventListener`` for Events).
2. The subscriber is responsible for unsubscribing when there's no longer interest in the notifications (``removeMessageListener`` for the Frame Message Manager, ``removeEventListener`` for Events).
3. Any number of subscribers can be attached at any one time.

.. figure:: Fission-framescripts.png
   :width: 320px
   :height: 200px

How JSWindowActors differ from the Frame Message Manager
``````````````````````````````````````````````````````````

For Fission, the JSWindowActors replacing framescripts will be structured in pairs. A pair of JSWindowActors will be instantiated lazily: one in the parent and one in the child process, and a direct channel of communication between the two will be established. The JSWindowActor in the parent must extend the global ``JSWindowActorParent`` class, and the JSWindowActor in the child must extend the global ``JSWindowActorChild`` class.

The JSWindowActor mechanism is similar to how `IPC Actors`_ work in the native layer of Firefox:

#. Every Actor has one counterpart in another process that they can communicate directly with.
#. Every Actor inherits a common communications API from a parent class.
#. Every Actor has a name that ends in either ``Parent`` or ``Child``.
#. There is no built-in mechanism for subscribing to messages. When one JSWindowActor sends a message, the counterpart JSWindowActor on the other side will receive it without needing to explicitly listen for it.

Other notable differences between JSWindowActor's and Message Manager / framescripts:

#. Each JSWindowActor pair is associated with a particular frame. For example, given the following DOM hierarchy::

     <browser src="https://www.example.com">
       <iframe src="https://www.a.com" />
       <iframe src="https://www.b.com" />

   A ``JSWindowActorParent / ``JSWindowActorChild`` pair instantiated for either of the ``iframe``'s would only be sending messages to and from that ``iframe``.

#. There's only one pair per actor type, per frame.

   For example, suppose we have a ``ContextMenu`` actor. The parent process can have up to N instances of the ``ContextMenuParent`` actor, where N is the number of frames that are currently loaded. For any individual frame though, there's only ever one `ContextMenuChild` associated with that frame.

#. We can no longer assume full, synchronous access to the frame tree, even in content processes.

   This is a natural consequence of splitting frames to run out-of-process.

#. ``JSWindowActorChild``'s live as long as the ``BrowsingContext`` they're associated with.

  If in the previously mentioned DOM hierarchy, one of the ``<iframe>``'s unload, any associated JSWindowActor pairs will be torn down.

.. hint::
   JSWindowActors are "managed" by the WindowGlobal IPC Actors, and are implemented as JS classes (subclasses of ``JSWindowActorParent`` and ``JSWindowActorChild``) instantiated when requested for any particular window. Like the Frame Message Manager, they are ultimately using IPC Actors to communicate under the hood.

.. figure:: Fission-actors-diagram.png
   :width: 233px
   :height: 240px

Cross-process communication with JSWindowActors
-------------------------------------------------

.. note::
    Like the Message Manager, JSWindowActors are implemented for both in-process and out-of-process frame communication. This means that porting to JSWindowActors can be done immediately without waiting for out-of-process iframes to be enabled.

The ``JSWindowActorParent`` and ``JSWindowActorChild`` base classes expose two methods for sending messages:

``sendAsyncMessage``
````````````````````

This has a similar signature as the ``sendAsyncMessage`` method for Message Managers::

    sendAsyncMessage("SomeMessage", { key: "value" }, { transferredObject });

Like messages sent via the Message Manager, anything that can be serialized using the structured clone algorithm can be sent down through the second argument. Additionally, ``nsIPrincipal``'s can be sent without manually serializing and deserializing them.

The third argument sends `Transferables`_ to the receiver, for example an ``ArrayBuffer``.

.. note::
    Cross Process Object Wrappers (CPOWs) cannot be sent over JSWindowActors.

.. note::
    Notably absent is ``sendSyncMessage`` or ``sendRPCMessage``. Sync IPC is not supported on JSWindowActors, and code which needs to send sync messages should be modified to use async messages, or must send them over the per-process message manager.

``sendQuery``
`````````````

``sendQuery`` improves upon ``sendAsyncMessage`` by returning a ``Promise``. The receiver of the message must then return a ``Promise`` that can eventually resolve into a value - at which time the ``sendQuery`` ``Promise`` resolves with that value.

The ``sendQuery`` method arguments follow the same conventions as ``sendAsyncMessage``, with the second argument being a structured clone, and the third being for `Transferables`_.

``receiveMessage``
``````````````````

This is identical to the Message Manager implementation of ``receiveMessage``. The method receives a single argument, which is the de-serialized arguments that were sent via either ``sendAsyncMessage`` or ``sendQuery``. Note that upon handling a ``sendQuery`` message, the ``receiveMessage`` handler must return a ``Promise`` for that message.

.. hint::
    Using ``sendQuery``, and the ``receiveMessage`` is able to return a value right away? Try using ``Promise.resolve(value);`` to return ``value``, or you could also make your ``receiveMessage`` method an async function, presuming none of the other messages it handles need to get a non-Promise return value.

Other JSWindowActor methods that can be overridden
--------------------------------------------------

``constructor()``

If there's something you need to do as soon as the JSWindowActor is instantiated, the ``constructor`` function is a great place to do that.

.. note::
    At this point the infrastructure for sending messages is not ready yet and objects such as ``manager`` or ``browsingContext`` are not available.

``observe(subject, topic, data)``
`````````````````````````````````

If you register your Actor to listen for ``nsIObserver`` notifications, implement an ``observe`` method with the above signature to handle the notification.

``handleEvent(event)``
``````````````````````

If you register your Actor to listen for content events, implement a ``handleEvent`` method with the above signature to handle the event.

``actorCreated``
````````````````

This method is called immediately after a child actor is created and initialized. Unlike the actor's constructor, it is possible to do things like access the actor's content window and send messages from this callback.

``willDestroy``
```````````````

This method will be called when we know that the JSWindowActor pair is going to be destroyed because the associated BrowsingContext is going away. You should override this method if you have any cleanup you need to do before going away.

You can also use ``willDestroy`` as a last opportunity to send messages to the other side, as the communications channel at this point is still running.

.. note::
    This method cannot be async.

``didDestroy``
``````````````

This is another point to clean-up an Actor before it is destroyed, but at this point, no communication is possible with the other side.

.. note::
    This method cannot be async.


Other things exposed on a JSWindowActorParent
---------------------------------------------

``CanonicalBrowsingContext``
````````````````````````````

TODO

``WindowGlobalParent``
``````````````````````

TODO

Other things exposed on a JSWindowActorChild
--------------------------------------------

``BrowsingContext``
```````````````````

TODO

``WindowGlobalChild``
`````````````````````

TODO


Helpful getters
```````````````

A number of helpful getters exist on a ``JSWindowActorChild``, including:

``this.document``
^^^^^^^^^^^^^^^^^

The currently loaded document in the frame associated with this ``JSWindowActorChild``.

``this.contentWindow``
^^^^^^^^^^^^^^^^^^^^^^

The outer window for the frame associated with this ``JSWindowActorChild``.

``this.docShell``
^^^^^^^^^^^^^^^^^

The ``nsIDocShell`` for the frame associated with this ``JSWindowActorChild``.

See `JSWindowActor.webidl`_ for more detail on exactly what is exposed on both ``JSWindowActorParent`` and ``JSWindowActorChild`` implementations.

How to port from message manager and framescripts to JSWindowActors
---------------------------------------------------------------------

.. _fission.message-manager-actors:

Message Manager Actors
``````````````````````

While the JSWindowActor mechanism was being designed and developed, large sections of our framescripts were converted to an "actor style" pattern to make eventual porting to JSWindowActors easier. These Actors use the Message Manager under the hood, but made it much easier to shrink our framescripts, and also allowed us to gain significant memory savings by having the actors be lazily instantiated.

You can find the list of Message Manager Actors (or "Legacy Actors") in `BrowserGlue.jsm <https://searchfox.org/mozilla-central/source/browser/components/BrowserGlue.jsm>`_ and `ActorManagerParent.jsm <https://searchfox.org/mozilla-central/source/toolkit/modules/ActorManagerParent.jsm>`_, in the ``LEGACY_ACTORS`` lists.

.. note::
  The split in Message Manager Actors defined between ``BrowserGlue`` and ``ActorManagerParent`` is mainly to keep Firefox Desktop specific Actors separate from Actors that can (in theory) be instantiated for non-Desktop browsers (like Fennec and GeckoView-based browsers). Firefox Desktop-specific Actors should be registered in ``BrowserGlue``. Shared "toolkit" Actors should go into ``ActorManagerParent``.

"Porting" these Actors often means doing what is necessary in order to move their registration entries from ``LEGACY_ACTORS`` to the ``ACTORS`` list.

Figuring out the lifetime of a new Actor pair
`````````````````````````````````````````````

In the old model, framescript were loaded and executed as soon as possible by the top-level frame. In the JSWindowActor model, the Actors are much lazier, and only instantiate when:

1. They're instantiated explicitly by calling ``getActor`` on a ``WindowGlobal``, and passing in the name of the Actor.
2. A message is sent to them.
3. A pre-defined ``nsIObserver`` observer notification fires
4. A pre-defined content Event fires

Making the Actors lazy like this saves on processing time to get a frame ready to load web pages, as well as the overhead of loading the Actor into memory.

When porting a framescript to JSWindowActors, often the first question to ask is: what's the entrypoint? At what point should the Actors instantiate and become active?

For example, when porting the content area context menu for Firefox, it was noted that the ``contextmenu`` event firing in content was a natural event to wait for to instantiate the Actor pair. Once the ``ContextMenuChild`` instantiated, the ``handleEvent`` method was used to inspect the event and prepare a message to be sent to the ``ContextMenuParent``. This example can be found by looking at the patch for the `Context Menu Fission Port`_.

.. _fission.registering-a-new-jswindowactor:

Registering a new JSWindowActor
```````````````````````````````

``ChromeUtils`` exposes an API for registering window actors, but both ``BrowserGlue`` and ``ActorManagerParent`` are the main entry points where the registration occurs. If you want to register an actor, you should put them in one of the ``ACTORS`` lists in one of those two files. See :ref:`fission.message-manager-actors` for details.

The ``ACTORS`` lists expect a key-value pair, where the key is the name of the actor pair (example: ``ContextMenu``), and the value is an ``Object`` of registration parameters.

The full list of registration parameters can be found in the `JSWindowActor.webidl`_ file as ``WindowActorOptions``, ``WindowActorSidedOptions`` and ``WindowActorChildOptions``.

Here's an example JSWindowActor registration pulled from ``BrowserGlue.jsm``:

.. code-block:: javascript

   Plugin: {
      parent: {
        moduleURI: "resource:///actors/PluginParent.jsm",
      },
      child: {
        moduleURI: "resource:///actors/PluginChild.jsm",
        events: {
          PluginBindingAttached: { capture: true, wantUntrusted: true },
          PluginCrashed: { capture: true },
          PluginOutdated: { capture: true },
          PluginInstantiated: { capture: true },
          PluginRemoved: { capture: true },
          HiddenPlugin: { capture: true },
        },

        observers: ["decoder-doctor-notification"],
      },

      allFrames: true,
    },

This example is for the JSWindowActor implementation of click-to-play for Flash.

Let's examine the first chunk:

.. code-block:: javascript

      parent: {
        moduleURI: "resource:///actors/PluginParent.jsm",
      },

Here, we're declaring that the ``PluginParent`` subclassing ``JSWindowActorParent`` will be defined and exported inside the ``PluginParent.jsm`` file. That's all we have to say for the parent (main process) side of things.

.. note::
    It's not sufficient to just add a new .jsm file to the actors subdirectories. You also need to update the ``moz.build`` files in the same directory to get the ``resource://`` linkages set up correctly.

Let's look at the second chunk:

.. code-block:: javascript

      child: {
        moduleURI: "resource:///actors/PluginChild.jsm",
        events: {
          PluginBindingAttached: { capture: true, wantUntrusted: true },
          PluginCrashed: { capture: true },
          PluginOutdated: { capture: true },
          PluginInstantiated: { capture: true },
          PluginRemoved: { capture: true },
          HiddenPlugin: { capture: true },
        },

        observers: ["decoder-doctor-notification"],
      },

      allFrames: true,
    },

We're similarly declaring where the ``PluginChild`` subclassing ``JSWindowActorChild`` can be found.

Next, we declare the content events, if fired in a BrowsingContext, will cause the JSWindowActor pair to instantiate if it doesn't already exist, and then have ``handleEvent`` called on the ``PluginChild`` instance. For each event name, an Object of event listener options can be passed. You can use the same event listener options as accepted by ``addEventListener``.

Next, we declare that ``PluginChild`` should observe the ``decoder-doctor-notification`` ``nsIObserver`` notification. When that observer notification fires, the ``PluginChild`` will be instantiated for all ``BrowsingContext``'s, and the ``observe`` method on the ``PluginChild`` implementation will be called.

Finally, we say that the ``PluginChild`` actor should apply to ``allFrames``. This means that the ``PluginChild`` is allowed to be loaded in any subframe. If ``allFrames`` is set to false, the actor will only ever load in the top-level frame.

Using ContentDOMReference instead of CPOWs
``````````````````````````````````````````

Despite being outlawed as a way of synchronously accessing the properties of objects in other processes, CPOWs ended up being useful as a way of passing handles for DOM elements between processes.

CPOW messages, however, cannot be sent over the JSWindowActor communications pipe, so this handy mechanism will no longer work.

Instead, a new module called `ContentDOMReference.jsm`_ has been created which supplies the same capability. See that file for documentation.

How to start porting parent-process browser code to use JSWindowActors
``````````````````````````````````````````````````````````````````````

The :ref:`fission.message-manager-actors` work made it much easier to migrate away from framescripts towards something that is similar to ``JSWindowActors``. It did not, however, substantially change how the parent process interacted with those framescripts.

So when porting code to work with ``JSWindowActors``, we find that this is often where the time goes - refactoring the parent process browser code to accomodate the new ``JSWindowActor`` model.

Usually, the first thing to do is to find a reasonable name for your actor pair, and get them registered (see :ref:`fission.registering-a-new-jswindowactor`), even if the actors implementations themselves are nothing but unmodified subclasses of ``JSWindowActorParent`` and ``JSWindowActorChild``.

Next, it's often helpful to find and note all of the places where ``sendAsyncMessage`` is being used to send messages through the old message manager interface for the component you're porting, and where any messages listeners are defined.

Let's look at a hypothetical example. Suppose we're porting part of the Page Info dialog, which scans each frame for useful information to display in the dialog. Given a chunk of code like this:

.. code-block:: javascript

    // This is some hypothetical Page Info dialog code.

    let mm = browser.messageManager;
    mm.sendAsyncMessage("PageInfo:getInfoFromAllFrames", { someArgument: 123 });

    // ... and then later on

    mm.addMessageListener("PageInfo:info", async function onmessage(message) {
      // ...
    });

If a ``PageInfo`` pair of ``JSWindowActor``'s is registered, it might be tempting to simply replace the first part with:

.. code-block:: javascript

    let actor = browser.browsingContext.currentWindowGlobal.getActor("PageInfo");
    actor.sendAsyncMessage("PageInfo:getInfoFromAllFrames", { someArgument: 123 });

However, if any of the frames on the page are running in their own process, they're not going to receive that ``PageInfo:getInfoFromAllFrames`` message. Instead, in this case, we should walk the ``BrowsingContext`` tree, and instantiate a ``PageInfo`` actor for each global, and send one message each to get information for each frame. Perhaps something like this:

.. code-block:: javascript

    let contextsToVisit = [browser.browsingContext];
    while (contextsToVisit.length) {
      let currentContext = contextsToVisit.pop();
      let global = currentContext.currentWindowGlobal;

      if (!global) {
        continue;
      }

      let actor = global.getActor("PageInfo");
      actor.sendAsyncMessage("PageInfo:getInfoForFrame", { someArgument: 123 });

      contextsToVisit.push(...currentContext.getChildren());
    }

The original ``"PageInfo:info"`` message listener will need to be updated, too. Any responses from the ``PageInfoChild`` actor will end up being passed to the ``receiveMessage`` method of the ``PageInfoParent`` actor. It will be necessary to pass that information along to the interested party (in this case, the dialog code which is showing the table of interesting Page Info).

It might be necessary to refactor or rearchitect the original senders and consumers of message manager messages in order to accommodate the ``JSWindowActor`` model. Sometimes it's also helpful to have a singleton management object that manages all ``JSWindowActorParent`` instances and does something with their results. See ``PermitUnloader`` inside the implementation of `BrowserElementParent.jsm`_ for example.

Where to store state
````````````````````

It's not a good idea to store any state within a ``JSWindowActorChild`` that you want to last beyond the lifetime of its ``BrowsingContext``. An out-of-process ``<iframe>`` can be closed at any time, and if it's the only one for a particular content process, that content process will soon be shut down, and any state you may have stored there will go away.

Your best bet for storing state is in the parent process.

.. hint::
    If each individual frame needs state, consider using a ``WeakMap`` in the parent process, mapping ``CanonicalBrowsingContext``'s with that state. That way, if the associates frames ever go away, you don't have to do any cleaning up yourself.

If you have state that you want multiple ``JSWindowActorParent``'s to have access to, consider having a "manager" of those ``JSWindowActorParent``'s inside of the same .jsm file to hold that state. See ``PermitUnloader`` inside the implementation of `BrowserElementParent.jsm`_ for example.

Do not break Responsive Design Mode (RDM)
`````````````````````````````````````````
RDM not being fully covered by unit tests makes it fragile and easy to break without anyone noticing when porting things to ``JSWindowActor``. This is because RDM currently lives in its own minimalistic browser that is embedded into the regular one and messages are proxied between the inner and the outer browser Message Managers.

However, tunneling is not necessary anymore since the RDM browser will have its own instance of ``JSWindowActorParent`` that can directly access
the outer browser from the inner browser via the ``outerBrowser`` property set only when we are in RDM mode (see `bug 1569570 <https://bugzilla.mozilla.org/show_bug.cgi?id=1569570>`_). Here's an example where a JSWindowActorParent realizes that it has been sent to the RDM inner browser, and then accesses the outer browser:

.. code-block:: javascript

    let browser = this.browsingContext.top.embedderElement; // Should point to the inner
                                                            // browser if we are in RDM.

    if (browser.outerBrowser) {
      // We are in RDM mode and we probably
      // want to work with the outer browser.
      browser = browser.outerBrowser;
    }

.. note::
    Message Manager tunneling is done in `tunnel.js <https://searchfox.org/mozilla-central/source/devtools/client/responsive/browser/tunnel.js>`_ and messages can be deleted from it after porting the code that uses them.

Minimal Example Actors
----------------------

**Define an Actor**

.. code-block:: javascript

  // resource://testing-common/TestParent.jsm
  var EXPORTED_SYMBOLS = ["TestParent"];
  class TestParent extends JSWindowActorParent {
    constructor() {
      super();
    }
    ...
  }

.. code-block:: javascript

  // resource://testing-common/TestChild.jsm
  var EXPORTED_SYMBOLS = ["TestChild"];
  class TestChild extends JSWindowActorChild {
    constructor() {
      super();
    }
    ...
  }


**Get a JS window actor for a specific window**

.. code-block:: javascript

  // get parent side actor
  let parentActor = this.browser.browsingContext.currentWindowGlobal.getActor("Test");

  // get child side actor
  let childActor = content.window.getWindowGlobalChild().getActor("Test");

.. _Electrolysis Project: https://wiki.mozilla.org/Electrolysis
.. _IPC Actors: https://developer.mozilla.org/en-US/docs/Mozilla/IPDL/Tutorial
.. _Transferables: https://developer.mozilla.org/en-US/docs/Web/API/Transferable
.. _Context Menu Fission Port: https://hg.mozilla.org/mozilla-central/rev/adc60720b7b8
.. _ContentDOMReference.jsm: https://searchfox.org/mozilla-central/source/toolkit/modules/ContentDOMReference.jsm
.. _JSWindowActor.webidl: https://searchfox.org/mozilla-central/source/dom/chrome-webidl/JSWindowActor.webidl
.. _BrowserElementParent.jsm: https://searchfox.org/mozilla-central/rev/ec806131cb7bcd1c26c254d25cd5ab8a61b2aeb6/toolkit/actors/BrowserElementParent.jsm
