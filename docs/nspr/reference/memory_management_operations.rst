This chapter describes the global functions and macros you use to
perform memory management. NSPR provides heap-based memory management
functions that map to the familiar ``malloc()``, ``calloc()``,
``realloc()``, and ``free()``.

-  `Memory Allocation Functions <#Memory_Allocation_Functions>`__
-  `Memory Allocation Macros <#Memory_Allocation_Macros>`__

.. _Memory_Allocation_Functions:

Memory Allocation Functions
---------------------------

NSPR has its own heap, and these functions act on that heap. Libraries
built on top of NSPR, such as the Netscape security libraries, use these
functions to allocate and free memory. If you are allocating memory for
use by such libraries or freeing memory that was allocated by such
libraries, you must use these NSPR functions rather than the libc
equivalents.

Memory allocation functions are:

 - :ref:`PR_Malloc`
 - :ref:`PR_Calloc`
 - :ref:`PR_Realloc`
 - :ref:`PR_Free`

``PR_Malloc()``, ``PR_Calloc()``, ``PR_Realloc()``, and ``PR_Free()``
have the same signatures as their libc equivalents ``malloc()``,
``calloc()``, ``realloc()``, and ``free()``, and have the same
semantics. (Note that the argument type ``size_t`` is replaced by
:ref:`PRUint32`.) Memory allocated by ``PR_Malloc()``, ``PR_Calloc()``, or
``PR_Realloc()`` must be freed by ``PR_Free()``.

.. _Memory_Allocation_Macros:

Memory Allocation Macros
------------------------

Macro versions of the memory allocation functions are available, as well
as additional macros that provide programming convenience:

 - :ref:`PR_MALLOC`
 - :ref:`PR_NEW`
 - :ref:`PR_REALLOC`
 - :ref:`PR_CALLOC`
 - :ref:`PR_NEWZAP`
 - :ref:`PR_DELETE`
 - :ref:`PR_FREEIF`
