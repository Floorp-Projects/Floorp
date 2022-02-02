PR_htons
========

Performs 16-bit conversion from host byte order to network byte order.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prnetdb.h>

   PRUint16 PR_htons(PRUint16 conversion);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``conversion``
   The 16-bit unsigned integer, in host byte order, to be converted.

.. _Returns:

Returns
~~~~~~~

The value of the ``conversion`` parameter in network byte order.
