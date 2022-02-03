This chapter describes the hash table functions in the plds (portable
library — data structures) library of NSPR. The hash table library
functions are declared in the header file ``plhash.h.``

.. warning::

   **Warning**: The NSPR hash table library functions are not thread
   safe.

A hash table lookup may change the internal organization of the hash
table (to speed up future lookups).

-  `Hash Table Types and Constants <#Hash_Table_Types_and_Constants>`__
-  `Hash Table Functions <#Hash_Table_Functions>`__

.. _Hash_Table_Types_and_Constants:

Hash Table Types and Constants
------------------------------

 - :ref:`PLHashEntry`
 - :ref:`PLHashTable`
 - :ref:`PLHashNumber`
 - :ref:`PLHashFunction`
 - :ref:`PLHashComparator`
 - :ref:`PLHashEnumerator`
 - :ref:`PLHashAllocOps`

.. _Hash_Table_Functions:

Hash Table Functions
--------------------

 - :ref:`PL_NewHashTable`
 - :ref:`PL_HashTableDestroy`
 - :ref:`PL_HashTableAdd`
 - :ref:`PL_HashTableRemove`
 - :ref:`PL_HashTableLookup`
 - :ref:`PL_HashTableEnumerateEntries`
 - :ref:`PL_HashString`
 - :ref:`PL_CompareStrings`
 - :ref:`PL_CompareValues`
