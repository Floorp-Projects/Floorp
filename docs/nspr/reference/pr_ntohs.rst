PR_ntohs
========

Performs 16-bit conversion from network byte order to host byte order.


Syntax
------

.. code::

   #include <prnetdb.h>

   PRUint16 PR_ntohs(PRUint16 conversion);


Parameter
~~~~~~~~~

The function has the following parameter:

``conversion``
   The 16-bit unsigned integer, in network byte order, to be converted.


Returns
~~~~~~~

The value of the ``conversion`` parameter in host byte order.
