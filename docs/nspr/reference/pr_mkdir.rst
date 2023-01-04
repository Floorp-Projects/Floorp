PR_MkDir
========

Creates a directory with a specified name and access mode.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_MkDir(
     const char *name,
     PRIntn mode);


Parameters
~~~~~~~~~~

The function has the following parameters:

``name``
   The name of the directory to be created. All the path components up
   to but not including the leaf component must already exist.
``mode``
   The access permission bits of the file mode of the new directory if
   the file is created when ``PR_CREATE_FILE`` is on.

Caveat: The mode parameter is currently applicable only on Unix
platforms. It may be applicable to other platforms in the future.

Possible values include the following:

 - :ref:`00400`. Read by owner.
 - :ref:`00200`. Write by owner.
 - :ref:`00100`. Search by owner.
 - :ref:`00040`. Read by group.
 - :ref:`00020`. Write by group.
 - :ref:`00010`. Search by group.
 - :ref:`00004`. Read by others.
 - :ref:`00002`. Write by others.
 - :ref:`00001`. Search by others.


Returns
~~~~~~~

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. The actual reason can be retrieved
   via :ref:`PR_GetError`.


Description
-----------

:ref:`PR_MkDir` creates a new directory with the pathname ``name``. All the
path components up to but not including the leaf component must already
exist. For example, if the pathname of the directory to be created is
``a/b/c/d``, the directory ``a/b/c`` must already exist.


See Also
--------

:ref:`PR_RmDir`
