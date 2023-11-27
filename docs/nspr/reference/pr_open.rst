PR_Open
=======

Opens a file for reading, writing, or both. Also used to create a file.


Syntax
------

.. code::

   #include <prio.h>

   PRFileDesc* PR_Open(
     const char *name,
     PRIntn flags,
     PRIntn mode);


Parameters
~~~~~~~~~~

The function has the following parameters:

``name``
   The pathname of the file to be opened.
``flags``
   The file status flags define how the file is accessed. It is a
   bitwise ``OR`` of the following bit flags. In most cases, only one of
   the first three flags may be used. If the ``flags`` parameter does
   not include any of the first three flags (``PR_RDONLY``,
   ``PR_WRONLY``, or ``PR_RDWR``), the open file can't be read or
   written, which is not useful.

.. note::

   The constants PR_RDWR and friends are not in any interface
   (`bug 433295 <https://bugzilla.mozilla.org/show_bug.cgi?id=433295>`__).
   Thus they cannot be used in JavaScript, you have to use the octal
   constants (see `File I/O Snippets </en/Code_snippets:File_I/O>`__).

+--------------------+-------+---------------------------------------+
| Name               | Value | Description                           |
+====================+=======+=======================================+
| ``PR_RDONLY``      | 0x01  | Open for reading only.                |
+--------------------+-------+---------------------------------------+
| ``PR_WRONLY``      | 0x02  | Open for writing only.                |
+--------------------+-------+---------------------------------------+
| ``PR_RDWR``        | 0x04  | Open for reading and writing.         |
+--------------------+-------+---------------------------------------+
| ``PR_CREATE_FILE`` | 0x08  | If the file does not exist, the file  |
|                    |       | is created. If the file exists, this  |
|                    |       | flag has no effect.                   |
+--------------------+-------+---------------------------------------+
| ``PR_APPEND``      | 0x10  | The file pointer is set to the end of |
|                    |       | the file prior to each write.         |
+--------------------+-------+---------------------------------------+
| ``PR_TRUNCATE``    | 0x20  | If the file exists, its length is     |
|                    |       | truncated to 0.                       |
+--------------------+-------+---------------------------------------+
| ``PR_SYNC``        | 0x40  | If set, each write will wait for both |
|                    |       | the file data and file status to be   |
|                    |       | physically updated.                   |
+--------------------+-------+---------------------------------------+
| ``PR_EXCL``        | 0x80  | With ``PR_CREATE_FILE``, if the file  |
|                    |       | does not exist, the file is created.  |
|                    |       | If the file already exists, no action |
|                    |       | and NULL is returned.                 |
+--------------------+-------+---------------------------------------+



``mode``
   When ``PR_CREATE_FILE`` flag is set and the file is created, these
   flags define the access permission bits of the newly created file.
   This feature is currently only applicable on Unix platforms. It is
   ignored by any other platform but it may apply to other platforms in
   the future. Possible values of the ``mode`` parameter are listed in
   the table below.

============ ===== =====================================
Name         Value Description
============ ===== =====================================
``PR_IRWXU`` 0700  read, write, execute/search by owner.
``PR_IRUSR`` 0400  read permission, owner.
``PR_IWUSR`` 0200  write permission, owner.
``PR_IXUSR`` 0100  execute/search permission, owner.
``PR_IRWXG`` 0070  read, write, execute/search by group
``PR_IRGRP`` 0040  read permission, group
``PR_IWGRP`` 0020  write permission, group
``PR_IXGRP`` 0010  execute/search permission, group
``PR_IRWXO`` 0007  read, write, execute/search by others
``PR_IROTH`` 0004  read permission, others
``PR_IWOTH`` 0002  write permission, others
``PR_IXOTH`` 0001  execute/search permission, others
============ ===== =====================================


Returns
~~~~~~~

The function returns one of the following values:

-  If the file is successfully opened, a pointer to a dynamically
   allocated :ref:`PRFileDesc` for the newly opened file. The
   :ref:`PRFileDesc` should be freed by calling :ref:`PR_Close`.
-  If the file was not opened successfully, a ``NULL`` pointer.


Description
-----------

:ref:`PR_Open` creates a file descriptor (:ref:`PRFileDesc`) for the file with
the pathname ``name`` and sets the file status flags of the file
descriptor according to the value of ``flags``. If a new file is created
as a result of the :ref:`PR_Open` call, its file mode bits are set
according to the ``mode`` parameter.
