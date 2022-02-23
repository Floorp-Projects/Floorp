PR_SetPollableEvent
===================

Set a pollable event.


Syntax
------

.. code:: eval

   NSPR_API(PRStatus) PR_SetPollableEvent(PRFileDesc *event);


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
