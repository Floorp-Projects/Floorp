PR_ShutdownThreadPool
=====================

Notifies all threads in a thread pool to terminate.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prtpool.h>

   NSPR_API(PRStatus) PR_ShutdownThreadPool( PRThreadPool *tpool );

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``tpool``
   A pointer to a :ref:`PRThreadPool` structure previously created by a
   call to :ref:`PR_CreateThreadPool`.

.. _Returns:

Returns
~~~~~~~

:ref:`PRStatus`
