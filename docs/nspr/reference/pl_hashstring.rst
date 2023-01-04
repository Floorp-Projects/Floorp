PL_HashString
=============

A general-purpose hash function for character strings.


Syntax
------

.. code::

   #include <plhash.h>

   PLHashNumber PL_HashString(const void *key);


Parameter
~~~~~~~~~

The function has the following parameter:

``key``
   A pointer to a character string.


Returns
~~~~~~~

The hash number for the specified key.


Description
-----------

:ref:`PL_HashString` can be used as the key hash function for a hash table
if the key is a character string.
