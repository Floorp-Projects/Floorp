PR_EnumerateAddrInfo
====================


Enumerates each of the possible network addresses of a ``PRAddrInfo``
structure, acquired from :ref:`PR_GetAddrInfoByName`.


Syntax
~~~~~~

.. code::

   #include <prnetdb.h>

   void *PR_EnumerateAddrInfo(
     void *enumPtr,
     const PRAddrInfo *addrInfo,
     PRUint16 port,
     PRNetAddr *result);


Parameters
~~~~~~~~~~

The function has the following parameters:

``enumPtr``
   The index pointer of the enumeration. To begin an enumeration, this
   argument is set to ``NULL``. To continue an enumeration (thereby
   getting successive addresses from the ``PRAddrInfo`` structure), the
   value should be set to the function's last returned value. The
   enumeration is complete when a value of ``NULL`` is returned.
``addrInfo``
   A pointer to a ``PRAddrInfo`` structure returned by
   :ref:`PR_GetAddrInfoByName`.
``port``
   The port number to be assigned as part of the :ref:`PRNetAddr`
   structure. This parameter is not checked for validity.
``result``
   On input, a pointer to a :ref:`PRNetAddr` structure. On output, this
   structure is filled in by the runtime if the result of the call is
   not ``NULL``.


Returns
~~~~~~~

The function returns the value you should specify in the ``enumPtr``
parameter for the next call of the enumerator. If the function returns
``NULL``, the enumeration is ended.


Description
-----------

:ref:`PR_EnumerateAddrInfo` is a stateless enumerator. The principle input,
the ``PRAddrInfo`` structure, is not modified.
