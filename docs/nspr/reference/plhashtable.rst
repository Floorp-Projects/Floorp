
Syntax
------

.. code::

   #include <plhash.h>

   typedef struct PLHashTable PLHashTable;


Description
-----------

The opaque ``PLHashTable`` structure represents a hash table. Entries in
the table have the type ``PLHashEntry`` and are organized into buckets.
The number of buckets in a hash table may be changed by the library
functions during the lifetime of the table to optimize speed and space.

A new hash table is created by the :ref:`PL_NewHashTable` function, and
destroyed by the :ref:`PL_HashTableDestroy` function.
