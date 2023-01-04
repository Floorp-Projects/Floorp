PRErrorCode
===========


Type for error codes that can be retrieved with :ref:`PR_GetError`. You can
also set your own errors using :ref:`PR_SetError`.


Syntax
------

.. code::

   #include <prerror.h>

   typedef PRInt32 PRErrorCode


Description
-----------

The service NSPR offers in this area is the ability to associate a
thread-specific condition with an error number. The error number
namespace is not well managed. NSPR assumes error numbers starting at
-6000 (decimal) and progressing towards zero. At present less than 100
error codes have been defined. If NSPR's error handling is adopted by
calling clients, then some sort of partitioning of the namespace will
have to be employed. NSPR does not attempt to address this issue.

For NSPR errors, see `Error Codes <NSPR_Error_Handling#Error_Code>`__.
