PR_htons
========

Performs 16-bit conversion from host byte order to network byte order.


Syntax
------

.. code::

   #include <prnetdb.h>

   PRUint16 PR_htons(PRUint16 conversion);


Parameter
~~~~~~~~~

The function has the following parameter:

``conversion``
   The 16-bit unsigned integer, in host byte order, to be converted.


Returns
~~~~~~~

The value of the ``conversion`` parameter in network byte order.
