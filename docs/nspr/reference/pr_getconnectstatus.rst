PR_GetConnectStatus
===================

Get the completion status of a nonblocking connection.


Syntax
------

.. code::

   PRStatus PR_GetConnectStatus(const PRPollDesc *pd);


Parameter
~~~~~~~~~

The function has the following parameter:

``pd``
   A pointer to a ``PRPollDesc`` satructure whose ``fd`` field is the
   socket and whose ``in_flags`` field must contain ``PR_POLL_WRITE``
   and ``PR_POLL_EXCEPT``.


Returns
~~~~~~~

The function returns one of these values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. The reason for the failure can be
   retrieved via :ref:`PR_GetError`.

If :ref:`PR_GetError` returns ``PR_IN_PROGRESS_ERROR``, the nonblocking
connection is still in progress and has not completed yet.Other errors
indicate that the connection has failed.


Description
-----------

After :ref:`PR_Connect` on a nonblocking socket fails with
``PR_IN_PROGRESS_ERROR``, you may wait for the connection to complete by
calling :ref:`PR_Poll` on the socket with the ``in_flags``
``PR_POLL_WRITE`` \| ``PR_POLL_EXCEPT``. When :ref:`PR_Poll` returns, call
:ref:`PR_GetConnectStatus` on the socket to determine whether the
nonblocking connect has succeeded or failed.
