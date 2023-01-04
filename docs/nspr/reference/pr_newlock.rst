PR_NewLock
==========

Creates a new lock.


Syntax
------

.. code::

   #include <prlock.h>

   PRLock* PR_NewLock(void);


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, a pointer to the new lock object.
-  If unsuccessful (for example, the lock cannot be created because of
   resource constraints), ``NULL``.


Description
-----------

:ref:`PR_NewLock` creates a new opaque lock.
