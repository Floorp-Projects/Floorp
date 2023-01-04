PR_ConnectContinue
==================


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_ConnectContinue(
     PRFileDesc *fd,
     PRInt16 out_flags);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object representing a socket.

``out_flags``
   The out_flags field of the poll descriptor returned by
   `PR_Poll() <PR_Poll>`__.


Returns
~~~~~~~

-  If the nonblocking connect has successfully completed,
   PR_ConnectContinue returns PR_SUCCESS.
-  If PR_ConnectContinue() returns PR_FAILURE, call PR_GetError():
- PR_IN_PROGRESS_ERROR: the nonblocking connect is still in
   progress and has not completed yet. The caller should poll the file
   descriptor for the in_flags PR_POLL_WRITE|PR_POLL_EXCEPT and retry
   PR_ConnectContinue later when PR_Poll() returns.
- Other errors: the nonblocking connect has failed with this
   error code.


Description
-----------

Continue a nonblocking connect. After a nonblocking connect is initiated
with PR_Connect() (which fails with PR_IN_PROGRESS_ERROR), one should
call PR_Poll() on the socket, with the in_flags PR_POLL_WRITE \|
PR_POLL_EXCEPT. When PR_Poll() returns, one calls PR_ConnectContinue()
on the socket to determine whether the nonblocking connect has completed
or is still in progress. Repeat the PR_Poll(), PR_ConnectContinue()
sequence until the nonblocking connect has completed.
