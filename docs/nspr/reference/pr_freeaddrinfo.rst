PR_FreeAddrInfo
===============


Destroys the ``PRAddrInfo`` structure returned by
:ref:`PR_GetAddrInfoByName`.


Syntax
------

.. code::

   #include <prnetdb.h>

   void PR_EnumerateAddrInfo(PRAddrInfo *addrInfo);


Parameters
~~~~~~~~~~

The function has the following parameters:

``addrInfo``
   A pointer to a ``PRAddrInfo`` structure returned by a successful call
   to :ref:`PR_GetAddrInfoByName`.


Returns
~~~~~~~

The function doesn't return anything.
