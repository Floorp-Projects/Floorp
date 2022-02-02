PR_CloseSemaphore
=================

Closes a specified semaphore.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <pripcsem.h>

   NSPR_API(PRStatus) PR_CloseSemaphore(PRSem *sem);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``sem``
   A pointer to a ``PRSem`` structure returned from a call to
   :ref:`PR_OpenSemaphore`.

.. _Returns:

Returns
~~~~~~~

:ref:`PRStatus`
