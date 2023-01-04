PRFileType
==========


Type for enumerators used in the type field of the :ref:`PRFileInfo` and
:ref:`PRFileInfo64` structures.


Syntax
------

.. code::

   #include <prio.h>

   typedef enum PRFileType{
      PR_FILE_FILE = 1,
      PR_FILE_DIRECTORY = 2,
      PR_FILE_OTHER = 3
   } PRFileType;


Enumerators
~~~~~~~~~~~

The enumeration has the following enumerators:

``PR_FILE_FILE``
   The information in the structure describes a file.
``PR_FILE_DIRECTORY``
   The information in the structure describes a directory.
``PR_FILE_OTHER``
   The information in the structure describes some other kind of file
   system object.
