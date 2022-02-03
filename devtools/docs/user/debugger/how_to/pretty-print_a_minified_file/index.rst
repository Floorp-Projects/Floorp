============================
Pretty-print a minified file
============================

To prettify a minified file, click the **Pretty print source** icon |image1| at the bottom of the :ref:`source pane <debugger_ui_tour_source_pane>`. The debugger formats the source and displays it as a new file with a name like: "{ } [original-name]".

.. |image1| image:: pretty_print_icon.png
  :width: 20

.. image:: pretty_print_source.png
  :class: border

After you click the icon, the source code looks like this:

.. image:: pretty_print_after.png
  :class: border

The **Pretty print source** icon is available only if the source file is minified (i.e., not an original file), and is not already "prettified".

.. note::

  Currently Firefox `does not support <https://bugzilla.mozilla.org/show_bug.cgi?id=1010150>`_ pretty printing inline Javascript.
