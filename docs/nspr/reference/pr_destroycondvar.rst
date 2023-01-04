PR_DestroyCondVar
=================

Destroys a condition variable.


Syntax
------

.. code::

   #include <prcvar.h>

   void PR_DestroyCondVar(PRCondVar *cvar);


Parameter
~~~~~~~~~

:ref:`PR_DestroyCondVar` has one parameter:

``cvar``
   A pointer to the condition variable object to be destroyed.


Description
-----------

Before calling :ref:`PR_DestroyCondVar`, the caller is responsible for
ensuring that the condition variable is no longer in use.
