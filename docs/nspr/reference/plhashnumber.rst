
Syntax
------

.. code::

   #include <plhash.h>

   typedef PRUint32 PLHashNumber;

   #define PL_HASH_BITS 32


Description
-----------

``PLHashNumber`` is an unsigned 32-bit integer. ``PLHashNumber`` is the
data type of the return value of a hash function. A hash function maps a
key to a hash number, which is then used to compute the index of the
bucket.

The macro ``PL_HASH_BITS`` is the size (in bits) of the ``PLHashNumber``
data type and has the value of 32.


See Also
--------

``PLHashFunction``
