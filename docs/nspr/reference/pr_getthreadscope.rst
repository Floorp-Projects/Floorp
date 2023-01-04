PR_GetThreadScope
=================

Gets the scoping of the current thread.


Syntax
------

.. code::

   #include <prthread.h>

   PRThreadScope PR_GetThreadScope(void);


Returns
~~~~~~~

A value of type :ref:`PRThreadScope` indicating whether the thread is local
or global.
