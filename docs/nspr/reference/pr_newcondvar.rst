PR_NewCondVar
=============

Creates a new condition variable.


Syntax
------

.. code::

   #include <prcvar.h>

   PRCondVar* PR_NewCondVar(PRLock *lock);


Parameter
~~~~~~~~~

:ref:`PR_NewCondVar` has one parameter:

``lock``
   The identity of the mutex that protects the monitored data, including
   this condition variable.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, a pointer to the new condition variable object.
-  If unsuccessful (for example, if system resources are unavailable),
   ``NULL``.
