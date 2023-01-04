
Syntax
------

.. code::

   #include <plhash.h>

   typedef struct PLHashEntry PLHashEntry;


Description
-----------

``PLHashEntry`` is a structure that represents an entry in the hash
table. An entry has a key and a value, represented by the following
fields in the ``PLHashEntry`` structure.

.. code::

   const void *key;
   void *value;

The key field is a pointer to an opaque key. The value field is a
pointer to an opaque value. If the key of an entry is an integral value
that can fit into a ``void *`` pointer, you can just cast the key itself
to ``void *`` and store it in the key field. Similarly, if the value of
an entry is an integral value that can fit into a ``void *`` pointer,
you can cast the value itself to ``void *`` and store it in the value
field.

.. warning::

   **Warning**: There are other fields in the ``PLHashEntry`` structure
   besides key and value. These fields are for use by the hash table
   library functions and the user should not tamper with them.
