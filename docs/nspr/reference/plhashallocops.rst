
Syntax
------

.. code::

   #include <plhash.h>

   typedef struct PLHashAllocOps {
     void *(PR_CALLBACK *allocTable)(void *pool, PRSize size);
     void (PR_CALLBACK *freeTable)(void *pool, void *item);
     PLHashEntry *(PR_CALLBACK *allocEntry)(void *pool, const void *key);
     void (PR_CALLBACK *freeEntry)(void *pool, PLHashEntry *he, PRUintn flag);
   } PLHashAllocOps;

   #define HT_FREE_VALUE 0 /* just free the entry's value */
   #define HT_FREE_ENTRY 1 /* free value and entire entry */


Description
-----------

Users of the hash table functions can provide their own memory
allocation functions. A pair of functions is used to allocate and tree
the table, and another pair of functions is used to allocate and free
the table entries.

The first argument, pool, for all four functions is a void \* pointer
that is a piece of data for the memory allocator. Typically pool points
to a memory pool used by the memory allocator.

The ``freeEntry`` function does not need to free the value of the entry.
If flag is ``HT_FREE_ENTRY``, the function frees the entry.


Remark
------

The ``key`` argument for the ``allocEntry`` function does not seem to be
useful. It is unused in the default ``allocEntry`` function.
