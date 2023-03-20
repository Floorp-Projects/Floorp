.. _tabbrowser:

===================
tabbrowser
===================

In the previous versions of Firefox, ``<xul:tabbrowser>`` was responsible for displaying and managing the contents of a window's tabs. As the browser evolved, the responsibilities of ``<xul:tabbrowser>`` grew. Each Firefox window had one ``<xul:tabbrowser>`` that could be accessed using the ``gBrowser`` variable.

At this point, ``<xul:tabbrowser>`` DOM element doesn't exist anymore, but we mention it here because it's often used synonymously with ``gBrowser``, and other documentation might still make direct or indirect reference to ``<xul:tabbrowser>``.

gBrowser
---------------

``gBrowser`` is a JavaScript object defined in :searchfox:`tabbrowser.js <browser/base/content/tabbrowser.js>`, that manages tabs, and the underlying infrastructure for switching tabs, adding tabs, removing tabs, knowing about tab switches, etc. ``gBrowser`` is available in the browser window scope and you get only one ``gBrowser`` per browser window.

What does the name gBrowser stand for?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

What does the *g* in ``gBrowser`` stand for? It's an old Mozilla convention and *g* stands for *global*. It's a way of indicating inside of the variable name that something is globally scoped. In this case it is global to the browser window and every single tab of the window will be managed through that window's ``gBrowser``.

What does the *browser* in ``gBrowser`` stand for? *browser* is an element that knows how to render web content. At some point in its lineage, Firefox didn't have tabs. There was one browser per window. That individual browser was called ``gBrowser``. The new ``gBrowser`` variable has the same interface as the old one, but would forward calls to the current ``selectedBrowser``, which is an actual ``<xul:browser>`` element.

Relationship between tabbrowser, browser and gBrowser
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``<xul:browser>`` is a XUL element that can load web pages, make HTTP requests and respond accordingly. It is conceptually similar to an ``iframe`` except that except that contains additional methods and has elevated privileges. In Firefox, each tab is associated with one ``<xul:browser>``.

Historically each Firefox window had one ``<xul:tabbrowser>``, that could be accessed using the ``gBrowser`` variable. It could contain multiple tabs each of which was associated with one ``<xul:browser>``.

Although the ``<xul:tabbrowser>`` DOM element was removed, you can still interact with all the browser's tabs using the ``gBrowser`` global. The ``gBrowser`` global is still defined in a file called :searchfox:`tabbrowser.js <browser/base/content/tabbrowser.js>` for the same historical reasons.

.. toctree::
   :maxdepth: 1

   async-tab-switcher
