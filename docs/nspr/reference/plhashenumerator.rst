
Syntax
------

.. code::

   #include <plhash.h>

   typedef PRIntn (PR_CALLBACK *PLHashEnumerator)(PLHashEntry *he, PRIntn index, void *arg);

   /* Return value */
   #define HT_ENUMERATE_NEXT     0   /* continue enumerating entries */
   #define HT_ENUMERATE_STOP     1   /* stop enumerating entries */
   #define HT_ENUMERATE_REMOVE   2   /* remove and free the current entry */
   #define HT_ENUMERATE_UNHASH   4   /* just unhash the current entry */


Description
-----------

``PLHashEnumerator`` is a function type used in the enumerating a hash
table. When all the table entries are enumerated, each entry is passed
to a user-specified function of type ``PLHashEnumerator`` with the hash
table entry, an integer index, and an arbitrary piece of user data as
argument.


Remark
------

The meaning of ``HT_ENUMERATE_UNHASH`` is not clear. In the current
implementation, it will leave the hash table in an inconsistent state.
The entries are unlinked from the table, they are not freed, but the
entry count (the ``nentries`` field of the ``PLHashTable`` structure) is
not decremented.


See Also
--------

:ref:`PL_HashTableEnumerateEntries`
