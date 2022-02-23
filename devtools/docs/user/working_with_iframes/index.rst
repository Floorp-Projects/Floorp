====================
Working with iframes
====================

You can point the developer tools at a specific `iframe <https://developer.mozilla.org/en-US/docs/Web/HTML/Element/iframe>`_ within a document. The :doc:`Inspector <../page_inspector/index>`, :doc:`Console <../web_console/index>`, :doc:`Debugger <../debugger/index>` and all other developer tools will then target that iframe (essentially behaving as if the rest of the page does not exist).

.. raw:: html

  <iframe width="560" height="315" src="https://www.youtube.com/embed/Me9hjqd74m8" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture" allowfullscreen></iframe>
  <br/>
  <br/>

To set an iframe as the target for the developer tools:

- Select the *iframe context picker button* to launch a popup listing all the iframes in the document (and the main document itself). Note that the button is only displayed if the page includes iframes!

  .. image:: developer_tools_select_iframe.png
    :alt: Screenshot showing how to set an iframe as the target of developer tools (using the iframe button)
    :class: center

- Select an entry to make it the *target iframe*.


.. note::
  The iframe context picker button feature is enabled by default (if it has been disabled the iframe button is never displayed). The feature can be re-enabled from the Settings menu, using the "Select an iframe as the currently targeted document" checkbox.
