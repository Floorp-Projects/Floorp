PRProtoEnt
==========

Protocol entry returned by :ref:`PR_GetProtoByName` and
:ref:`PR_GetProtoByNumber`.


Syntax
------

.. code::

   #include <prnetdb.h>

   typedef struct PRProtoEnt {
     char *p_name;
     char **p_aliases;
   #if defined(_WIN32)
     PRInt16 p_num;
   #else
     PRInt32 p_num;
   #endif
   } PRProtoEnt;


Fields
~~~~~~

The structure has the following fields:

``p_name``
   Pointer to official protocol name.
``p_aliases``
   Pointer to a pointer to a list of aliases. The list is terminated
   with a ``NULL`` entry.
``p_num``
   Protocol number.
