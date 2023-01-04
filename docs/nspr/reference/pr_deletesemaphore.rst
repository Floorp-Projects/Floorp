PR_DeleteSemaphore
==================

Removes a semaphore specified by name from the system.


Syntax
------

.. code::

   #include <pripcsem.h>

   NSPR_API(PRStatus) PR_DeleteSemaphore(const char *name);


Parameter
~~~~~~~~~

The function has the following parameter:

``name``
   The name of a semaphore that was previously created via a call to
   :ref:`PR_OpenSemaphore`.


Returns
~~~~~~~

:ref:`PRStatus`
