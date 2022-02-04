Browsing Context Embedding
==========================

Embedder Element to nsDocShell
------------------------------

In order to render the contents of a ``BrowsingContext``, the embedding
element needs to be able to communicate with the ``nsDocShell`` which is
currently being used to host it's content. This is done in 3 different ways
depending on which combination of processes is in-use.

- *in-process*: The ``nsFrameLoader`` directly embeds the ``nsDocShell``.
- *remote tab*: The parent process is the embedder, and uses a ``PBrowser``,
  via a ``BrowserHost``. The ``BrowserChild`` actor holds the actual
  ``nsDocShell`` alive.
- *remote subframe*: A content process is the embedder, and uses a
  ``PBrowserBridge``, via a ``BrowserBridgeHost`` to communicate with the
  parent process. The parent process then uses a ``BrowserParent``, as in the
  *remote tab* case, to communicate with the ``nsDocShell``.

Diagram
^^^^^^^

.. digraph:: embedding

  node [shape=rectangle]

  subgraph cluster_choice {
    color=transparent;
    node [shape=none];

    "In-Process";
    "Remote Tab";
    "Remote Subframe";
  }

  "nsFrameLoaderOwner" [label="nsFrameLoaderOwner\ne.g. <iframe>, <xul:browser>, <embed>"]

  "nsFrameLoaderOwner" -> "nsFrameLoader";

  "nsFrameLoader" -> "In-Process" [dir=none];
  "nsFrameLoader" -> "Remote Tab" [dir=none];
  "nsFrameLoader" -> "Remote Subframe" [dir=none];

  "In-Process" -> "nsDocShell";
  "Remote Tab" -> "BrowserHost";
  "Remote Subframe" -> "BrowserBridgeHost";

  "BrowserHost" -> "BrowserParent";
  "BrowserParent" -> "BrowserChild" [label="PBrowser" style=dotted];
  "BrowserChild" -> "nsDocShell";

  "BrowserBridgeHost" -> "BrowserBridgeChild";
  "BrowserBridgeChild" -> "BrowserBridgeParent" [label="PBrowserBridge", style=dotted];
  "BrowserBridgeParent" -> "BrowserParent";

nsDocShell to Document
----------------------

Embedding an individual document within a ``nsDocShell`` is done within the
content process, which has that docshell.


Diagram
^^^^^^^

This diagram shows the objects involved in a content process which is being
used to host a given ``BrowsingContext``, along with rough relationships
between them. Dotted lines represent a "current" relationship, whereas solid
lines are a stronger lifetime relationship.

.. graph:: document

  node [shape=rectangle]

  "BrowsingContext" -- "nsDocShell" [style=dotted];
  "nsDocShell" -- "nsGlobalWindowOuter";
  "nsGlobalWindowOuter" -- "nsGlobalWindowInner" [style=dotted];
  "nsGlobalWindowInner" -- "Document" [style=dotted];

  "nsDocShell" -- "nsDocumentViewer" [style=dotted];
  "nsDocumentViewer" -- "Document";
  "nsDocumentViewer" -- "PresShell";

  "nsGlobalWindowInner" -- "WindowGlobalChild";
  "BrowsingContext" -- "WindowContext" [style=dotted];
  "WindowContext" -- "nsGlobalWindowInner";

  subgraph cluster_synced {
    label = "Synced Contexts";
    "BrowsingContext" "WindowContext";
  }
