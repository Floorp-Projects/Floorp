PR_NewProcessAttr
=================

Creates a process attributes structure.


Syntax
------

.. code::

   #include <prlink.h>

   PRProcessAttr *PR_NewProcessAttr(void);


Parameters
~~~~~~~~~~

The function has no parameters.


Returns
~~~~~~~

A pointer to the new process attributes structure.


Description
-----------

This function creates a new :ref:`PRProcessAttr`\ structure that specifies
the attributes of a new process, then returns a pointer to the
structure. The new :ref:`PRProcessAttr`\ structure is initialized with
these default attributes:

-  The standard I/O streams (standard input, standard output, and
   standard error) are not redirected.
-  No file descriptors are inherited by the new process.
