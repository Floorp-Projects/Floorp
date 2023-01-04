PR_WaitForPollableEvent
=======================

Blocks the calling thread until the pollable event is set, and then
atomically unsetting the event before returning.


Syntax
------

.. code::

   NSPR_API(PRStatus) PR_WaitForPollableEvent(PRFileDesc *event);


Parameter
~~~~~~~~~

The function has the following parameter:

``event``
   Pointer to a :ref:`PRFileDesc` structure previously created via a call
   to :ref:`PR_NewPollableEvent`.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. The reason for the failure can be
   retrieved via :ref:`PR_GetError`.
