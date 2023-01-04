PR_QueueJob
===========

Queues a job to a thread pool for execution.


Syntax
------

.. code::

   #include <prtpool.h>

   NSPR_API(PRJob *)
   PR_QueueJob(
     PRThreadPool *tpool,
     PRJobFn fn,
     void *arg,
     PRBool joinable
   );


Parameters
~~~~~~~~~~

The function has the following parameters:

``tpool``
   A pointer to a :ref:`PRThreadPool` structure previously created by a
   call to :ref:`PR_CreateThreadPool`.
``fn``
   The function to be executed when the job is executed.
``arg``
   A pointer to an argument passed to ``fn``.
``joinable``
   If ``PR_TRUE``, the job is joinable. If ``PR_FALSE``, the job is not
   joinable. See :ref:`PR_JoinJob`.


Returns
~~~~~~~

Pointer to a :ref:`PRJob` structure or ``NULL`` on error.
