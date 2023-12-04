``logging::ffi``
================

.. js:class:: ICU4XLogger

    An object allowing control over the logging used


    .. js:function:: init_simple_logger()

        Initialize the logger using ``simple_logger``

        Requires the ``simple_logger`` Cargo feature.

        Returns ``false`` if there was already a logger set.


    .. js:function:: init_console_logger()

        Deprecated: since ICU4X 1.4, this now happens automatically if the ``log`` feature is enabled.

