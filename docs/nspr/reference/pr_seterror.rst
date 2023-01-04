PR_SetError
===========

Sets error information within a thread context.


Syntax
------

.. code::

   #include <prerror.h>

   void PR_SetError(PRErrorCode errorCode, PRInt32 oserr)


Parameters
~~~~~~~~~~

The function has these parameters:

``errorCode``
   The NSPR (platform-independent) translation of the error.

``oserr``
   The platform-specific error. If there is no appropriate OS error
   number, a zero may be supplied.


Description
-----------

NSPR does not validate the value of the error number or OS error number
being specified. The runtime merely stores the value and returns it when
requested.
