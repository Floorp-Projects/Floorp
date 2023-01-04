PRSeekWhence
============

Specifies how to interpret the ``offset`` parameter in setting the file
pointer associated with the ``fd`` parameter for the :ref:`PR_Seek` and
:ref:`PR_Seek64` functions.


Syntax
------

.. code::

   #include <prio.h>

   typedef PRSeekWhence {
     PR_SEEK_SET = 0,
     PR_SEEK_CUR = 1,
     PR_SEEK_END = 2
   } PRSeekWhence;


Enumerators
~~~~~~~~~~~

The enumeration has the following enumerators:

``PR_SEEK_SET``
   Sets the file pointer to the value of the ``offset`` parameter.
``PR_SEEK_CUR``
   Sets the file pointer to its current location plus the value of the
   ``offset`` parameter.
``PR_SEEK_END``
   Sets the file pointer to the size of the file plus the value of the
   ``offset`` parameter.
