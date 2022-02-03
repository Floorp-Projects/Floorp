===================
Web Console Helpers
===================

The JavaScript command line provided by the Web Console offers a few built-in helper functions that make certain tasks easier.

$(selector, element)
  Looks up a CSS selector string ``selector`` , returning the first node descended from ``element`` that matches. If unspecified, ``element`` defaults to ``document``. Equivalent to `document.querySelector() <https://developer.mozilla.org/en-US/docs/Web/API/Document/querySelector>`_ or calls the $ function in the page, if it exists.

  See the `QuerySelector code snippet <https://developer.mozilla.org/en-US/docs/Web/API/Document/querySelector>`_.

.. _web_console_helpers_$$:

$$(selector, element)
  Looks up a CSS selector string ``selector``, returning an array of DOMnodesdescended from ``element`` that match. If unspecified, ``element`` defaults to ``document``. This is like for `document.querySelectorAll() <https://developer.mozilla.org/en-US/docs/Web/API/Document/querySelectorAll>`_, but returns an array instead of a `NodeList <https://developer.mozilla.org/en-US/docs/Web/API/NodeList>`_.

.. _web_console_helpers_$0:

$0
  The currently-inspected element in the page.

.. _web_console_helpers_$:

$_
  Stores the result of the last expression executed in the console's command line. For example, if you type "2+2 <enter>", then "$_ <enter>", the console will print 4.

$x(xpath, element, resultType)
  Evaluates the `XPath <https://developer.mozilla.org/en-US/docs/Web/XPath>`_ ``xpath`` expression in the context of ``element`` and returns an array of matching nodes. If unspecified, ``element`` defaults to ``document``. The resultType parameter specifies the type of result to return; it can be an `XPathResult constant <https://developer.mozilla.org/en-US/docs/Web/API/XPathResult#constants>`_, or a corresponding string: ``"number"``, ``"string"``, ``"bool"``, ``"node"``, or ``"nodes"``; if not provided, ``ANY_TYPE`` is used.

:block
  (Starting in Firefox 80) Followed by an unquoted string, blocks requests where the URL contains that string. In the :doc:`Network Monitor <../../network_monitor/index>`, the string now appears and is selected in the :ref:`Request Blocking sidebar <network_monitor_blocking_specific_urls>`. Unblock with ``:unblock``.

clear()
  Clears the console output area.

clearHistory()
  Just like a normal command line, the console command line :ref:`remembers the commands you've typed <command_line_interpreter_execution_history>`. Use this function to clear the console's command history.

.. _web_console_helpers_copy:

copy()
  Copies the argument to the clipboard. If the argument is a string, it's copied as-is. If the argument is a DOM node, its `outerHTML <https://developer.mozilla.org/en-US/docs/Web/API/Element/outerHTML>`_ is copied. Otherwise, `JSON.stringify <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/stringify>`_ will be called on the argument, and the result will be copied to the clipboard.

**help()** (deprecated)

.. _web_console_helpers_help:

:help
  Displays help text. Actually, in a delightful example of recursion, it brings you to this page.

inspect()
  Given an object, generates:doc:`rich output <../rich_output/index>` for that object. Once you select the object in the output area, you can use the arrow keys to navigate the object.

keys()
  Given an object, returns a list of the keys (or property names) on that object. This is a shortcut for `Object.keys <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/keys>`_.

pprint() (deprecated)
  Formats the specified value in a readable way; this is useful for dumping the contents of objects and arrays.

:screenshot
  Creates a screenshot of the current page with the supplied filename. If you don't supply a filename, the image file will be named with the following format:

  ``Screen Shot yyy-mm-dd at hh.mm.ss.png``

  The command has the following optional parameters:

.. list-table::
  :widths: 20 20 60
  :header-rows: 1

  * - Command
    - Type
    - Description

  * - ``--clipboard``
    - boolean
    - When present, this parameter will cause the screenshot to be copied to the clipboard.

  * - ``--dpr``
    - number
    - The device pixel ratio to use when taking the screenshot.

  * - ``--file``
    - boolean
    - When present, the screenshot will be saved to a file, even if other options (e.g. ``--clipboard``) are included.

  * - ``--filename``
    - string
    - The name to use in saving the file. The file should have a ".png" extension.

  * - ``--fullpage``
    - boolean
    - If included, the full webpage will be saved. With this parameter, even the parts of the webpage which are outside the current bounds of the window will be included in the screenshot. When used, ``-fullpage`` will be appended to the file name.

  * - ``--selector``
    - string
    - The CSS query selector for a single element on the page. When supplied, only this element will be included in the screenshot.


:unblock
  (Starting in Firefox 80) Followed by an unquoted string, removes blocking for URLs containing that string. In the :doc:`Network Monitor <../../network_monitor/index>`, the string is removed from the :ref:`Request Blocking sidebar <network_monitor_blocking_specific_urls>`. No error is given if the string was not previously blocked.

values()
  Given an object, returns a list of the values on that object; serves as a companion to ``keys()``.


Please refer to the `Console API <https://developer.mozilla.org/en-US/docs/Web/API/console>`_ for more information about logging from content.


Variables
*********

.. _web_console_helpers_tempn:

temp*N*
  The :ref:`Use in Console <page_inspector_how_to_examine_and_edit_html_use_in_console>` option in the Inspector generates a variable for a node named ``temp0``, ``temp1``, ``temp2``, etc. referencing the node.


Examples
********

Looking at the contents of a DOMnode
------------------------------------

Let's say you have a DOMnode with the class"title". In fact, this page you're reading right now has one, so you can open up the Web Console and try this right now.

Let's take a look at the contents of that node by using the ``$()`` and ``inspect()`` functions:

.. code-block:: javascript

  inspect($(".title"))


This automatically generates rich output for the object, showing you the contents of the first DOMnode that matches the CSS selector ``".title"``, which is of course the first element with class ``"title"``. You can use the up- and down-arrow keys to navigate through the output, the right-arrow key to expand an item, and the left-arrow key to collapse it.


See also
********

- `console <https://developer.mozilla.org/en-US/docs/Web/API/console>`_
