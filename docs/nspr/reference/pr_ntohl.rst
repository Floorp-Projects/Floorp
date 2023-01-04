PR_ntohl
========

Performs 32-bit conversion from network byte order to host byte order.


Syntax
------

.. code::

   #include <prnetdb.h>

   PRUint32 PR_ntohl(PRUint32 conversion);


Parameter
~~~~~~~~~

The function has the following parameter:

``conversion``
   The 32-bit unsigned integer, in network byte order, to be converted.


Returns
~~~~~~~

The value of the ``conversion`` parameter in host byte order.
