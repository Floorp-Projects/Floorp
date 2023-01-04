PR_Sync
=======

Synchronizes any buffered data for a file descriptor to its backing
device (disk).


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_Sync(PRFileDesc *fd);


Parameter
~~~~~~~~~

The function has the following parameter:

``fd``
   Pointer to a :ref:`PRFileDesc` object representing a file.


Returns
~~~~~~~

The function returns one of the following values:

-  On successful completion, ``PR_SUCCESS``.
-  If the function fails, ``PR_FAILURE``.


Description
-----------

:ref:`PR_Sync` writes all the in-memory buffered data of the specified file
to the disk.
