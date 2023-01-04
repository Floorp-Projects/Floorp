PR_ExportFileMapAsString
========================

Creates a string identifying a :ref:`PRFileMap`.


Syntax
------

.. code::

   #include <prshma.h>

   NSPR_API( PRStatus )
   PR_ExportFileMapAsString(
     PRFileMap *fm,
     PRSize bufsize,
     char *buf
   );

#. define PR_FILEMAP_STRING_BUFSIZE 128


Parameters
~~~~~~~~~~

The function has the following parameters:

``fm``
   A pointer to the :ref:`PRFileMap` to be represented as a string.
``bufsize``
   sizeof(buf)
``buf``
   A pointer to abuffer of length ``PR_FILEMAP_STRING_BUFSIZE``.


Returns
~~~~~~~

:ref:`PRStatus`


Description
-----------

Creates an identifier, as a string, from a :ref:`PRFileMap` object
previously created with :ref:`PR_OpenAnonFileMap`.
