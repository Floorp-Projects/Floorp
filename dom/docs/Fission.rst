=======
Fission
=======

Fission is a cross-functional project for revamping and strengthening the architecture of the Firefox browser.

The work is tracked under this bug (https://bugzilla.mozilla.org/show_bug.cgi?id=fission). See this Wiki page for more details (https://wiki.mozilla.org/Project_Fission).

We don't have an all-encompassing design document at this time. This may change in the future.

IPC Diagram
===========

.. image:: Fission-IPC-Diagram.svg

JS Window Actor
===============

What are JS Window Actors?
--------------------------

In the Fission world, JS Window Actors will be the replacement for framescripts. Framescripts were how we structured code to be aware of the parent (UI) and child (content) separation, including establishing the communication channel between the two (via the Frame Message Manager).

However, the framescripts had no way to establish further process separation downwards (that is, for out-of-process iframes). JS Window Actors will be the replacement.

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

How JS Window Actors differ from the Frame Message Manager
``````````````````````````````````````````````````````````

For Fission, the JS Window Actors replacing framescripts will be structured in pairs. A pair of JS Window Actors will be instantiated lazily: one in the parent and one in the child process, and a direct channel of communication between the two will be established. The JS Window Actor in the parent must extend the global ``JSWindowActorParent`` class, and the JS Window Actor in the child must extend the global ``JSWindowActorChild`` class.

The JS Window Actor mechanism is similar to how `IPC Actors`_ work in the native layer of Firefox:

#. Every Actor has one counterpart in another process that they can communicate directly with.
#. Every Actor inherits a common communications API from a parent class.
#. Every Actor has a name that ends in either ``Parent`` or ``Child``.
#. There is no built-in mechanism for subscribing to messages. When one JS Window Actor sends a message, the counterpart JS Window Actor on the other side will receive it without needing to explicitly listen for it.

Other notable differences between JSWindowActor's and Message Manager / framescripts:

#. Each JSWindowActor pair is associated with a particular frame. For example, given the following DOM hierarchy::

     <browser src="https://www.example.com">
       <iframe src="https://www.a.com" />
       <iframe src="https://www.b.com" />

   A ``JSWindowActorParent / ``JSWindowActorChild`` pair instantiated for either of the ``iframe``'s would only be sending messages to and from that ``iframe``.

#. We can no longer assume full, synchronous access to the frame tree, even in content processes.

   This is a natural consequence of splitting frames to run out-of-process.

#. ``JSWindowActorChild``'s live as long as the ``BrowsingContext`` they're associated with.

  If in the previously mentioned DOM hierarchy, one of the ``<iframe>``s unload, any associated JSWindowActor pairs will be torn down.

.. hint::
   JS Window Actors are "managed" by the WindowGlobal IPC Actors, and are implemented as JS classes (subclasses of ``JSWindowActorParent`` and ``JSWindowActorChild``) instantiated when requested for any particular window. Like the Frame Message Manager, they are ultimately using IPC Actors to communicate under the hood.

.. figure:: Fission-actors-diagram.png
   :width: 233px
   :height: 240px

Cross-process communication with JS Window Actors
-------------------------------------------------

.. note::
    Like the Message Manager, JSWindowActors are implemented for both in-process and out-of-process frame communication. This means that porting to JSWindowActors can be done immediately without waiting for out-of-process iframes to be enabled.

The ``JSWindowActorParent`` and ``JSWindowActorChild`` base classes expose two methods for sending messages:

``sendAsyncMessage``
````````````````````

This has a similar signature as the ``sendAsyncMessage`` method for Message Managers::

    sendAsyncMessage("SomeMessage", { key: "value" }, { transferredObject });

The second argument is serialized and sent down to the receiver (this can include ``nsIPrincipal``s), and the third object sends `Transferrables`_ to the receiver. Note that CPOWs cannot be sent.

.. note::
    Notably absent is ``sendSyncMessage`` or ``sendRPCMessage``. Sync IPC is not supported on JSWindowActors, and code which needs to send sync messages should be modified to use async messages, or must send them over the per-process message manager.

``sendQuery``
`````````````

``sendQuery`` improves upon ``sendAsyncMessage`` by returning a ``Promise``. The receiver of the message must then return a ``Promise`` that can eventually resolve into a value - at which time the ``sendQuery`` ``Promise`` resolves with that value.

``receiveMessage``
``````````````````

This is identical to the Message Manager implementation of ``receiveMessage``. The method receives a single argument, which is the de-serialized arguments that were sent via either ``sendAsyncMessage`` or ``sendQuery``. Note that upon handling a ``sendQuery`` message, the ``receiveMessage`` handler must return a ``Promise`` for that message.

.. hint::
    Using ``sendQuery``, and the ``receiveMessage`` is able to return a value right away? Try using ``Promise.resolve(value);`` to return ``value``.

JSWindowActor destroy methods
-----------------------------

``willDestroy``
```````````````

This method will be called when we know that the JSWindowActor pair is going to be destroyed because the associated BrowsingContext is going away. You should override this method if you have any cleanup you need to do before going away.

You can also use ``willDestroy`` as a last opportunity to send messages to the other side, as the communications channel at this point is still running.

``didDestroy``
``````````````

This is another point to clean-up an Actor before it is destroyed, but at this point, no communication is possible with the other side.


Other things are exposed on a JSWindowActorParent
-------------------------------------------------

``BrowsingContext``
```````````````````

TODO

``WindowGlobalParent``
``````````````````````

TODO

Other things are exposed on a JSWindowActorChild
-------------------------------------------------

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

See `JSWindowActor.webidl <https://searchfox.org/mozilla-central/source/dom/chrome-webidl/JSWindowActor.webidl>`_ for more detail on exactly what is exposed on both ``JSWindowActorParent`` and ``JSWindowActorChild`` implementations.

How to port from message manager and framescripts to JS Window Actors
---------------------------------------------------------------------

TBD

Example
-------

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


**Registering an Actor**

.. code-block:: javascript

    ChromeUtils.registerWindowActor("Test", {
      parent: {
        moduleURI: "resource://testing-common/TestParent.jsm",
      },
      child: {
        moduleURI: "resource://testing-common/TestChild.jsm",

        events: {
          "mozshowdropdown": {},
        },

        observers: [
          "test-js-window-actor-child-observer",
        ],
      },

      allFrames: true,
    });


**Get a JS window actor for a specific window**

.. code-block:: javascript

  // get parent side actor
  let parentActor = this.browser.browsingContext.currentWindowGlobal.getActor("Test");

  // get child side actor
  let childActor = content.window.getWindowGlobalChild().getActor("Test");

.. _Electrolysis Project: https://wiki.mozilla.org/Electrolysis
.. _IPC Actors: https://developer.mozilla.org/en-US/docs/Mozilla/IPDL/Tutorial
.. _Transferrables https://developer.mozilla.org/en-US/docs/Web/API/Transferable