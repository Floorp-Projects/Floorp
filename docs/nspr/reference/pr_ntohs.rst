PR_ntohs
========

Performs 16-bit conversion from network byte order to host byte order.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prnetdb.h>

   PRUint16 PR_ntohs(PRUint16 conversion);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``conversion``
   The 16-bit unsigned integer, in network byte order, to be converted.

.. _Returns:

Returns
~~~~~~~

The value of the ``conversion`` parameter in host byte order.
