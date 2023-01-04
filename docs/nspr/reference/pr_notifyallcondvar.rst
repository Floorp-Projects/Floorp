PR_NotifyAllCondVar
===================

Notifies all of the threads waiting on a specified condition variable.


Syntax
------

.. code::

   #include <prcvar.h>

   PRStatus PR_NotifyAllCondVar(PRCondVar *cvar);


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful (for example, if the caller has not locked the lock
   associated with the condition variable), ``PR_FAILURE``.


Description
-----------

The calling thread must hold the lock that protects the condition, as
well as the invariants that are tightly bound to the condition.

A call to :ref:`PR_NotifyAllCondVar` causes all of the threads waiting on
the specified condition variable to be promoted to a ready state. If no
threads are waiting, the operation is no-op.
