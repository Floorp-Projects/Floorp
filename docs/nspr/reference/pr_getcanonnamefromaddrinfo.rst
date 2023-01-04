PR_GetCanonNameFromAddrInfo
===========================

Extracts the canonical name of the hostname passed to
:ref:`PR_GetAddrInfoByName`.


Syntax
------

.. code::

   #include <prnetdb.h>

   const char *PR_GetCanonNameFromAddrInfo(const PRAddrInfo *addrInfo);


Parameters
~~~~~~~~~~

The function has the following parameters:

``addrInfo``
   A pointer to a ``PRAddrInfo`` structure returned by a successful call
   to :ref:`PR_GetAddrInfoByName`.


Returns
~~~~~~~

The function returns a const pointer to the canonical hostname stored in
the given ``PRAddrInfo`` structure. This pointer is invalidated once the
``PRAddrInfo`` structure is destroyed by a call to :ref:`PR_FreeAddrInfo`.
