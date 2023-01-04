PR_JoinThreadPool
=================

Waits for all threads in a thread pool to complete, then releases
resources allocated to the thread pool.


Syntax
------

.. code::

   #include <prtpool.h>

   NSPR_API(PRStatus) PR_JoinThreadPool( PRThreadPool *tpool );


Parameter
~~~~~~~~~

The function has the following parameter:

``tpool``
   A pointer to a :ref:`PRThreadPool` structure previously created by a
   call to :ref:`PR_CreateThreadPool`.


Returns
~~~~~~~

:ref:`PRStatus`
