PR_Abort
========

Aborts the process in a nongraceful manner.


Syntax
------

.. code::

   #include <prinit.h>

   void PR_Abort(void);


Description
-----------

:ref:`PR_Abort` results in a core file and a call to the debugger or
equivalent, in addition to causing the entire process to stop.
