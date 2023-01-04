PR_Access
=========

Determines the accessibility of a file.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_Access(
     const char *name,
     PRAccessHow how);


Parameters
~~~~~~~~~~

The function has the following parameters:

``name``
   The pathname of the file whose accessibility is to be determined.
``how``
   Specifies which access permission to check for. Use one of the
   following values:

    - :ref:`PR_ACCESS_READ_OK`. Test for read permission.
    - :ref:`PR_ACCESS_WRITE_OK`. Test for write permission.
    - :ref:`PR_ACCESS_EXISTS`. Check existence of file.


Returns
~~~~~~~

One of the following values:

-  If the requested access is permitted, ``PR_SUCCESS``.
-  If the requested access is not permitted, ``PR_FAILURE``.
