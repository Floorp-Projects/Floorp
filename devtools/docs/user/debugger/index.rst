===============================
The Firefox JavaScript Debugger
===============================

The JavaScript Debugger enables you to step through JavaScript code and examine or modify its state to help track down bugs.

.. raw:: html

  <iframe width="560" height="315" src="https://www.youtube.com/embed/QK4hKWmJVLo" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture" allowfullscreen></iframe>
  <br/>
  <br/>


You can use it to debug code running locally in Firefox or running remotely, for example on an Android device running Firefox for Android. See :doc:`about debugging <../about_colon_debugging/index>` to learn how to connect the debugger to a remote target.

To find your way around the debugger, here's a :doc:`quick tour of the UI<ui_tour/index>`.


How to
******

To find out what you can do with the debugger, refer to the following how-to guides.

- :doc:`Open the debugger <how_to/open_the_debugger/index>`
- :doc:`Pretty-print a minified file <how_to/pretty-print_a_minified_file/index>`
- :doc:`Search <how_to/search/index>`
- :doc:`Use a source map <how_to/use_a_source_map/index>`
- `Debug worker threads <https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API/Using_web_workers#debugging_worker_threads>`_


Pause execution
---------------

You probably want to pause the execution of your code, in order to see what is going on at various points. There are multiple ways to tell the Debugger how and when to pause:


- :doc:`Set a breakpoint <how_to/set_a_breakpoint/index>`
- :doc:`Set a conditional breakpoint <how_to/set_a_conditional_breakpoint/index>`
- :doc:`Set an XHR breakpoint <set_an_xhr_breakpoint/index>`
- :doc:`Set event listener <set_event_listener_breakpoints/index>`
- :doc:`Break on exceptions <how_to/breaking_on_exceptions/index>`
- :doc:`Use watchpoints for property reads and writes <how_to/use_watchpoints/index>`
- :doc:`Break on DOM Mutation <break_on_dom_mutation/index>`
- :doc:`Disable breakpoints <how_to/disable_breakpoints/index>`


Control execution
-----------------

What can you do after execution pauses?

- :doc:`Step through code <how_to/step_through_code/index>`
- :doc:`Black box a source <how_to/ignore_a_source/index>`
- `Debug worker threads <https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API/Using_web_workers#debugging_worker_threads>`_
- :doc:`Debug eval sources <how_to/debug_eval_sources/index>`


Look at values
--------------

You probably want to see the value of variables or expressions, either during execution or when it is paused.

- :doc:`Set a logpoint <set_a_logpoint/index>`
- :doc:`Set watch expressions <how_to/set_watch_expressions/index>`

Reference
*********

- :ref:`Keyboard shortcuts <keyboard-shortcuts-debugger>`
- :doc:`Source map errors <source_map_errors/index>`
