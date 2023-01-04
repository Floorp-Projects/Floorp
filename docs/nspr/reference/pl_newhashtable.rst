PL_NewHashTable
===============

Create a new hash table.


Syntax
------

.. code::

   #include <plhash.h>

   PLHashTable *PL_NewHashTable(
     PRUint32 numBuckets,
     PLHashFunction keyHash,
     PLHashComparator keyCompare,
     PLHashComparator valueCompare,
     const PLHashAllocOps *allocOps,
     void *allocPriv
   );


Parameters
~~~~~~~~~~

The function has the following parameters:

``numBuckets``
   The number of buckets in the hash table.
``keyHash``
   Hash function.
``keyCompare``
   Function used to compare keys of entries.
``valueCompare``
   Function used to compare keys of entries.
``allocOps``
   A pointer to a ``PLHashAllocOps`` structure that must exist
   throughout the lifetime of the new hash table.
``allocPriv``
   Passed as the first argument (pool).


Returns
~~~~~~~

The new hash table.


Description
-----------

:ref:`PL_NewHashTable` creates a new hash table. The table has at least 16
buckets. You can pass a value of 0 as ``numBuckets`` to create the
default number of buckets in the new table. The arguments ``keyCompare``
and ``valueCompare`` are functions of type :ref:`PLHashComparator` that the
hash table library functions use to compare the keys and the values of
entries.

The argument ``allocOps`` points to a ``PLHashAllocOps`` structure that
must exist throughout the lifetime of the new hash table. The hash table
library functions do not make a copy of this structure. When the
allocation functions in ``allocOps`` are invoked, the allocation private
data allocPriv is passed as the first argument (pool). You can specify a
``NULL`` value for ``allocOps`` to use the default allocation functions.
If ``allocOps`` is ``NULL``, ``allocPriv`` is ignored. Note that the
default ``freeEntry`` function does not free the value of the entry.
