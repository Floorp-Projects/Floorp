PR_htonl
========

Performs 32-bit conversion from host byte order to network byte order.


Syntax
------

.. code::

   #include <prnetdb.h>

   PRUint32 PR_htonl(PRUint32 conversion);


Parameter
~~~~~~~~~

The function has the following parameter:

``conversion``
   The 32-bit unsigned integer, in host byte order, to be converted.


Returns
~~~~~~~

The value of the ``conversion`` parameter in network byte order.
