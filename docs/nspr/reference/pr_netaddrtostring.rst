PR_NetAddrToString
==================

Converts a character string to a network address.


Syntax
------

.. code::

   #include <prnetdb.h>

   PRStatus PR_NetAddrToString(
     const PRNetAddr *addr,
     char *string,
     PRUint32 size);


Parameters
~~~~~~~~~~

The function has the following parameters:

``addr``
   A pointer to the network address to be converted.
``string``
   A buffer that will hold the converted string on output.
``size``
   The size of the result buffer (``string``).


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. You can retrieve the reason for the
   failure by calling :ref:`PR_GetError`.


Description
-----------

The network address to be converted (``addr``) may be either an IPv4 or
IPv6 address structure, assuming that the NSPR library and the host
system are both configured to utilize IPv6 addressing. If ``addr`` is an
IPv4 address, ``size`` needs to be at least 16. If ``addr`` is an IPv6
address, ``size`` needs to be at least 46.
