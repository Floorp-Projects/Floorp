PR_QueueJob_Write
=================

Causes a job to be queued when a socket becomes writable.


Syntax
------

.. code::

   #include <prtpool.h>

   NSPR_API(PRJob *)
   PR_QueueJob_Write(
     PRThreadPool *tpool,
     PRJobIoDesc *iod,
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
``iod``
   A pointer to a :ref:`PRJobIoDesc` structure.
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
