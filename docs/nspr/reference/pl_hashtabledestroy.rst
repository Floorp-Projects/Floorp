PL_HashTableDestroy
===================

Frees the table and all the entries.


Syntax
------

.. code::

   #include <plhash.h>

   void PL_HashTableDestroy(PLHashTable *ht);


Parameter
~~~~~~~~~

The function has the following parameter:

``ht``
   A pointer to the hash table to be destroyed.


Description
-----------

:ref:`PL_HashTableDestroy` frees all the entries in the table and the table
itself. The entries are freed by the ``freeEntry`` function (with the
``HT_FREE_ENTRY`` flag) in the ``allocOps`` structure supplied when the
table was created.
