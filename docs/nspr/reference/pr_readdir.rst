PR_ReadDir
==========

Gets a pointer to the next entry in the directory.


Syntax
------

.. code::

   #include <prio.h>

   PRDirEntry* PR_ReadDir(
     PRDir *dir,
     PRDirFlags flags);


Parameters
~~~~~~~~~~

The function has the following parameters:

``dir``
   A pointer to a :ref:`PRDir` object that designates an open directory.
``flags``
   Specifies which directory entries, if any, to skip. Values can
   include the following:

    - :ref:`PR_SKIP_NONE`. Do not skip any files.
    - :ref:`PR_SKIP_DOT`. Skip the directory entry "." representing the
      current directory.
    - :ref:`PR_SKIP_DOT_DOT`. Skip the directory entry ".." representing
      the parent directory.
    - :ref:`PR_SKIP_BOTH`. Skip both "." and ".."
    - :ref:`PR_SKIP_HIDDEN`. Skip hidden files. On Windows platforms and
      the Mac OS, this value identifies files with the "hidden"
      attribute set. On Unix platform, this value identifies files whose
      names begin with a period (".").


Returns
~~~~~~~

-  A pointer to the next entry in the directory.
-  If the end of the directory is reached or an error occurs, ``NULL``.
   The reason can be retrieved via :ref:`PR_GetError`.


Description
-----------

:ref:`PR_ReadDir` returns a pointer to a directory entry structure:

.. code::

   struct PRDirEntry {
     const char *name;
   };

   typedef struct PRDirEntry PRDirEntry;

The structure has the following field:

``name``
   Name of entry, relative to directory name.

The ``flags`` parameter is an enum of type ``PRDirFlags``:

.. code::

   typedef enum PRDirFlags {
     PR_SKIP_NONE    = 0x0,
     PR_SKIP_DOT     = 0x1,
     PR_SKIP_DOT_DOT = 0x2,
     PR_SKIP_BOTH    = 0x3,
     PR_SKIP_HIDDEN  = 0x4
   } PRDirFlags;

The memory associated with the returned PRDirEntry structure is managed
by NSPR. The caller must not free the ``PRDirEntry`` structure.
Moreover, the ``PRDirEntry`` structure returned by each :ref:`PR_ReadDir`
call is valid only until the next :ref:`PR_ReadDir` or :ref:`PR_CloseDir` call
on the same :ref:`PRDir` object.

If the end of the directory is reached, :ref:`PR_ReadDir` returns ``NULL``,
and :ref:`PR_GetError` returns ``PR_NO_MORE_FILES_ERROR``.


See Also
--------

:ref:`PR_OpenDir`
