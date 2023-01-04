PR_FreeLibraryName
==================

Frees memory allocated by NSPR for library names and path names.


Syntax
------

.. code::

   #include <prlink.h>

   void PR_FreeLibraryName(char *mem);


Parameters
~~~~~~~~~~

The function has this parameter:

``mem``
   A reference to a character array that was previously allocated by the
   dynamic library runtime.


Returns
~~~~~~~

Nothing.


Description
-----------

This function deletes the storage allocated by the runtime in the
functions described previously. It is important to use this function to
rather than calling directly into ``malloc`` in order to isolate the
runtime's semantics regarding storage management.
