
Syntax
------

.. code::

   #include <plhash.h>

   typedef PLHashNumber (PR_CALLBACK *PLHashFunction)(const void *key);


Description
-----------

``PLHashNumber`` is a function type that maps the key of a hash table
entry to a hash number.


See Also
--------

`PL_HashString <PL_HashString>`__
