PR_UnloadLibrary
================

Unloads a library loaded with :ref:`PR_LoadLibrary`.


Syntax
------

.. code::

   #include <prlink.h>

   PRStatus PR_UnloadLibrary(PRLibrary *lib);


Parameters
~~~~~~~~~~

The function has this parameter:

``lib``
   A reference previously returned from :ref:`PR_LoadLibrary`.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. Use :ref:`PR_GetError` to find the
   reason for the failure.


Description
-----------

This function undoes the effect of a :ref:`PR_LoadLibrary`. After calling
this function, future references to the library using its identity as
returned by :ref:`PR_LoadLibrary` will be invalid.
