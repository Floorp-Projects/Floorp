PR_ProcessExit
==============

Causes an immediate, nongraceful, forced termination of the process.


Syntax
------

.. code::

   #include <prinit.h>

   void PR_ProcessExit(PRIntn status);


Parameter
~~~~~~~~~

:ref:`PR_ProcessExit` has one parameter:

status
   The exit status code of the process.
