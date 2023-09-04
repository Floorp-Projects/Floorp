======================
All keyboard shortcuts
======================

This page lists all keyboard shortcuts used by the developer tools built into Firefox.

The first section lists the shortcut for opening each tool and the second section lists shortcuts that are applicable to the Toolbox itself. After that there's one section for each tool, which lists the shortcuts that you can use within that tool.

Because access keys are locale-dependent, they're not documented in this page.


.. |br| raw:: html

  <br/>


.. _keyboard-shortcuts-opening-and-closing-tools:

Opening and closing tools
*************************

These shortcuts work in the main browser window to open the specified tool. The same shortcuts will work to close tools hosted in the Toolbox, if the tool is active. For tools like the Browser Console that open in a new window, you have to close the window to close the tool.

.. list-table::
  :widths: 25 25 25 25
  :header-rows: 1

  * - **Command**
    - **Windows**
    - **macOS**
    - **Linux**

  * - Open Toolbox (with the most recent tool activated)
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`I`
    - :kbd:`Cmd` + :kbd:`Opt` + :kbd:`I`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`I`

  * - Bring Toolbox to foreground (if the Toolbox is in a separate window and not in foreground)
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`I` or :kbd:`F12`
    - :kbd:`Cmd` + :kbd:`Opt` + :kbd:`I` or :kbd:`F12`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`I` or :kbd:`F12`

  * - Close Toolbox (if the Toolbox is in a separate window and in foreground)
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`I` or :kbd:`F12`
    - :kbd:`Cmd` + :kbd:`Opt` + :kbd:`I` or :kbd:`F12`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`I` or :kbd:`F12`

  * - Open Web Console [#]_
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`K`
    - :kbd:`Cmd` + :kbd:`Opt` + :kbd:`K`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`K`

  * - Toggle "Pick an element from the page" (opens the Toolbox and/or focus the Inspector tab)
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`C`
    - :kbd:`Cmd` + :kbd:`Opt` + :kbd:`C`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`C`

  * - Open Style Editor
    - :kbd:`Shift` + :kbd:`F7`
    - :kbd:`Shift` + :kbd:`F7`
    - :kbd:`Shift` + :kbd:`F7`

  * - Open Profiler
    - :kbd:`Shift` + :kbd:`F5`
    - :kbd:`Shift` + :kbd:`F5`
    - :kbd:`Shift` + :kbd:`F5`

  * - Open Network Monitor [#]_
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`E`
    - :kbd:`Cmd` + :kbd:`Opt` + :kbd:`E`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`E`

  * - Toggle Responsive Design Mode
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`M`
    - :kbd:`Cmd` + :kbd:`Opt` + :kbd:`M`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`M`

  * - Open Browser Console
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`J`
    - :kbd:`Cmd` + :kbd:`Opt` + :kbd:`J`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`J`

  * - Open Browser Toolbox
    - :kbd:`Ctrl` + :kbd:`Alt` + :kbd:`Shift` + :kbd:`I`
    - :kbd:`Cmd` + :kbd:`Opt` + :kbd:`Shift` + :kbd:`I`
    - :kbd:`Ctrl` + :kbd:`Alt` + :kbd:`Shift` + :kbd:`I`

  * - Storage Inspector
    - :kbd:`Shift` + :kbd:`F9`
    - :kbd:`Shift` + :kbd:`F9`
    - :kbd:`Shift` + :kbd:`F9`

  * - Open Debugger [#]_
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`Z`
    - :kbd:`Cmd` + :kbd:`Opt` + :kbd:`Z`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`Z`


.. [#] Unlike the other toolbox-hosted tools, this shortcut does not also close the Web Console. Instead, it focuses on the Web Console's command line. To close the Web Console, use the global toolbox shortcut of :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`I` (:kbd:`Cmd` + :kbd:`Opt` + :kbd:`I` on a Mac).

.. [#] Before Firefox 55, the keyboard shortcut was :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`Q` (:kbd:`Cmd` + :kbd:`Opt` + :kbd:`Q` on a Mac)

.. [#] Starting in Firefox 71. Before Firefox 66, the letter in this shortcut was :kbd:`S`.


.. _keyboard-shortcuts-toolbox:

Toolbox
*******

Keyboard shortcuts for the :doc:`Toolbox <../tools_toolbox/index>`

These shortcuts work whenever the toolbox is open, no matter which tool is active.


.. list-table::
  :widths: 25 25 25 25
  :header-rows: 1

  * - **Command**
    - **Windows**
    - **macOS**
    - **Linux**

  * - Cycle through tools left to right
    - :kbd:`Ctrl` + :kbd:`]`
    - :kbd:`Cmd` + :kbd:`]`
    - :kbd:`Ctrl` + :kbd:`]`

  * - Cycle through tools right to left
    - :kbd:`Ctrl` + :kbd:`[`
    - :kbd:`Cmd` + :kbd:`[`
    - :kbd:`Ctrl` + :kbd:`[`

  * - Toggle between active tool and settings.
    - :kbd:`F1`
    - :kbd:`F1`
    - :kbd:`F1`

  * - Toggle toolbox between the last 2 :ref:`docking modes <tools-toolbox-docking-mode>`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`D`
    - :kbd:`Cmd` + :kbd:`Shift` + :kbd:`D`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`D`

  * - Toggle split console (except if console is the currently selected tool)
    - :kbd:`Esc`
    - :kbd:`Esc`
    - :kbd:`Esc`


These shortcuts work in all tools that are hosted in the toolbox.

.. list-table::
  :widths: 25 25 25 25
  :header-rows: 1

  * - **Command**
    - **Windows**
    - **macOS**
    - **Linux**

  * - Increase font size
    - :kbd:`Ctrl` + :kbd:`+`
    - :kbd:`Cmd` + :kbd:`+`
    - :kbd:`Ctrl` + :kbd:`+`

  * - Decrease font size
    - :kbd:`Ctrl` + :kbd:`-`
    - :kbd:`Cmd` + :kbd:`-`
    - :kbd:`Ctrl` + :kbd:`-`

  * - Reset font size
    - :kbd:`Ctrl` + :kbd:`0`
    - :kbd:`Cmd` + :kbd:`0`
    - :kbd:`Ctrl` + :kbd:`0`


Source editor
*************

This table lists the default shortcuts for the source editor.

In the :ref:`Editor Preferences <settings-editor-preferences>` section of the developer tools settings, you can choose to use Vim, Emacs, or Sublime Text key bindings instead.

To select these, visit ``about:config``, select the setting ``devtools.editor.keymap``, and assign "vim" or "emacs", or "sublime" to that setting. If you do this, the selected bindings will be used for all the developer tools that use the source editor. You need to reopen the editor for the change to take effect.

From Firefox 33 onwards, the key binding preference is exposed in the :ref:`Editor Preferences <settings-editor-preferences>` section of the developer tools settings, and you can set it there instead of ``about:config``.


.. list-table::
  :widths: 25 25 25 25
  :header-rows: 1

  * - **Command**
    - **Windows**
    - **macOS**
    - **Linux**

  * - Go to line
    - :kbd:`Ctrl` + :kbd:`J`, :kbd:`Ctrl` + :kbd:`G`
    - :kbd:`Cmd` + :kbd:`J`, :kbd:`Cmd` + :kbd:`G`
    - :kbd:`Ctrl` + :kbd:`J`, :kbd:`Ctrl` + :kbd:`G`

  * - Find in file
    - :kbd:`Ctrl` + :kbd:`F`
    - :kbd:`Cmd` + :kbd:`F`
    - :kbd:`Ctrl` + :kbd:`F`

  * - Select all
    - :kbd:`Ctrl` + :kbd:`A`
    - :kbd:`Cmd` + :kbd:`A`
    - :kbd:`Ctrl` + :kbd:`A`

  * - Cut
    - :kbd:`Ctrl` + :kbd:`X`
    - :kbd:`Cmd` + :kbd:`X`
    - :kbd:`Ctrl` + :kbd:`X`

  * - Copy
    - :kbd:`Ctrl` + :kbd:`C`
    - :kbd:`Cmd` + :kbd:`C`
    - :kbd:`Ctrl` + :kbd:`C`

  * - Paste
    - :kbd:`Ctrl` + :kbd:`V`
    - :kbd:`Cmd` + :kbd:`V`
    - :kbd:`Ctrl` + :kbd:`V`

  * - Undo
    - :kbd:`Ctrl` + :kbd:`Z`
    - :kbd:`Cmd` + :kbd:`Z`
    - :kbd:`Ctrl` + :kbd:`Z`

  * - Redo
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`Z` / :kbd:`Ctrl` + :kbd:`Y`
    - :kbd:`Cmd` + :kbd:`Shift` + :kbd:`Z` / :kbd:`Cmd` + :kbd:`Y`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`Z` / :kbd:`Ctrl` + :kbd:`Y`

  * - Indent
    - :kbd:`Tab`
    - :kbd:`Tab`
    - :kbd:`Tab`

  * - Unindent
    - :kbd:`Shift` + :kbd:`Tab`
    - :kbd:`Shift` + :kbd:`Tab`
    - :kbd:`Shift` + :kbd:`Tab`

  * - Move line(s) up
    - :kbd:`Alt` + :kbd:`Up`
    - :kbd:`Alt` + :kbd:`Up`
    - :kbd:`Alt` + :kbd:`Up`

  * - Move line(s) down
    - :kbd:`Alt` + :kbd:`Down`
    - :kbd:`Alt` + :kbd:`Down`
    - :kbd:`Alt` + :kbd:`Down`

  * - Comment/uncomment line(s)
    - :kbd:`Ctrl` + :kbd:`/`
    - :kbd:`Cmd` + :kbd:`/`
    - :kbd:`Ctrl` + :kbd:`/`


.. _keyboard-shortcuts-page-inspector:

Page Inspector
**************

Keyboard shortcuts for the :doc:`Page inspector <../page_inspector/index>`.

.. list-table::
  :widths: 25 25 25 25
  :header-rows: 1

  * - **Command**
    - **Windows**
    - **macOS**
    - **Linux**

  * - Inspect Element
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`C`
    - :kbd:`Cmd` + :kbd:`Shift` + :kbd:`C`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`C`


Node picker
***********

These shortcuts work while the :ref:`node picker <page-inspector-how-to-select-an-element-with-the-node-picker>` is active.

.. list-table::
  :widths: 25 25 25 25
  :header-rows: 1

  * - **Command**
    - **Windows**
    - **macOS**
    - **Linux**

  * - Select the element under the mouse and cancel picker mode
    - :kbd:`Click`
    - :kbd:`Click`
    - :kbd:`Click`

  * - Select the element under the mouse and stay in picker mode
    - :kbd:`Shift` + :kbd:`Click`
    - :kbd:`Shift` + :kbd:`Click`
    - :kbd:`Shift` + :kbd:`Click`


.. _keyboard-shortcuts-html-pane:

HTML pane
*********

These shortcuts work while you're in the :doc:`Inspector's HTML pane <../page_inspector/how_to/examine_and_edit_html/index>`.

.. list-table::
  :widths: 40 20 20 20
  :header-rows: 1

  * - **Command**
    - **Windows**
    - **macOS**
    - **Linux**

  * - Delete the selected node
    - :kbd:`Delete`
    - :kbd:`Delete`
    - :kbd:`Delete`

  * - Undo delete of a node
    - :kbd:`Ctrl` + :kbd:`Z`
    - :kbd:`Cmd` + :kbd:`Z`
    - :kbd:`Ctrl` + :kbd:`Z`

  * - Redo delete of a node
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`Z` / :kbd:`Ctrl` + :kbd:`Y`
    - :kbd:`Cmd` + :kbd:`Shift` + :kbd:`Z` / :kbd:`Cmd` + :kbd:`Y`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`Z` / :kbd:`Ctrl` + :kbd:`Y`

  * - Move to next node (expanded nodes only)
    - :kbd:`↓`
    - :kbd:`↓`
    - :kbd:`↓`

  * - Move to previous node
    - :kbd:`↑`
    - :kbd:`↑`
    - :kbd:`↑`

  * - Move to first node in the tree.
    - :kbd:`Home`
    - :kbd:`Home`
    - :kbd:`Home`

  * - Move to last node in the tree.
    - :kbd:`End`
    - :kbd:`End`
    - :kbd:`End`

  * - Expand currently selected node
    - :kbd:`→`
    - :kbd:`→`
    - :kbd:`→`

  * - Collapse currently selected node
    - :kbd:`←`
    - :kbd:`←`
    - :kbd:`←`

  * - (When a node is selected) move inside the node so you can start stepping through attributes.
    - :kbd:`Enter`
    - :kbd:`Return`
    - :kbd:`Enter`

  * - Step forward through the attributes of a node
    - :kbd:`Tab`
    - :kbd:`Tab`
    - :kbd:`Tab`

  * - Step backward through the attributes of a node
    - :kbd:`Shift` + :kbd:`Tab`
    - :kbd:`Shift` + :kbd:`Tab`
    - :kbd:`Shift` + :kbd:`Tab`

  * - (When an attribute is selected) start editing the attribute
    - :kbd:`Enter`
    - :kbd:`Return`
    - :kbd:`Enter`

  * - Hide/show the selected node
    - :kbd:`H`
    - :kbd:`H`
    - :kbd:`H`

  * - Focus on the search box in the HTML pane
    - :kbd:`Ctrl` + :kbd:`F`
    - :kbd:`Cmd` + :kbd:`F`
    - :kbd:`Ctrl` + :kbd:`F`

  * - Edit as HTML
    - :kbd:`F2`
    - :kbd:`F2`
    - :kbd:`F2`

  * - Stop editing HTML
    - :kbd:`F2` / :kbd:`Ctrl` +:kbd:`Enter`
    - :kbd:`F2` / :kbd:`Cmd` + :kbd:`Return`
    - :kbd:`F2` / :kbd:`Ctrl` + :kbd:`Enter`

  * - Copy the selected node's outer HTML
    - :kbd:`Ctrl` + :kbd:`C`
    - :kbd:`Cmd` + :kbd:`C`
    - :kbd:`Ctrl` + :kbd:`C`

  * - Scroll the selected node into view
    - :kbd:`S`
    - :kbd:`S`
    - :kbd:`S`

  * - Find the next match in the markup, when searching is active
    - :kbd:`Enter`
    - :kbd:`Return`
    - :kbd:`Enter`

  * - Find the previous match in the markup, when searching is active
    - :kbd:`Shift` + :kbd:`Enter`
    - :kbd:`Shift` + :kbd:`Return`
    - :kbd:`Shift` + :kbd:`Enter`


.. _keyboard-shortcuts-breadcrumbs-bar:

Breadcrumbs bar
***************

These shortcuts work when the :ref:`breadcrumbs bar <page-inspector-how-to-examine-and-edit-html-breadcrumbs>` is focused.

.. list-table::
  :widths: 40 20 20 20
  :header-rows: 1

  * - **Command**
    - **Windows**
    - **macOS**
    - **Linux**

  * - Move to the previous element in the breadcrumbs bar
    - :kbd:`←`
    - :kbd:`←`
    - :kbd:`←`

  * - Move to the next element in the breadcrumbs bar
    - :kbd:`→`
    - :kbd:`→`
    - :kbd:`→`

  * - Focus the :ref:`HTML pane <page_inspector_ui_tour_html_pane>`
    - :kbd:`Shift` + :kbd:`Tab`
    - :kbd:`Shift` + :kbd:`Tab`
    - :kbd:`Shift` + :kbd:`Tab`

  * - Focus the :ref:`CSS pane <page_inspector_ui_tour_rules_view>`
    - :kbd:`Tab`
    - :kbd:`Tab`
    - :kbd:`Tab`


CSS pane
********

These shortcuts work when you're in the :doc:`Inspector's CSS panel <../page_inspector/how_to/examine_and_edit_css/index>`

.. list-table::
  :widths: 40 20 20 20
  :header-rows: 1

  * - **Command**
    - **Windows**
    - **macOS**
    - **Linux**

  * - Focus on the search box in the CSS pane
    - :kbd:`Ctrl` + :kbd:`F`
    - :kbd:`Cmd` + :kbd:`F`
    - :kbd:`Ctrl` + :kbd:`F`

  * - Clear search box content (only when the search box is focused, and content has been entered)
    - :kbd:`Esc`
    - :kbd:`Esc`
    - :kbd:`Esc`

  * - Step forward through properties and values
    - :kbd:`Tab`
    - :kbd:`Tab`
    - :kbd:`Tab`

  * - Step backward through properties and values
    - :kbd:`Shift` + :kbd:`Tab`
    - :kbd:`Shift` + :kbd:`Tab`
    - :kbd:`Shift` + :kbd:`Tab`

  * - Start editing property or value (Rules view only, when a property or value is selected, but not already being edited)
    - :kbd:`Enter` or :kbd:`Space`
    - :kbd:`Return` or :kbd:`Space`
    - :kbd:`Enter` or :kbd:`Space`

  * - Cycle up and down through auto-complete suggestions (Rules view only, when a property or value is being edited)
    - :kbd:`↑` , :kbd:`↓`
    - :kbd:`↑` , :kbd:`↓`
    - :kbd:`↑` , :kbd:`↓`

  * - Choose current auto-complete suggestion (Rules view only, when a property or value is being edited)
    - :kbd:`Enter` or :kbd:`Tab`
    - :kbd:`Return` or :kbd:`Tab`
    - :kbd:`Enter` or :kbd:`Tab`

  * - Increment selected value by 1
    - :kbd:`↑`
    - :kbd:`↑`
    - :kbd:`↑`

  * - Decrement selected value by 1
    - :kbd:`↓`
    - :kbd:`↓`
    - :kbd:`↓`

  * - Increment selected value by 100
    - :kbd:`Shift` + :kbd:`PageUp`
    - :kbd:`Shift` + :kbd:`PageUp`
    - :kbd:`Shift` + :kbd:`PageUp`

  * - Decrement selected value by 100
    - :kbd:`Shift` + :kbd:`PageDown`
    - :kbd:`Shift` + :kbd:`PageDown`
    - :kbd:`Shift` + :kbd:`PageDown`

  * - Increment selected value by 10
    - :kbd:`Shift` + :kbd:`↑`
    - :kbd:`Shift` + :kbd:`↑`
    - :kbd:`Shift` + :kbd:`↑`

  * - Decrement selected value by 10
    - :kbd:`Shift` + :kbd:`↓`
    - :kbd:`Shift` + :kbd:`↓`
    - :kbd:`Shift` + :kbd:`↓`

  * - Increment selected value by 0.1
    - :kbd:`Alt` + :kbd:`↑` (:kbd:`Ctrl` + :kbd:`↑` from Firefox 60 onwards.)
    - :kbd:`Alt` + :kbd:`↑`
    - :kbd:`Alt` + :kbd:`↑` (:kbd:`Ctrl` + :kbd:`↑` from Firefox 60 onwards.)

  * - Decrement selected value by 0.1
    - :kbd:`Alt` + :kbd:`↓` (:kbd:`Ctrl` + :kbd:`↓` from Firefox 60 onwards).
    - :kbd:`Alt` + :kbd:`↓`
    - :kbd:`Alt` + :kbd:`↓` (:kbd:`Ctrl` + :kbd:`↓` from Firefox 60 onwards).

  * - Show/hide more information about current property (Computed view only, when a property is selected)
    - :kbd:`Enter` or :kbd:`Space`
    - :kbd:`Return` or :kbd:`Space`
    - :kbd:`Enter` or :kbd:`Space`

  * - Open MDN reference page about current property (Computed view only, when a property is selected)
    - :kbd:`F1`
    - :kbd:`F1`
    - :kbd:`F1`

  * - Open current CSS file in Style Editor (Computed view only, when more information is shown for a property and a CSS file reference is focused).
    - :kbd:`Enter`
    - :kbd:`Return`
    - :kbd:`Enter`


.. _keyboard-shortcuts-debugger:

Debugger
********

Keyboard shortcuts for the :doc:`Firefox JavaScript Debugger <../debugger/index>`.

.. list-table::
  :widths: 25 25 25 25
  :header-rows: 1

  * - **Command**
    - **Windows**
    - **macOS**
    - **Linux**

  * - Close current file
    - :kbd:`Ctrl` + :kbd:`W`
    - :kbd:`Cmd` + :kbd:`W`
    - :kbd:`Ctrl` + :kbd:`W`

  * - Search for a string in the current file
    - :kbd:`Ctrl` + :kbd:`F`
    - :kbd:`Cmd` + :kbd:`F`
    - :kbd:`Ctrl` + :kbd:`F`

  * - Search for a string in all files
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`F`
    - :kbd:`Cmd` + :kbd:`Shift` + :kbd:`F`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`F`

  * - Find next in the current file
    - :kbd:`Ctrl` + :kbd:`G`
    - :kbd:`Cmd` + :kbd:`G`
    - :kbd:`Ctrl` + :kbd:`G`

  * - Search for scripts by name
    - :kbd:`Ctrl` + :kbd:`P`
    - :kbd:`Cmd` + :kbd:`P`
    - :kbd:`Ctrl` + :kbd:`P`

  * - Resume execution when at a breakpoint
    - :kbd:`F8`
    - :kbd:`F8` [4]_
    - :kbd:`F8`

  * - Step over
    - :kbd:`F10`
    - :kbd:`F10` [4]_
    - :kbd:`F10`

  * - Step into
    - :kbd:`F11`
    - :kbd:`F11` [4]_
    - :kbd:`F11`

  * - Step out
    - :kbd:`Shift` + :kbd:`F11`
    - :kbd:`Shift` + :kbd:`F11` [4]_
    - :kbd:`Shift` + :kbd:`F11`

  * - Toggle breakpoint on the currently selected line
    - :kbd:`Ctrl` + :kbd:`B`
    - :kbd:`Cmd` + :kbd:`B`
    - :kbd:`Ctrl` + :kbd:`B`

  * - Toggle conditional breakpoint on the currently selected line
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`B`
    - :kbd:`Cmd` + :kbd:`Shift` + :kbd:`B`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`B`


.. [4] By default, on some Macs, the function key is remapped to use a special feature: for example, to change the screen brightness or the volume. See this `guide to using these keys as standard function keys <https://support.apple.com/kb/HT3399>`_. To use a remapped key as a standard function key, hold the Function key down as well (so to open the Profiler, use :kbd:`Shift` + :kbd:`Function` + :kbd:`F5`).


.. note::
  Before Firefox 66, the combination :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`S` on Windows and Linux or :kbd:`Cmd` + :kbd:`Opt` + :kbd:`S` on macOS would open/close the Debugger. From Firefox 66 and later, this is no longer the case.


.. _keyboard-shortcuts-web-console:

Web Console
***********

Keyboard shortcuts for the :doc:`Web Console <../web_console/index>`.

.. list-table::
  :widths: 25 25 25 25
  :header-rows: 1

  * - **Command**
    - **Windows**
    - **macOS**
    - **Linux**

  * - Open the Web Console
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`K`
    - :kbd:`Cmd` + :kbd:`Opt` + :kbd:`K`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`K`

  * - Search in the message display pane
    - :kbd:`Ctrl` + :kbd:`F`
    - :kbd:`Cmd` + :kbd:`F`
    - :kbd:`Ctrl` + :kbd:`F`

  * - Open the :ref:`object inspector pane <web_console_rich_output_examining_object_properties>`
    - :kbd:`Ctrl` + :kbd:`Click`
    - :kbd:`Ctrl` + :kbd:`Click`
    - :kbd:`Ctrl` + :kbd:`Click`

  * - Clear the :ref:`object inspector pane <web_console_rich_output_examining_object_properties>`
    - :kbd:`Esc`
    - :kbd:`Esc`
    - :kbd:`Esc`

  * - Focus on the command line
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`K`
    - :kbd:`Cmd` + :kbd:`Opt` + :kbd:`K`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`K`

  * - Clear output
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`L`
    - :kbd:`Ctrl` + :kbd:`L` |br| |br| From Firefox 67: |br| |br| :kbd:`Cmd` + :kbd:`K`
    - :kbd:`Ctrl` + :kbd:`Shift` + :kbd:`L`



Command line interpreter
************************

These shortcuts apply when you're in the :doc:`command line interpreter <../web_console/the_command_line_interpreter/index>`.

.. list-table::
  :widths: 25 25 25 25
  :header-rows: 1

  * - **Command**
    - **Windows**
    - **macOS**
    - **Linux**

  * - Scroll to start of console output (only if the command line is empty)
    - :kbd:`Home`
    - :kbd:`Home`
    - :kbd:`Home`

  * - Scroll to end of console output (only if the command line is empty)
    - :kbd:`End`
    - :kbd:`End`
    - :kbd:`End`

  * - Page up through console output
    - :kbd:`PageUp`
    - :kbd:`PageUp`
    - :kbd:`PageUp`

  * - Page down through console output
    - :kbd:`PageDown`
    - :kbd:`PageDown`
    - :kbd:`PageDown`

  * - Go backward through :ref:`command history <command_line_interpreter_execution_history>`
    - :kbd:`↑`
    - :kbd:`↑`
    - :kbd:`↑`

  * - Go forward through command history
    - :kbd:`↓`
    - :kbd:`↓`
    - :kbd:`↓`

  * - Initiate reverse search through command history/step backwards through matching commands
    - :kbd:`F9`
    - :kbd:`Ctrl` + :kbd:`R`
    - :kbd:`F9`

  * - Step forward through matching command history (after initiating reverse search)
    - :kbd:`Shift` + :kbd:`F9`
    - :kbd:`Ctrl` + :kbd:`S`
    - :kbd:`Shift` + :kbd:`F9`

  * - Move to the beginning of the line
    - :kbd:`Home`
    - :kbd:`Ctrl` + :kbd:`A`
    - :kbd:`Ctrl` + :kbd:`A`

  * - Move to the end of the line
    - :kbd:`End`
    - :kbd:`Ctrl` + :kbd:`E`
    - :kbd:`Ctrl` + :kbd:`E`

  * - Execute the current expression
    - :kbd:`Enter`
    - :kbd:`Return`
    - :kbd:`Enter`

  * - Add a new line, for entering multiline expressions
    - :kbd:`Shift` + :kbd:`Enter`
    - :kbd:`Shift` + :kbd:`Return`
    - :kbd:`Shift` + :kbd:`Enter`


Autocomplete popup
******************

These shortcuts apply while the :ref:`autocomplete popup <command_line_interpreter_autocomplete>` is open:

.. list-table::
  :widths: 40 20 20 20
  :header-rows: 1

  * - **Command**
    - **Windows**
    - **macOS**
    - **Linux**

  * - Choose the current autocomplete suggestion
    - :kbd:`Tab`
    - :kbd:`Tab`
    - :kbd:`Tab`

  * - Cancel the autocomplete popup
    - :kbd:`Esc`
    - :kbd:`Esc`
    - :kbd:`Esc`

  * - Move to the previous autocomplete suggestion
    - :kbd:`↑`
    - :kbd:`↑`
    - :kbd:`↑`

  * - Move to the next autocomplete suggestion
    - :kbd:`↓`
    - :kbd:`↓`
    - :kbd:`↓`

  * - Page up through autocomplete suggestions
    - :kbd:`PageUp`
    - :kbd:`PageUp`
    - :kbd:`PageUp`

  * - Page down through autocomplete suggestions
    - :kbd:`PageDown`
    - :kbd:`PageDown`
    - :kbd:`PageDown`

  * - Scroll to start of autocomplete suggestions
    - :kbd:`Home`
    - :kbd:`Home`
    - :kbd:`Home`

  * - Scroll to end of autocomplete suggestions
    - :kbd:`End`
    - :kbd:`End`
    - :kbd:`End`


.. _keyboard-shortcuts-style-editor:

Style Editor
************

Keyboard shortcuts for the :doc:`Style editor <../style_editor/index>`.

.. list-table::
  :widths: 25 25 25 25
  :header-rows: 1

  * - **Command**
    - **Windows**
    - **macOS**
    - **Linux**

  * - Open the Style Editor
    - :kbd:`Shift` + :kbd:`F7`
    - :kbd:`Shift` + :kbd:`F7`
    - :kbd:`Shift` + :kbd:`F7`

  * - Open autocomplete popup
    - :kbd:`Ctrl` + :kbd:`Space`
    - :kbd:`Cmd` + :kbd:`Space`
    - :kbd:`Ctrl` + :kbd:`Space`

  * - Find Next
    - :kbd:`Ctrl` + :kbd:`G`
    - :kbd:`Cmd` + :kbd:`G`
    - :kbd:`Ctrl` + :kbd:`G`

  * - Find Previous
    - :kbd:`Shift` + :kbd:`Ctrl` + :kbd:`G`
    - :kbd:`Shift` + :kbd:`Cmd` + :kbd:`G`
    - :kbd:`Shift` + :kbd:`Ctrl` + :kbd:`G`

  * - Replace
    - :kbd:`Shift` + :kbd:`Ctrl` + :kbd:`F`
    - :kbd:`Cmd` + :kbd:`Option` + :kbd:`F`
    - :kbd:`Shift` + :kbd:`Ctrl` + :kbd:`F`

  * - Focus the filter input
    - :kbd:`Ctrl` + :kbd:`P`
    - :kbd:`Cmd` + :kbd:`P`
    - :kbd:`Ctrl` + :kbd:`P`

  * - Save file to disk
    - :kbd:`Ctrl` + :kbd:`S`
    - :kbd:`Cmd` + :kbd:`S`
    - :kbd:`Ctrl` + :kbd:`S`

.. _keyboard-shortcuts-eyedropper:

Eyedropper
**********

Keyboard shortcuts for the :doc:`Eyedropper <../eyedropper/index>`.

.. list-table::
  :widths: 25 25 25 25
  :header-rows: 1

  * - **Command**
    - **Windows**
    - **macOS**
    - **Linux**

  * - Select the current color
    - :kbd:`Enter`
    - :kbd:`Return`
    - :kbd:`Enter`

  * - Dismiss the Eyedropper
    - :kbd:`Esc`
    - :kbd:`Esc`
    - :kbd:`Esc`

  * - Move by 1 pixel
    - :kbd:`ArrowKeys`
    - :kbd:`ArrowKeys`
    - :kbd:`ArrowKeys`

  * - Move by 10 pixels
    - :kbd:`Shift` + :kbd:`ArrowKeys`
    - :kbd:`Shift` + :kbd:`ArrowKeys`
    - :kbd:`Shift` + :kbd:`ArrowKeys`
