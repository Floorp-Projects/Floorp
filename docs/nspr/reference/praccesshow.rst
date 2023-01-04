PRAccessHow
===========

This is the declaration for the enumeration :ref:`PRAccessHow`, used in the
``how`` parameter of :ref:`PR_Access`:

.. code::

   #include <prio.h>

   typedef enum PRAccessHow {
     PR_ACCESS_EXISTS = 1,
     PR_ACCESS_WRITE_OK = 2,
     PR_ACCESS_READ_OK = 3
   } PRAccessHow;

See `PR_Access <en/PR_Access>`__ for what each of these values mean.
