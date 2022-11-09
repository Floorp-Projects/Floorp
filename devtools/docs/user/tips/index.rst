====
Tips
====

General
*******

Screenshots:

.. |image1| image:: screenshot_button.png
  :alt: Button to take screenshots of the entire page
  :width: 20

- Entire page: Click the screenshot button (|image1| :ref:`needs to be enabled <tools-toolbox-extra-tools>` first).
- Viewport: Click the screenshot button (|image1|) in :ref:`Responsive Design Mode <responsive-design-mode-camera-button>`.
- Node: Right-click a node within the Inspector and click :ref:`"Screenshot Node" <taking-screenshots-of-an-element>`.
- Via :doc:`Web Console command <../web_console/helpers/index>`: ``screenshot <filename.png> --fullpage``.


Settings:

- Choose between a :ref:`light and a dark theme <settings-themes>` for the developer tools.
- :ref:`Change the keyboard bindings <settings-editor-preferences>` to Vim, Emacs or Sublime Text if you're used to different shortcuts.
- Check or uncheck the different tools to enable or disable them. (There are more than the default tools!)


Page Inspector
**************

In the :ref:`markup view <page_inspector_ui_tour_html_pane>`:


- Press :kbd:`H` with a node selected to hide/show it.
- Press :kbd:`Backspace` or :kbd:`Del` with a node selected to delete it.
- :kbd:`Alt` + click on a node to expand or collapse it and all its descendants.
- Click on the last :ref:`breadcrumb button <page-inspector-how-to-examine-and-edit-html-breadcrumbs>` to scroll the selection into view in the inspector.
- Click the "ev" icon besides a node to :doc:`see all event listeners attached to it <../page_inspector/examine_event_listeners/index>`.
- Press :kbd:`S` with a node selected to see it in the page (same as right-click a node and click :ref:`Scroll Into View <page_inspector_how_to_examine_and_edit_scroll_into_view>`).
- Right-click a node and click :ref:`Use in Console<page_inspector_how_to_examine_and_edit_html_use_in_console>` to :doc:`command line <../web_console/the_command_line_interpreter/index>` as ``tempN`` variable.


When :ref:`selecting elements <page_inspector_select_element_button>`:


- :kbd:`Shift` + click to select an element but keep selecting (picker mode doesn't disengage).
- Use :kbd:`←`/:kbd:`→` to navigate to parents/children elements (if they're hard to select).


In the :ref:`rules view <page_inspector_ui_tour_rules_view>`:

.. |image2| image:: picker.png
  :width: 20

.. |image3| image:: filter.png
  :width: 20

- Click the inspector icon |image2| next to any selector to highlight all elements that match it.
- Click the inspector icon |image2| next to the ``element{}`` rule to lock the highlighter on the current element.
- Right-click any property and select "Show MDN Docs" to view the MDN docs for this property.
- Click on the filter icon |image3| next to an overridden property to :ref:`find which other property overrides it <page-inspector-how-to-examine-and-edit-css-overridden-declarations>`.
- Right-click on a name, value, or rule to copy anything from the name, the value, the declaration or the whole rule to your clipboard.


Web Console
***********

In all panels:

- :kbd:`Esc` opens the :doc:`split console <../web_console/split_console/index>`; useful when debugging, or inspecting nodes


In the :doc:`command line <../web_console/the_command_line_interpreter/index>`:


- :ref:`$0 <web_console_helpers_$0>` references the currently selected node.
- :ref:`$() <web_console_helpers_$>` is a shortcut to `document.querySelector() <https://developer.mozilla.org/en-US/docs/Web/API/Document/querySelector>`_
- :ref:`$$() <web_console_helpers_$$>` returns an array with the results from `document.querySelector() <https://developer.mozilla.org/en-US/docs/Web/API/Document/querySelector>`_.
- :ref:`copy <web_console_helpers_copy>` copies anything as a string.
- Right-click a node in the Inspector and click :ref:`Use in Console <page_inspector_how_to_examine_and_edit_html_use_in_console>` to create a :ref:`temp*N* <web_console_helpers_tempn>` variable for it.
- `console.table() <https://developer.mozilla.org/en-US/docs/Web/API/console/table>`_ displays tabular data as table.
- :ref:`help <web_console_helpers_help>` opens the MDN page describing the available commands.
- ``:screenshot <filename.png> --fullpage`` saves a screenshot to your downloads directory using the optional file name. If no filename is included, the file name will be in this format:

.. code-block::

  Screen Shot date at time.png

The ``--fullpage`` parameter is optional. If you include it, the screenshot will be of the whole page, not just the section visible in the browser windows. The file name will have ``-fullpage`` appended to it as well. See :doc:`Web Console Helpers <../web_console/helpers/index>` for all parameters.


In the console output:


- Click on the inspector icon |image2| next to an element in the output to :ref:`select it within the Inspector <web_console_rich_output_highlighting_and_inspecting_dom_nodes>`.
- Check :ref:`"Enable persistent logs" <settings-common-preferences>` in the settings to keep logged messages from before even after navigation.
- Check :ref:`"Enable timestamps" <settings-web-console>` in the settings to show timestamps besides the logged messages.


Debugger
********

.. |image4| image:: black_boxing.png
  :width: 20


- Skip JavaScript libraries in debugging sessions via the black box icon |image4|.
- Press :kbd:`Ctrl` + :kbd:`Alt` + :kbd:`F` to search in all scripts.
- Press :kbd:`Ctrl` + :kbd:`D` to search for a function definition.
- Press :kbd:`Ctrl` + :kbd:`L` to go to a specific line.


Style Editor
************

.. |image5| image:: import_button.png
  :alt: Button to import a style sheet from the file system
  :width: 20

.. |image6| image:: create_style_sheet_button.png
  :alt: Button to create a new style sheet
  :width: 20

- The black box icon |image4| in the style sheet pane toggles the visibility of a style sheet.
- Click an :ref:`@media rule <style-editor-the-at-rules-sidebar>` to apply it in :doc:`Responsive Design Mode <../responsive_design_mode/index>`.
- Click the import button |image5| to import a style sheet or the create button |image6| to create a new one.
- Click the options button in the :ref:`style sheet pane <style-editor-the-style-sheet-pane>` and click :ref:`"Show original sources" <style-editor-source-map-support>` to toggle the display of CSS preprocessor files.


Network Monitor
***************

- Click the request summary to :doc:`compare performance of cache vs. no-cache page loading <../network_monitor/performance_analysis/index>`.
- When a request is selected click :ref:`"Edit and Resend" <network-monitor-request-list-edit-and-resend>` to modify its headers and send it again.
- Check :ref:`"enable persistent logs" <settings-common-preferences>` in the settings to keep requests from before even after navigation.
- Hover the :ref:`"js" icon within the "Cause" column <request-list-requst-list-cause-column>` to see the JavaScript stack trace, which caused the request.
- Check :ref:`"Disable HTTP Cache (when toolbox is open)" <settings_advanced_settings>` in the settings to disable the network cache while debugging network issues.


Storage Inspector
*****************

- Right-click the column headers to open a menu allowing to toggle the display of the columns.
- Right-click an entry and click "Delete *name*" to delete it or "Delete All" to delete all entries.
- Select an entry to see the parsed value of it in the sidebar.
