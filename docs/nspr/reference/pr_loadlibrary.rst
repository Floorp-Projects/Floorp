PR_LoadLibrary
==============

Loads a referenced library.


Syntax
------

.. code::

   #include <prlink.h>

   PRLibrary* PR_LoadLibrary(const char *name);


Parameters
~~~~~~~~~~

The function has this parameter:

``name``
   A platform-dependent character array that names the library to be
   loaded, as returned by :ref:`PR_GetLibraryName`.


Returns
~~~~~~~

If successful, returns a reference to an opaque :ref:`PRLibrary` object.

If the operation fails, returns ``NULL``. Use :ref:`PR_GetError` to find
the reason for the failure.


Description
-----------

This function loads and returns a reference to the specified library.
The returned reference becomes the library's identity. The function
suppresses duplicate loading if the library is already known by the
runtime.

Each call to :ref:`PR_LoadLibrary` must be paired with a corresponding call
to :ref:`PR_UnloadLibrary` in order to return the runtime to its original
state.
