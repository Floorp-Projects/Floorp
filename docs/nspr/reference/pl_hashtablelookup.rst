PL_HashTableLookup
==================

Looks up the entry with the specified key and return its value.


Syntax
------

.. code::

   #include <plhash.h>

   void *PL_HashTableLookup(
     PLHashTable *ht,
     const void *key);


Parameters
~~~~~~~~~~

The function has the following parameters:

``ht``
   A pointer to the hash table in which to look up the entry specified
   by ``key``.
``key``
   A pointer to the key for the entry to look up.


Returns
~~~~~~~

The value of the entry with the specified key, or ``NULL`` if there is
no such entry.


Description
-----------

If there is no entry with the specified key, :ref:`PL_HashTableLookup`
returns ``NULL``. This means that one cannot tell whether a ``NULL``
return value means the entry does not exist or the value of the entry is
``NULL``. Keep this ambiguity in mind if you want to store ``NULL``
values in a hash table.
