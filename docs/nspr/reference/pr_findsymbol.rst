PR_FindSymbol
=============

``PR_FindSymbol()`` will return an untyped reference to a symbol in a
particular library given the identity of the library and a textual
representation of the symbol in question.


Syntax
------

.. code::

   #include <prlink.h>

   void* PR_FindSymbol (
      PRLibrary *lib,
      const char *name);


Parameters
~~~~~~~~~~

The function has these parameters:

``lib``
   A valid reference to a loaded library, as returned by
   :ref:`PR_LoadLibrary`, or ``NULL``.
``name``
   A textual representation of the symbol to resolve.


Returns
~~~~~~~

An untyped pointer.


Description
-----------

This function finds and returns an untyped reference to the specified
symbol in the specified library. If the lib parameter is ``NULL``, all
libraries known to the runtime and the main program are searched in an
unspecified order.

Use this function to look up functions or data symbols in a shared
library. Getting a pointer to a symbol in a library does indicate that
the library is available when the search was made. The runtime does
nothing to ensure the continued validity of the symbol. If the library
is unloaded, for instance, the results of any :ref:`PR_FindSymbol` calls
become invalid as well.
