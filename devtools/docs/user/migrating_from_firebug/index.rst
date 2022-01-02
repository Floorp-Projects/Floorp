======================
Migrating from Firebug
======================

When migrating from Firebug to the Firefox Developer Tools, you may wonder where the features you loved in Firebug are available in the Developer Tools. The following list aims to help Firebug users to find their way into the Developer Tools.

.. image:: logo-developer-quantum.png
  :class: center

.. rst-class:: center

  For the latest developer tools and features, try Firefox Developer Edition.

.. raw:: html

  <a href="https://www.mozilla.org/en-US/firefox/developer/" style="width: 280px; display: block; margin-left: auto; margin-right: auto; padding: 10px; text-align: center; border-radius: 4px; background-color: #81BC2E; white-space: nowrap; color: white; text-shadow: 0px 1px 0px rgba(0, 0, 0, 0.25); box-shadow: 0px 1px 0px 0px rgba(0, 0, 0, 0.2), 0px -1px 0px 0px rgba(0, 0, 0, 0.3) inset;">Download Firefox Developer Edition</a>


General
*******

Activation
----------

Firebug's activation is URL based respecting the `same origin policy <https://en.wikipedia.org/wiki/Same_origin_policy>`_. That means that when you open a page on the same origin in a different tab, Firebug gets opened automatically. And when you open a page of a different origin in the same tab, it closes automatically. The DevTools' activation on the other hand is tab based. That means, that when you open the DevTools in a tab, they stay open even when you switch between different websites. When you switch to another tab, though, they're closed.


Open the tools
--------------

Firebug can be opened by pressing F12. To open it to inspect an element it is possible to press :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`C` / :kbd:`Cmd` + :kbd:`Opt` + :kbd:`C`. The DevTools share the same shortcuts, but also provide :ref:`shortcuts for the different panels <keyboard-shortcuts-opening-and-closing-tools>`. E.g. the :doc:`Network Monitor <../network_monitor/index>` can be opened via :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`Q` / :kbd:`Cmd` + :kbd:`Opt` + :kbd:`Q`, the :doc:`Web Console <../web_console/index>` via :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`K` / :kbd:`Cmd` + :kbd:`Opt` + :kbd:`K` and the :doc:`Debugger <../debugger/index>` via :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`S` / :kbd:`Cmd` + :kbd:`Opt` + :kbd:`S`.


Web Console
***********

The :doc:`Web Console <../web_console/index>` is the equivalent of Firebug's Console panel. It shows log information associated with a web page and allows you to execute JavaScript expressions via its :doc:`command line <../web_console/the_command_line_interpreter/index>`. The display between both is somewhat different. This may be changed in `bug 1269730 <https://bugzilla.mozilla.org/show_bug.cgi?id=1269730>`_.


Filter log messages
-------------------

Firebug offers two ways to filter log messages, via the options menu and via the filter buttons within the toolbar. The Developer Tools console offers similar functionality via the :ref:`filter buttons inside its toolbar <web_console_ui_tour_filtering_by_category>` — centralized at one place.


Command Line API
----------------

The Command Line API in Firebug provides some special functions for your convenience. The Developer Tools command line has :ref:`some functions in common <command_line_interpreter_helper_commands>`, but also has some other functions and misses others.


Console API
-----------

To log things to the console from within the web page Firebug makes a Console API available within the page. The Developer Tools share the `same API <https://developer.mozilla.org/en-US/docs/Web/API/console>`_, so your ``console.*`` statements will continue to work.


Persist logs
------------

In Firebug you can click the *Persist* button within the toolbar to keep the logged messages between page navigations and reloads. In the DevTools this option is called :ref:`Enable persistent logs <settings-common-preferences>` and is available within the Toolbox Options panel.


Server logs
-----------

Firebug extensions like FirePHP allow to log server-side messages to the Firebug console. This functionality is already :ref:`integrated into the DevTools <web_console_server>` using the `ChromeLogger <https://craig.is/writing/chrome-logger>`_ protocol and doesn't require any extensions to be installed.


Command history
---------------

The :ref:`command history <command_line_interpreter_execution_history>` available through a button in Firebug's command line, is available by pressing :kbd:`↑`/:kbd:`↓` within the DevTools command line.


Inspect object properties
-------------------------

By clicking on an object logged within the console you can inspect the object's properties and methods within the DOM panel. In the Firefox DevTools you can also inspect the objects. The difference is that they :ref:`show the properties and methods within a side panel inside the Web Console <web_console_rich_output_examining_object_properties>`.


Show network requests
---------------------

The Console panel in Firebug allows to log `AJAX <https://developer.mozilla.org/en-US/docs/Glossary/AJAX>`_ requests (aka `XMLHttpRequest <https://developer.mozilla.org/en-US/docs/Glossary/XHR_(XMLHttpRequest)>`_). This option is also available within the DevTools Web Console via the *Net* > *XHR*. Furthermore, the Web Console even allows to display all other network requests via *Net* > *Log*.


View JSON and XML structures
----------------------------

To view JSON and XML responses of `AJAX <https://developer.mozilla.org/en-US/docs/Glossary/AJAX>`_ requests, Firebug has special tabs when expanding the request within the Console panel. The DevTools Web Console shows those structures directly under the "Response" tab.


Multi-line command line
-----------------------

Firebug's console has a multi-line command line called Command Editor. The DevTools have a :ref:`side panel <command_line_interpreter_multi_line_mode>` like the Command Editor.


Response preview
----------------

There is a *Preview* tab when a network request logged to the console is expanded in Firebug. The Web Console displays a preview within the *Response* tab. It is currently missing the preview for HTML, XML and SVG, though, which is tracked in `bug 1247392 <https://bugzilla.mozilla.org/show_bug.cgi?id=1247392>`_ and `bug 1262796 <https://bugzilla.mozilla.org/show_bug.cgi?id=1262796>`_, but when you click on the URL of the request you switch to the :doc:`Network Monitor <../network_monitor/index>`, which has a *Preview* tab.


Inspector
*********

Firebug has an HTML panel, which allows to edit HTML/XML/SVG and the CSS related to it. Within the DevTools this functionality is served by the :doc:`Page Inspector <../page_inspector/index>`.


Edit HTML
---------

Within the Page Inspector the tag attributes and the contents can be edited inline just like in Firebug. Beyond that it allows to edit the tag names inline.

You can also edit the HTML directly. In Firebug you do this by right-clicking a node and clicking Edit HTML... in the context menu. In the DevTools this option is also available via the context menu. There the option is called :ref:`Edit As HTML <page-inspector-how-to-examine-and-edit-html-editing_html>`. Only the live preview of changes is currently missing, which is tracked in `bug 1067318 <https://bugzilla.mozilla.org/show_bug.cgi?id=1067318>`_ and `bug 815464 <https://bugzilla.mozilla.org/show_bug.cgi?id=815464>`_.


Copy HTML and related information
---------------------------------

Firebug's HTML panel allows to copy the inner and outer HTML of an element as well as the CSS and XPath to it via the context menu of an element. The Page Inspector provides the same functionality except copying XPaths. This is covered by `bug 987877 <https://bugzilla.mozilla.org/show_bug.cgi?id=987877>`_.


Edit CSS
--------

Both tools allow to view and edit the CSS rules related to the element selected within the node view in a similar way. Firebug has a Style side panel for this, the DevTools have a :doc:`Rules side panel <../page_inspector/how_to/examine_and_edit_css/index>`.

In Firebug you add new rules by right-clicking and choosing *Add Rule...* from the context menu. The DevTools also have a context menu option for that named :ref:`Add New Rule and additionally have a + button <page_inspector_how_to_examine_and_edit_css_add_rules>` within the Rules panel's toolbar to create new rules.

To edit element styles, i.e. the CSS properties of the `style <https://developer.mozilla.org/en-US/docs/Web/HTML/Global_attributes#attr-style>`_ attribute of an element, in Firebug you have to right-click into the Style side panel and choose Edit Element Style... from the context menu. The DevTools display an :ref:`element {} rule <page_inspector_how_to_examine_and_edit_css_element_rule>` for this purpose, which requires a single click into it to start editing the properties.


Auto-completion of CSS
----------------------

As in Firebug, the Rules view provides an auto-completion for the CSS property names and their values. A few property values are not auto-completed yet, which is tracked in `bug 1337918 <https://bugzilla.mozilla.org/show_bug.cgi?id=1337918>`_.


Copy & paste CSS
----------------

Firebug's Style side panel as well as the DevTools' Rules side panel provide options within their context menus to copy the CSS rule or the style declarations. The DevTools additionally provide an option to copy the selector of a rule and copy disabled property declarations as commented out. They are missing the option to copy the whole style declaration, though this can be achieved by selecting them within the panel and copying the selection by pressing :kbd:`Ctrl` + :kbd:`C` or via the context menu.

The Rules side panel of the DevTools is smarter when it comes to pasting CSS into it. You can paste whole style declarations into an existing rule property declarations which are commented out are automatically disabled.


Toggle pseudo-classes
---------------------

Firebug lets you toggle the CSS `pseudo-classes <https://developer.mozilla.org/en-US/docs/Web/CSS/Pseudo-classes>`_ `:hover <https://developer.mozilla.org/en-US/docs/Web/CSS/:hover>`_, `:active <https://developer.mozilla.org/en-US/docs/Web/CSS/:active>`_ and `:focus <https://developer.mozilla.org/en-US/docs/Web/CSS/:focus>`_ for an element via the options menu of the Style side panel. In the DevTools there are two ways to do the same. The first one is to toggle them via the pseudo-class panel within the Rules side panel. The second one is to right-click and element within the node view and toggle the pseudo-classes via the :ref:`context menu <page_inspector_how_to_examine_and_edit_html_context_menu_reference>`.


Examine CSS shorthand properties
--------------------------------

CSS `shorthand properties <https://developer.mozilla.org/en-US/docs/Web/CSS/Shorthand_properties>`_ can be split into their related longhand properties by setting the option *Expand Shorthand Properties* within the Style side panel. The DevTools' Rules panel is a bit smarter and allows you to expand individual shorthand properties by clicking the twisty besides them.


Only show applied styles
------------------------

The Style side panel in Firebug has an option to display only the properties of a CSS rule that are applied to the selected element and hide all overwritten styles. There is no such feature in the :doc:`Rules side panel <../page_inspector/how_to/examine_and_edit_css/index>` of the DevTools, but it is requested in `bug 1335327 <https://bugzilla.mozilla.org/show_bug.cgi?id=1335327>`_.


Inspect box model
-----------------

In Firebug the `box model <https://developer.mozilla.org/en-US/docs/Learn/CSS/Building_blocks/The_box_model>`_ can be inspected via the Layout side panel. In the DevTools the :doc:`box model is part of the Computed side panel <../page_inspector/how_to/examine_and_edit_the_box_model/index>`. Both tools highlight the different parts of the box model within the page when hovering them in the box model view. Also, both tools allow you to edit the different values inline via a click on them.


Inspect computed styles
-----------------------

The computed values of CSS properties are displayed within the DevTools' :ref:`Computed side panel <page_inspector_how_to_examine_and_edit_css_examine_computed_css>` like within Firebug's Computed side panel. The difference is that in the DevTools the properties are always listed alphabetically and not grouped (see `bug 977128 <https://bugzilla.mozilla.org/show_bug.cgi?id=977128>`_) and there is no option to hide the Mozilla specific styles, therefore there is an input field allowing to filter the properties.


Inspect events
--------------

Events assigned to an element are displayed in the Events side panel in Firebug. In the DevTools they are shown when clicking the small 'ev' icon besides an element within the node view. Both tools allow to display wrapped event listeners (e.g. listeners wrapped in jQuery functions). To improve the UI of the DevTools, there is also a request to add an Events side panel to them like the one in Firebug (see `bug 1226640 <https://bugzilla.mozilla.org/show_bug.cgi?id=1226640>`_).


Stop script execution on DOM mutation
-------------------------------------

In Firebug you can break on DOM mutations, that means that when an element is changed, the script execution is stopped at the related line within the JavaScript file, which caused the change. This feature can globally be enabled via the *Break On Mutate* button, or individually for each element and for different types of changes like attribute changes, content changes or element removal. Unfortunately, the DevTools do not have this feature yet (see `bug 1004678 <https://bugzilla.mozilla.org/show_bug.cgi?id=1004678>`_). To stop the script execution there, you need to set a breakpoint on the line with the modification within the :doc:`Debugger panel <../debugger/index>`.


Search for elements via CSS selectors or XPaths
-----------------------------------------------

Firebug allows to search for elements within the HTML panel via CSS selectors or XPaths. Also the :ref:`DevTools' Inspector panel allows to search for CSS selectors <page_inspector_how_to_examine_and_edit_html_searching>`. It even displays a list with matching IDs or classes. Searching by XPaths is not supported though (see `bug 963933 <https://bugzilla.mozilla.org/show_bug.cgi?id=963933>`_).


Debugger
********

What's the Script panel in Firebug, is the :doc:`Debugger panel <../debugger/index>` in the DevTools. Both allow you to debug JavaScript code executed on a website.


Switch between sources
----------------------

Firebug has a Script Location Menu listing all JavaScript sources related to the website. Those sources can be static, i.e. files, or they can be dynamically generated (i.e. scripts executed via event handlers, ``eval()``, ``new Function()``, etc.). In the DevTools' Debugger panel the scripts are listed at the left side within the :ref:`Sources side panel <debugger-ui-tour-source-list-pane>`. Dynamically generated scripts are only listed there when they are :doc:`named via a //# sourceURL comment <../debugger/how_to/debug_eval_sources/index>`.


Managing breakpoints
--------------------

In Firebug you can set different types of breakpoints, which are all listed within the Breakpoints side panel. In the DevTools the breakpoints are shown below each script source within the :ref:`Sources side panel <debugger-ui-tour-source-list-pane>`. Those panels allow you to enable and disable single or all breakpoints and to remove single breakpoints or all of them at once. They do currently only allow to set script breakpoints. XHR, DOM, Cookie and Error breakpoints are not supported yet (see `bug 821610 <https://bugzilla.mozilla.org/show_bug.cgi?id=821610>`_, `bug 1004678 <https://bugzilla.mozilla.org/show_bug.cgi?id=1004678>`_, `bug 895893 <https://bugzilla.mozilla.org/show_bug.cgi?id=895893>`_ and `bug 1165010 <https://bugzilla.mozilla.org/show_bug.cgi?id=1165010>`_). While there are no breakpoints for single JavaScript errors, there is a setting *Pause on Exceptions* within the :ref:`Debugger panel options <settings-debugger>`.


Step through code
-----------------

Once the script execution is stopped, you can step through the code using the Continue (:kbd:`F8`), Step Over (:kbd:`F10`), Step Into (:kbd:`F11`) and Step Out (:kbd:`Shift` + :kbd:`F11`) options. They work the same in both tools.


Examine call stack
------------------

When the script execution is paused, Firebug displays the function call stack within its Stack side panel. In there the functions are listed together with their call parameters. In the DevTools the function call stack is shown within the :ref:`Call Stack side panel <debugger-ui-tour-call-stack>`. To see the call parameters in the DevTools, you need to have a look at the :doc:`Variables side panel <../debugger/how_to/set_watch_expressions/index>`.


Examine variables
-----------------

The Watch side panel in Firebug displays the `window <https://developer.mozilla.org/en-US/docs/Web/API/Window>`_ object (the global scope) by default. With the script execution halted it shows the different variable scopes available within the current call stack frame. Furthermore, it allows you to add and manipulate watch expressions. The DevTools have a :doc:`Variables side panel <../debugger/how_to/set_watch_expressions/index>`, which works basically the same. The main difference is that it is empty when the script execution is not stopped, i.e. it doesn't display the ``window`` object. Though you can inspect that object either via the :doc:`DOM property viewer <../dom_property_viewer/index>` or via the :doc:`Web Console <../web_console/index>`.


Style Editor
************

The :doc:`Style Editor <../style_editor/index>` in the Firefox DevTools allows you to examine and edit the different CSS style sheets of a page like Firebug's CSS panel does it. In addition to that it allows to create new style sheets and to import existing style sheets and apply them to the page. It also allows you to toggle individual style sheets.


Switch between sources
----------------------

The CSS panel of Firebug allows to switch between different CSS sources using the CSS Location Menu. The Style Editor has a :ref:`sidebar <style-editor-the-style-sheet-pane>` for this purpose.


Edit a style sheet
------------------

Firebug's CSS panel offers three different ways for editing style sheets. The default one is to edit them inline like within the Style side panel. Furthermore it has a Source and a Live Edit mode, which allow to edit the selected style sheet like within a text editor. The Style Editor of the DevTools only has one way to edit style sheets, which corresponds to Firebug's Live Edit mode.


Try out CSS selectors
---------------------

Firebug's Selectors side panel provides a way to validate a CSS selector. It lists all elements matching the entered selector. The DevTools don't have this feature yet, but it's requested in `bug 1323746 <https://bugzilla.mozilla.org/show_bug.cgi?id=1323746>`_.


Searching within the style sheets
---------------------------------

Firebug allows to search within the style sheets via the search field. The Style Editor in the DevTools also provides a way to search within a style sheet, though there is currently no option to search within multiple sheets (see `bug 889571 <https://bugzilla.mozilla.org/show_bug.cgi?id=889571>`_ and also not via a regular expression (see `bug 1362030 <https://bugzilla.mozilla.org/show_bug.cgi?id=1362030>`_).


Performance Tool
****************

Firebug allows to profile JavaScript performance via the "Profile" button within the Console panel or the ``console.profile()`` and ``console.profileEnd()`` commands. The DevTools provide advanced tooling regarding performance profiling. A profile can be created via `console.profile() <https://developer.mozilla.org/en-US/docs/Web/API/console/profile>`_ and `console.profileEnd() <https://developer.mozilla.org/en-US/docs/Web/API/console/profileEnd>`_ like in Firebug or via the "Start Recording Performance" button in the :doc:`Performance Tool <../performance/index>`. The output of the :doc:`Call Tree <../performance/call_tree/index>` is the one that comes nearest to the output in Firebug, but the Performance panel provides much more information than just the JavaScript performance. E.g. it also provides information about HTML parsing or layout.

This is the part where Firebug and the DevTools differ the most, because the outputs are completely different. While Firebug focuses on JavaScript performance and provides detailed information about JavaScript function calls during the profiling session, the Performance Tool in the DevTools offers a broad spectrum of information regarding a website's performance but doesn't go into detail regarding JavaScript function calls.


View JavaScript call performance
--------------------------------

What comes nearest to Firebug's profiler output is the :doc:`Call Tree view <../performance/index>` in the Performance panel. Like in Firebug it lists the total execution time of each function call under *Total Time* as well as the number of calls under *Samples*, the time spent within the function under *Self Time* and the related percentages in reference to the total execution time.


.. note::

  The times and percentages listed in the DevTools' Call Tree view is not equivalent to the ones shown in Firebug, because it uses different APIs sampling the execution of the JavaScript code.


Jump to function declaration
----------------------------

Like in Firebug's profiler output the :doc:`Call Tree view <../performance/call_tree/index>` of the DevTools' Performance Tool allows to jump to the line of code where the called JavaScript function is defined. In Firebug the source link to the function is located at the right side of the Console panel output while within the DevTools the link is placed on the right side within the Call Tree View.


Network Monitor
***************

To monitor network requests Firebug provides a Net panel. The Firefox DevTools allow to inspect the network traffic using the :doc:`Network Monitor <../network_monitor/index>`. Both tools provide similar information including a timeline showing the request and response times of the network requests.


Inspect request information
---------------------------

Both Firebug and the Firefox DevTools' Network Monitor allow you to inspect the information about a request by clicking on it. The only difference is that Firebug shows the information below the request while the Network Monitor displays it within a side panel.

In both tools there are different tabs containing different kinds of information for the selected request. They contain a *Headers*, *Params*, *Response* and *Cookies* panel. A preview of the response is shown within specifically named panels like *HTML*. The Network Monitor has a *Preview* panel for this purpose. It doesn't provide information about the cached data yet (see `bug 859051 <https://bugzilla.mozilla.org/show_bug.cgi?id=859051>`_), but provides a *Security* tab in addition to Firebug's information and a *Timings* tab showing detailed information about the network timings.


View request timings
--------------------

Firebug offers detailed information about the network timings related to a request by hovering the Timeline column within its Net panel. The Network Monitor shows this information within a :ref:`Timings side panel <network-monitor-request-details-timings-tab>` when you select a request.


View remote address
-------------------

The remote address of a request is shown within the Remote IP column within Firebug. In the Network Monitor the address is shown at *Remote Address* in the *Headers* tab when a request is selected.


Search within requests
----------------------

The search field within Firebug allows to search within the requests. The search field in the Firefox DevTools filters the requests by the entered string.

Firebug allowed to search within the response body of the network requests by checking *Response Bodies* within its search field options. This feature is not available yet within the Network Monitor, but it's requested in `bug 1334408 <https://bugzilla.mozilla.org/show_bug.cgi?id=1334408>`_. While response bodies can't be searched yet, the Network Monitor allows to :ref:`filter by different request properties <request-list-filtering-by-properties>`.


Storage Inspector
*****************

The Cookies panel in Firebug displays information related to the cookies created by a page and allows to manipulate the information they store. Within the DevTools this functionality is located within the :doc:`Storage Inspector <../storage_inspector/index>`. In contrast to Firebug the Storage Inspector not only allows to inspect cookies but also other kinds of storages like the local and session storage, the cache and `IndexedDB <https://developer.mozilla.org/en-US/docs/Web/API/IndexedDB_API>`_ databases.


Inspect cookies
---------------

All cookies related to a website are listed inside the Cookies panel in Firebug. Inside the DevTools, the cookies are grouped by domain under the Cookies section within the :doc:`Storage Inspector <../storage_inspector/index>`. Both show pretty much the same information per cookie, i.e. the name, value, domain, path, expiration date and whether the cookie is HTTP-only.

The DevTools don't show by default whether a cookie is secure, but this can be enabled by right-clicking the table header and checking *Secure* from the context menu. Additionally, the DevTools allow to display the creation date of a cookie as well as when it was last accessed and whether it is host-only.


Edit cookies
------------

To edit a cookie in Firebug you have to right-click the cookie and choose *Edit* from the context menu. Then a dialog pops up allowing you to edit the data of the cookie and save it. Inside the Storage Inspector you just have to double-click the data you want to edit. Then an inline editor allows you to edit the value.

Delete cookies
--------------

Firebug's Cookies panel allows you to delete all cookies of a website via the menu option *Cookies* > *Remove Cookies* or by pressing :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`O`. It also allows you to only remove session cookies via *Cookies* > *Remove Session Cookies* and to remove single cookies by right-clicking them and choosing *Delete*. The DevTools Storage Inspector allows to remove all cookies and a single one by right-clicking on a cookie and choosing *Delete All* resp. *Delete "<cookie name>"*. Additionally, it allows to delete all cookies from a specific domain via the context menu option *Delete All From "<domain name>"*. It currently does not allow to only delete session cookies (see `bug 1336934 <https://bugzilla.mozilla.org/show_bug.cgi?id=1336934>`_).


Feedback
********

We are always happy to respond to feedback and questions. If you have any queries or points of view, feel free to share them on our `DevTools Discourse Forum <https://discourse.mozilla.org/c/devtools>`_.
