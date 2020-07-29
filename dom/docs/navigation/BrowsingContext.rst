BrowsingContext and WindowContext
=================================

The ``BrowsingContext`` is the Gecko representation of the spec-defined
`Browsing Context`_ object.

.. _Browsing Context: https://html.spec.whatwg.org/multipage/browsers.html#browsing-context


The Browsing Context Tree
-------------------------

``BrowsingContext`` and ``WindowContext`` objects form a tree, corresponding
to the tree of frame elements which was used to construct them.


Example
^^^^^^^

Loading the HTML documents listed here, would end up creating a set of BrowsingContexts and WindowContexts forming the tree.

.. code-block:: html

  <!-- http://example.com/top.html -->
  <iframe src="frame1.html"></iframe>
  <iframe src="http://mozilla.org/"></iframe>

  <!-- http://example.com/frame1.html -->
  <iframe src="http://example.com/frame2.html"></iframe>

  <!-- http://mozilla.org -->
  <iframe></iframe>
  <iframe></iframe>

.. digraph:: browsingcontext

  node [shape=rectangle]

  "BC1" [label="BrowsingContext A"]
  "BC2" [label="BrowsingContext B"]
  "BC3" [label="BrowsingContext C"]
  "BC4" [label="BrowsingContext D"]
  "BC5" [label="BrowsingContext E"]
  "BC6" [label="BrowsingContext F"]

  "WC1" [label="WindowContext\n(http://example.com/top.html)"]
  "WC2" [label="WindowContext\n(http://example.com/frame1.html)"]
  "WC3" [label="WindowContext\n(http://mozilla.org)"]
  "WC4" [label="WindowContext\n(http://example.com/frame2.html)"]
  "WC5" [label="WindowContext\n(about:blank)"]
  "WC6" [label="WindowContext\n(about:blank)"]

  "BC1" -> "WC1";
  "WC1" -> "BC2";
  "WC1" -> "BC3";
  "BC2" -> "WC2";
  "BC3" -> "WC3";
  "WC2" -> "BC4";
  "BC4" -> "WC4";
  "WC3" -> "BC5";
  "BC5" -> "WC5";
  "WC3" -> "BC6";
  "BC6" -> "WC6";


Synced Fields
-------------

WIP - In-progress documentation at `<https://wiki.mozilla.org/Project_Fission/BrowsingContext>`_.


API Documentation
-----------------

.. cpp:class:: BrowsingContext

  .. hlist::
    :columns: 3

    * `header (searchfox) <https://searchfox.org/mozilla-central/source/docshell/base/BrowsingContext.h>`_
    * `source (searchfox) <https://searchfox.org/mozilla-central/source/docshell/base/BrowsingContext.cpp>`_
    * `html spec <https://html.spec.whatwg.org/multipage/browsers.html#browsing-context>`_

  This is a synced-context type. Instances of it will exist in every
  "relevant" content process for the navigation.

  Instances of :cpp:class:`BrowsingContext` created in the parent processes
  will be :cpp:class:`CanonicalBrowsingContext`.

  .. cpp:function:: WindowContext* GetParentWindowContext()

    Get the parent ``WindowContext`` embedding this context, or ``nullptr``,
    if this is the toplevel context.

  .. cpp:function:: WindowContext* GetTopWindowContext()

    Get the toplevel ``WindowContext`` embedding this context, or ``nullptr``
    if this is the toplevel context.

    This is equivalent to repeatedly calling ``GetParentWindowContext()``
    until it returns nullptr.

  .. cpp:function:: BrowsingContext* GetParent()

  .. cpp:function:: BrowsingContext* Top()

  .. cpp:function:: static already_AddRefed<BrowsingContext> Get(uint64_t aId)

    Look up a specific ``BrowsingContext`` by it's unique ID. Callers should
    check if the returned context has already been discarded using
    ``IsDiscarded`` before using it.

.. cpp:class:: CanonicalBrowsingContext : public BrowsingContext

  .. hlist::
    :columns: 3

    * `header (searchfox) <https://searchfox.org/mozilla-central/source/docshell/base/CanonicalBrowsingContext.h>`_
    * `source (searchfox) <https://searchfox.org/mozilla-central/source/docshell/base/CanonicalBrowsingContext.cpp>`_

  When a :cpp:class:`BrowsingContext` is constructed in the parent process,
  it is actually an instance of :cpp:class:`CanonicalBrowsingContext`.

  Due to being in the parent process, more information about the context is
  available from a ``CanonicalBrowsingContext``.

.. cpp:class:: WindowContext

  .. hlist::
    :columns: 3

    * `header (searchfox) <https://searchfox.org/mozilla-central/source/docshell/base/WindowContext.h>`_
    * `source (searchfox) <https://searchfox.org/mozilla-central/source/docshell/base/WindowContext.cpp>`_

.. cpp:class:: WindowGlobalParent : public WindowContext, public WindowGlobalActor, public PWindowGlobalParent

  .. hlist::
    :columns: 3

    * `header (searchfox) <https://searchfox.org/mozilla-central/source/dom/ipc/WindowGlobalParent.h>`_
    * `source (searchfox) <https://searchfox.org/mozilla-central/source/dom/ipc/WindowGlobalParent.cpp>`_

.. cpp:class:: WindowGlobalChild : public WindowGlobalActor, public PWindowGlobalChild

  .. hlist::
    :columns: 3

    * `header (searchfox) <https://searchfox.org/mozilla-central/source/dom/ipc/WindowGlobalChild.h>`_
    * `source (searchfox) <https://searchfox.org/mozilla-central/source/dom/ipc/WindowGlobalChild.cpp>`_
