PR_JoinJob
==========

Blocks the current thread until a job has completed.


Syntax
------

.. code::

   #include <prtpool.h>

   NSPR_API(PRStatus) PR_JoinJob(PRJob *job);


Parameter
~~~~~~~~~

The function has the following parameter:

``job``
   A pointer to a :ref:`PRJob` structure returned by a :ref:`PR_QueueJob`
   function representing the job to be cancelled.


Returns
~~~~~~~

:ref:`PRStatus`
