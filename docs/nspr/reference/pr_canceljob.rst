PR_CancelJob
============

Causes a previously queued job to be canceled.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prtpool.h>

   NSPR_API(PRStatus) PR_CancelJob(PRJob *job);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``job``
   A pointer to a :ref:`PRJob` structure returned by a :ref:`PR_QueueJob`
   function representing the job to be cancelled.

.. _Returns:

Returns
~~~~~~~

:ref:`PRStatus`
