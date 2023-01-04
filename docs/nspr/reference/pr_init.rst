PR_Init
=======

Initializes the runtime.


Syntax
------

.. code::

   #include <prinit.h>

   void PR_Init(
     PRThreadType type,
     PRThreadPriority priority,
     PRUintn maxPTDs);


Parameters
~~~~~~~~~~

:ref:`PR_Init` has the following parameters:

``type``
   This parameter is ignored.
``priority``
   This parameter is ignored.
``maxPTDs``
   This parameter is ignored.


Description
-----------

NSPR is now implicitly initialized, usually by the first NSPR function
called by a program. :ref:`PR_Init` is necessary only if a program has
specific initialization-sequencing requirements.

Call :ref:`PR_Init` as follows:

.. code::

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
