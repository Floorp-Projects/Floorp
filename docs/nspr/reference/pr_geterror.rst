PR_GetError
===========

Returns the current thread's last set platform-independent error code.


Syntax
------

.. code::

   #include <prerror.h>

   PRErrorCode PR_GetError(void)


Returns
~~~~~~~

The value returned is a 32-bit number. NSPR provides no direct
interpretation of the number's value. NSPR does use :ref:`PR_SetError` to
set error numbers defined in `Error
Codes <NSPR_Error_Handling#Error_Code>`__.
