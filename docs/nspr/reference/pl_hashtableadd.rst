PL_HashTableAdd
===============

Add a new entry with the specified key and value to the hash table.


Syntax
------

.. code::

   #include <plhash.h>

   PLHashEntry *PL_HashTableAdd(
     PLHashTable *ht,
     const void *key,
     void *value);


Parameters
~~~~~~~~~~

The function has the following parameters:

``ht``
   A pointer to the the hash table to which to add the entry.
``key``
   A pointer to the key for the entry to be added.
``value``
   A pointer to the value for the entry to be added.


Returns
~~~~~~~

A pointer to the new entry.


Description
-----------

Add a new entry with the specified key and value to the hash table.

If an entry with the same key already exists in the table, the
``freeEntry`` function is invoked with the ``HT_FREE_VALUE`` flag. You
can write your ``freeEntry`` function to free the value of the specified
entry if the old value should be freed. The default ``freeEntry``
function does not free the value of the entry.

:ref:`PL_HashTableAdd` returns ``NULL`` if there is not enough memory to
create a new entry. It doubles the number of buckets if the table is
overloaded.
