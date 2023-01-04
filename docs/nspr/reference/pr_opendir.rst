PR_OpenDir
==========

Opens the directory with the specified pathname.


Syntax
------

.. code::

   #include <prio.h>

   PRDir* PR_OpenDir(const char *name);


Parameter
~~~~~~~~~

The function has the following parameter:

``name``
   The pathname of the directory to be opened.


Returns
~~~~~~~

-  If the directory is successfully opened, a :ref:`PRDir` object is
   dynamically allocated and the function returns a pointer to it.
-  If the directory cannot be opened, the function returns ``NULL``.


Description
-----------

:ref:`PR_OpenDir` opens the directory specified by the pathname ``name``
and returns a pointer to a directory stream (a :ref:`PRDir` object) that
can be passed to subsequent :ref:`PR_ReadDir` calls to get the directory
entries (files and subdirectories) in the directory. The :ref:`PRDir`
pointer should eventually be closed by a call to :ref:`PR_CloseDir`.
