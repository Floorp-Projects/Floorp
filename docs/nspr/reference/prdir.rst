PRDir
=====

Directory structure used with `Directory I/O
Functions <I_O_Functions#Directory_I.2FO_Functions>`__.


Syntax
------

.. code::

   #include <prio.h>

   typedef struct PRDir PRDir;


Description
-----------

The opaque structure :ref:`PRDir` represents an open directory in the file
system. The function :ref:`PR_OpenDir` opens a specified directory and
returns a pointer to a :ref:`PRDir` structure, which can be passed to
:ref:`PR_ReadDir` repeatedly to obtain successive entries (files or
subdirectories in the open directory). To close the directory, pass the
:ref:`PRDir` pointer to :ref:`PR_CloseDir`.
