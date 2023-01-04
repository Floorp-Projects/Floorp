PR_FamilyInet
=============

Gets the value of the address family for Internet Protocol.


Syntax
------

.. code::

   #include <prnetdb.h>

   PRUint16 PR_FamilyInet(void);


Returns
~~~~~~~

The value of the address family for Internet Protocol. This is usually
``PR_AF_INET``, but can also be ``PR_AF_INET6`` if IPv6 is enabled. The
returned value can be assigned to the ``inet.family`` field of a
:ref:`PRNetAddr` object.
