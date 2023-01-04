PR_FindSymbolAndLibrary
=======================

Finds a symbol in one of the currently loaded libraries, and returns
both the symbol and the library in which it was found.


Syntax
------

.. code::

   #include <prlink.h>

   void* PR_FindSymbolAndLibrary (
      const char *name,
      PRLibrary **lib);


Parameters
~~~~~~~~~~

The function has these parameters:

``name``
   The textual representation of the symbol to locate.
``lib``
   A reference to a location at which the runtime will store the library
   in which the symbol was discovered. This location must be
   pre-allocated by the caller.


Returns
~~~~~~~

If successful, returns a non-``NULL`` pointer to the found symbol, and
stores a pointer to the library in which it was found at the location
pointed to by lib.

If the symbol could not be found, returns ``NULL``.


Description
-----------

This function finds the specified symbol in one of the currently loaded
libraries. It returns the address of the symbol. Upon return, the
location pointed to by the parameter lib contains a pointer to the
library that contains that symbol. The location must be pre-allocated by
the caller.

The function returns ``NULL`` if no such function can be found. The
order in which the known libraries are searched in not specified. This
function is equivalent to calling first :ref:`PR_LoadLibrary`, then
:ref:`PR_FindSymbol`.

The identity returned from this function must be the target of a
:ref:`PR_UnloadLibrary` in order to return the runtime to its original
state.
