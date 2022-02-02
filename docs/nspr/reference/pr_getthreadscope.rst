PR_GetThreadScope
=================

Gets the scoping of the current thread.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prthread.h>

   PRThreadScope PR_GetThreadScope(void);

.. _Returns:

Returns
~~~~~~~

A value of type :ref:`PRThreadScope` indicating whether the thread is local
or global.
