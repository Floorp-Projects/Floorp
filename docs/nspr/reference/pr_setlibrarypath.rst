PR_SetLibraryPath
=================

Registers a default library pathname with a runtime.


Syntax
------

.. code::

   #include <prlink.h>

   PRStatus PR_SetLibraryPath(const char *path);


Parameters
~~~~~~~~~~

The function has this parameter:

``path``
   A pointer to a character array that contains the directory path that
   the application should use as a default. The syntax of the pathname
   is not defined, nor whether that pathname should be absolute or
   relative.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. This may indicate that the function
   cannot allocate sufficient storage to make a copy of the path string


Description
-----------

This function registers a default library pathname with the runtime.
This allows an environment to express policy decisions globally and
lazily, rather than hardcoding and distributing the decisions throughout
the code.
