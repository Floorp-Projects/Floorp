PR_CloseDir
===========

Closes the specified directory.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_CloseDir(PRDir *dir);


Parameter
~~~~~~~~~

The function has the following parameter:

``dir``
   A pointer to a :ref:`PRDir` structure representing the directory to be
   closed.


Returns
~~~~~~~

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. The reason for the failure can be
   retrieved via :ref:`PR_GetError`.


Description
-----------

When a :ref:`PRDir` object is no longer needed, it must be closed and freed
with a call to :ref:`PR_CloseDir` call. Note that after a :ref:`PR_CloseDir`
call, any ``PRDirEntry`` object returned by a previous :ref:`PR_ReadDir`
call on the same :ref:`PRDir` object becomes invalid.


See Also
--------

:ref:`PR_OpenDir`
