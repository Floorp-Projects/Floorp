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

What are actors?
----------------

In the Fission world, Actors will be the replacement for framescripts. Framescripts were how we structured code to be aware of the parent (UI) and child (content) separation, including establishing the communication channel between the two (via the message manager).

However, the framescripts had no way to establish further process separation downwards (that is, for out-of-process iframes). Actors will be the replacement.

How are they structured?
------------------------

Currently, in the post-e10s Firefox codebase, we have code living in the parent process (UI) that is in plain JS (.js files) or in JS modules (.jsm). In the child process (hosting the content), we use framescripts (.js) and also JS modules. The framescripts are instantiated once per top-level frame (or, in simpler terms, once per tab). This code has access to all of the DOM from the web content, including iframes on it.

The two processes communicate between them through the message manager (mm) using the sendAsyncMessage API, and any code in the parent can communicate with any code in the child (and vice versa), by just listening to the messsages of interest.

.. figure:: Fission-framescripts.png
   :width: 320px
   :height: 200px

For fission, the actors replacing FrameScript will be structured in pairs. A pair of actors will be instantiated lazily: one in the parent and one in the child process, and a direct channel of communication between the two will be established.
These actors are "managed" by the WindowGlobal actors, and are implemented as JS classes instantiated when requested for any particular window.

.. figure:: Fission-actors-diagram.png
   :width: 233px
   :height: 240px

Communicating between actors
----------------------------

The actor will only communicate between each other. To communicate, the actor supports ``sendAsyncMessage`` and ``sendQuery`` which acts like send async message, but also returns a promise, and it will be present for both in-process and out-of-process windows.

.. note::

    Sync IPC is not supported on JSWindowActors, and code which needs to send sync messages should be modified, or must send them over the per-process message manager.

Things are exposed on a JS Window Actor
---------------------------------------

See `JSWindowActor.webidl <https://searchfox.org/mozilla-central/source/dom/chrome-webidl/JSWindowActor.webidl>`_ for more detail.


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
