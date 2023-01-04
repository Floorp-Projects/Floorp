PL_HashTableEnumerateEntries
============================

Enumerates all the entries in the hash table, invoking a specified
function on each entry.


Syntax
------

.. code::

   #include <plhash.h>

   PRIntn PL_HashTableEnumerateEntries(
     PLHashTable *ht,
     PLHashEnumerator f,
     void *arg);


Parameters
~~~~~~~~~~

The function has the following parameters:

``ht``
   A pointer to the hash table whose entries are to be enumerated.
``f``
   Function to be applied to each entry.
``arg``
   Argument for function ``f``.


Returns
~~~~~~~

The number of entries enumerated.


Description
-----------

The entries are enumerated in an unspecified order. For each entry, the
enumerator function is invoked with the entry, the index (in the
sequence of enumeration, starting from 0) of the entry, and arg as
arguments.
