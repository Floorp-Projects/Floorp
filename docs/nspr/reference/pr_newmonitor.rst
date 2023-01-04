PR_NewMonitor
=============

Creates a new monitor object. The caller is responsible for the object
and is expected to destroy it when appropriate.


Syntax
------

.. code::

   #include <prmon.h>

   PRMonitor* PR_NewMonitor(void);


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, a pointer to a :ref:`PRMonitor` object.
-  If unsuccessful (for example, if some operating system resource is
   unavailable), ``NULL``.


Description
-----------

A newly created monitor has an entry count of zero.
