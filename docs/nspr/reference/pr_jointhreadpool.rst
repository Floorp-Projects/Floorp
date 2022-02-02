PR_JoinThreadPool
=================

Waits for all threads in a thread pool to complete, then releases
resources allocated to the thread pool.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prtpool.h>

   NSPR_API(PRStatus) PR_JoinThreadPool( PRThreadPool *tpool );

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
